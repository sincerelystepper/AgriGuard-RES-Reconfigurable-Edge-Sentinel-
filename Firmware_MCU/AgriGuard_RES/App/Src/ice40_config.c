/**
 * @file    ice40_config.c
 * @brief   iCE40HX8K SPI Slave Configuration Engine
 *          AgriGuard-RES | Agrionics Systems Co.
 *
 * iCE40 SPI Slave Programming Sequence (per iCE40 Programming and Configuration
 * Technical Note TN1248):
 *
 *  T0: Assert CRESET_B LOW
 *  T1: Assert SS_B (CSN) LOW within 500ns of CRESET_B going LOW
 *  T2: Release CRESET_B HIGH (min 200µs LOW pulse)
 *  T3: Wait tCRESET2DONE_MAX (800 clock cycles on internal oscillator)
 *       → conservative delay: 1200µs @ worst-case 1MHz oscillator
 *  T4: Begin streaming bitstream MSB-first on SCLK rising edge (CPOL=0,CPHA=0)
 *  T5: CDONE asserts HIGH after last bitstream byte is latched
 *  T6: Issue ≥49 additional SCLK cycles (dummy 0xFF bytes) for user-mode entry
 *  T7: De-assert SS_B HIGH
 *
 * The bitstream size is stored as a 4-byte little-endian uint32 at the very
 * start of the NOR Flash (offset 0x00000000). The actual bitstream data
 * begins at offset 0x00000004. If bitstream_sz is passed as non-zero, the
 * stored length word is skipped.
 */

#include "ice40_config.h"
#include "flash_driver.h"
#include <string.h>

/* =========================================================================
 * PRIVATE HELPERS
 * ========================================================================= */

static inline void ice40_cs_assert(void)
{
    HAL_GPIO_WritePin(AGRD_PIN_ICE40_CSN_PORT,
                      AGRD_PIN_ICE40_CSN_PIN,
                      GPIO_PIN_RESET);
}

static inline void ice40_cs_deassert(void)
{
    HAL_GPIO_WritePin(AGRD_PIN_ICE40_CSN_PORT,
                      AGRD_PIN_ICE40_CSN_PIN,
                      GPIO_PIN_SET);
}

static inline void ice40_creset_assert(void)
{
    HAL_GPIO_WritePin(AGRD_PIN_ICE40_CRESET_PORT,
                      AGRD_PIN_ICE40_CRESET_PIN,
                      GPIO_PIN_RESET);
}

static inline void ice40_creset_deassert(void)
{
    HAL_GPIO_WritePin(AGRD_PIN_ICE40_CRESET_PORT,
                      AGRD_PIN_ICE40_CRESET_PIN,
                      GPIO_PIN_SET);
}

/**
 * @brief Microsecond-level delay using DWT cycle counter.
 *        Assumes SystemCoreClock is current.
 */
static void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000U);
    while ((DWT->CYCCNT - start) < ticks) {
        /* busy-wait — acceptable only during one-time FPGA config */
        __NOP();
    }
}

/* =========================================================================
 * PUBLIC API
 * ========================================================================= */

bool ICE40_IsCDONE(void)
{
    return (HAL_GPIO_ReadPin(AGRD_PIN_ICE40_CDONE_PORT,
                             AGRD_PIN_ICE40_CDONE_PIN) == GPIO_PIN_SET);
}

void ICE40_AssertReset(void)
{
    ice40_creset_assert();
}

AgrdStatus_t ICE40_Configure(SPI_HandleTypeDef *hspi_conf,
                             SPI_HandleTypeDef *hspi_flash,
                             uint32_t           bitstream_sz)
{
    AgrdStatus_t   rc;
    HAL_StatusTypeDef hal_rc;
    uint8_t        chunk_buf[ICE40_CONF_CHUNK_BYTES];
    uint32_t       bytes_sent   = 0U;
    uint32_t       flash_offset = 0U;
    uint32_t       t_start;

    /* ------------------------------------------------------------------
     * Step 0: If bitstream_sz == 0, read the 4-byte length from flash
     * ------------------------------------------------------------------ */
    if (bitstream_sz == 0U) {
        uint8_t len_bytes[4];
        rc = Flash_Read(hspi_flash, FLASH_BITSTREAM_BASE_ADDR, len_bytes, 4U);
        if (rc != AGRD_OK) {
            return AGRD_ERR_FLASH;
        }
        bitstream_sz = ((uint32_t)len_bytes[3] << 24U) |
                       ((uint32_t)len_bytes[2] << 16U) |
                       ((uint32_t)len_bytes[1] <<  8U) |
                       ((uint32_t)len_bytes[0]);
        flash_offset = 4U; /* bitstream data starts after the length word */
    }

    if ((bitstream_sz == 0U) || (bitstream_sz > FLASH_BITSTREAM_MAX_BYTES)) {
        return AGRD_ERR_PARAM;
    }

    /* ------------------------------------------------------------------
     * Step 1: Assert CRESET_B and CSN simultaneously
     * Per TN1248 §4.1: SS_B must go LOW within 500 ns of CRESET_B.
     * We do them back-to-back in the same function to meet timing.
     * ------------------------------------------------------------------ */
    ice40_creset_assert();
    ice40_cs_assert();

    /* ------------------------------------------------------------------
     * Step 2: Hold CRESET_B LOW for ≥200 µs
     * ------------------------------------------------------------------ */
    delay_us(ICE40_CRESET_PULSE_US);

    /* ------------------------------------------------------------------
     * Step 3: Release CRESET_B, keep CS asserted
     * Wait for internal oscillator startup: ≥1200µs conservative
     * ------------------------------------------------------------------ */
    ice40_creset_deassert();
    delay_us(1200U);

    /* ------------------------------------------------------------------
     * Step 4: Stream bitstream in ICE40_CONF_CHUNK_BYTES bursts
     * SPI2 must be configured CPOL=0, CPHA=0, MSB-first, 8-bit, ≤25MHz
     * ------------------------------------------------------------------ */
    while (bytes_sent < bitstream_sz) {
        uint16_t chunk_len = ICE40_CONF_CHUNK_BYTES;
        if ((bytes_sent + chunk_len) > bitstream_sz) {
            chunk_len = (uint16_t)(bitstream_sz - bytes_sent);
        }

        rc = Flash_ReadBitstreamChunk(hspi_flash, flash_offset, chunk_buf, chunk_len);
        if (rc != AGRD_OK) {
            ice40_cs_deassert();
            ice40_creset_assert(); /* Reset FPGA to known state on error */
            return AGRD_ERR_FLASH;
        }

        hal_rc = HAL_SPI_Transmit(hspi_conf, chunk_buf, chunk_len,
                                  ICE40_SPI_TIMEOUT_MS);
        if (hal_rc != HAL_OK) {
            ice40_cs_deassert();
            ice40_creset_assert();
            return AGRD_ERR_TIMEOUT;
        }

        bytes_sent   += chunk_len;
        flash_offset += chunk_len;
    }

    /* ------------------------------------------------------------------
     * Step 5: Poll CDONE HIGH — configuration latch complete
     * ------------------------------------------------------------------ */
    t_start = HAL_GetTick();
    while (!ICE40_IsCDONE()) {
        if ((HAL_GetTick() - t_start) > ICE40_CDONE_POLL_TIMEOUT_MS) {
            ice40_cs_deassert();
            ice40_creset_assert();
            return AGRD_ERR_FPGA_CONF;
        }
        HAL_Delay(1U);
    }

    /* ------------------------------------------------------------------
     * Step 6: Issue ≥49 dummy SCLK cycles (7 × 0xFF = 56 bits)
     * Required to release FPGA I/Os for user mode
     * ------------------------------------------------------------------ */
    static const uint8_t dummy_clocks[7] = {
        0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU
    };
    hal_rc = HAL_SPI_Transmit(hspi_conf, (uint8_t *)dummy_clocks,
                               sizeof(dummy_clocks), ICE40_SPI_TIMEOUT_MS);
    if (hal_rc != HAL_OK) {
        return AGRD_ERR_TIMEOUT;
    }

    /* ------------------------------------------------------------------
     * Step 7: De-assert SS_B — FPGA enters user mode
     * ------------------------------------------------------------------ */
    ice40_cs_deassert();

    return AGRD_OK;
}
