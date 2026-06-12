/**
 * @file    flash_driver.c
 * @brief   W25Q128JVS 128Mb SPI NOR Flash Driver Implementation
 *          AgriGuard-RES | Agrionics Systems Co.
 *
 * Design notes:
 *  - All operations are blocking with explicit HAL_GetTick() timeout guards.
 *  - CS is asserted/de-asserted manually around every SPI transaction to
 *    support the W25Q's command-framing model.
 *  - Write Enable (WREN) is issued explicitly before each write/erase.
 *  - No DMA used here — the iCE40 config engine calls Flash_Read directly;
 *    the host-side DMA would create unnecessary complexity on a shared bus.
 */

#include "flash_driver.h"
#include <string.h>

/* =========================================================================
 * INTERNAL HELPERS
 * ========================================================================= */

static inline void flash_cs_assert(void)
{
    HAL_GPIO_WritePin(AGRD_PIN_FLASH_CSN_PORT,
                      AGRD_PIN_FLASH_CSN_PIN,
                      GPIO_PIN_RESET);
}

static inline void flash_cs_deassert(void)
{
    HAL_GPIO_WritePin(AGRD_PIN_FLASH_CSN_PORT,
                      AGRD_PIN_FLASH_CSN_PIN,
                      GPIO_PIN_SET);
}

/**
 * @brief Send a single opcode byte (no data phase).
 */
static AgrdStatus_t flash_cmd_nodata(SPI_HandleTypeDef *hspi, uint8_t cmd)
{
    HAL_StatusTypeDef rc;
    flash_cs_assert();
    rc = HAL_SPI_Transmit(hspi, &cmd, 1U, FLASH_SPI_TIMEOUT_MS);
    flash_cs_deassert();
    return (rc == HAL_OK) ? AGRD_OK : AGRD_ERR_TIMEOUT;
}

/**
 * @brief Build a 4-byte command header: [opcode | addr[23:16] | addr[15:8] | addr[7:0]]
 */
static void flash_build_addr_cmd(uint8_t *buf, uint8_t opcode, uint32_t addr)
{
    buf[0] = opcode;
    buf[1] = (uint8_t)((addr >> 16U) & 0xFFU);
    buf[2] = (uint8_t)((addr >>  8U) & 0xFFU);
    buf[3] = (uint8_t)( addr         & 0xFFU);
}

/* =========================================================================
 * PUBLIC API IMPLEMENTATION
 * ========================================================================= */

AgrdStatus_t Flash_Init(SPI_HandleTypeDef *hspi)
{
    AgrdStatus_t rc;
    uint8_t jedec[3] = {0U};

    /* Ensure CS is de-asserted at startup */
    flash_cs_deassert();

    /* Release from power-down first (device may be in default PD state) */
    rc = Flash_WakeUp(hspi);
    if (rc != AGRD_OK) {
        return rc;
    }

    /* Verify JEDEC ID — Winbond W25Q128JVS = EF 40 18 */
    rc = Flash_ReadJEDECID(hspi, jedec);
    if (rc != AGRD_OK) {
        return rc;
    }

    if (jedec[0] != W25Q_JEDEC_MANUF_ID) {
        return AGRD_ERR_FLASH;
    }

    return AGRD_OK;
}

AgrdStatus_t Flash_WakeUp(SPI_HandleTypeDef *hspi)
{
    return flash_cmd_nodata(hspi, W25Q_CMD_RELEASE_POWER_DOWN);
}

void Flash_PowerDown(SPI_HandleTypeDef *hspi)
{
    (void)flash_cmd_nodata(hspi, W25Q_CMD_POWER_DOWN);
    /* tDP = 3µs minimum before chip enters power-down — caller handles delay */
}

AgrdStatus_t Flash_ReadJEDECID(SPI_HandleTypeDef *hspi, uint8_t id_buf[3])
{
    HAL_StatusTypeDef rc;
    uint8_t cmd = W25Q_CMD_READ_JEDEC_ID;

    flash_cs_assert();
    rc = HAL_SPI_Transmit(hspi, &cmd, 1U, FLASH_SPI_TIMEOUT_MS);
    if (rc == HAL_OK) {
        rc = HAL_SPI_Receive(hspi, id_buf, 3U, FLASH_SPI_TIMEOUT_MS);
    }
    flash_cs_deassert();

    return (rc == HAL_OK) ? AGRD_OK : AGRD_ERR_TIMEOUT;
}

AgrdStatus_t Flash_WaitReady(SPI_HandleTypeDef *hspi, uint32_t timeout_ms)
{
    HAL_StatusTypeDef hal_rc;
    uint8_t cmd    = W25Q_CMD_READ_STATUS1;
    uint8_t status = 0U;
    uint32_t t_start = HAL_GetTick();

    do {
        if ((HAL_GetTick() - t_start) > timeout_ms) {
            return AGRD_ERR_TIMEOUT;
        }
        flash_cs_assert();
        hal_rc = HAL_SPI_Transmit(hspi, &cmd, 1U, FLASH_SPI_TIMEOUT_MS);
        if (hal_rc == HAL_OK) {
            hal_rc = HAL_SPI_Receive(hspi, &status, 1U, FLASH_SPI_TIMEOUT_MS);
        }
        flash_cs_deassert();

        if (hal_rc != HAL_OK) {
            return AGRD_ERR_HAL;
        }
    } while (status & W25Q_STATUS1_BUSY_MASK);

    return AGRD_OK;
}

AgrdStatus_t Flash_Read(SPI_HandleTypeDef *hspi,
                        uint32_t addr,
                        uint8_t *buf,
                        uint32_t len)
{
    HAL_StatusTypeDef rc;
    /* Fast Read: opcode + 3-byte addr + 1 dummy byte */
    uint8_t hdr[5];

    if ((buf == NULL) || (len == 0U) || ((addr + len) > FLASH_TOTAL_BYTES)) {
        return AGRD_ERR_PARAM;
    }

    flash_build_addr_cmd(hdr, W25Q_CMD_FAST_READ, addr);
    hdr[4] = 0x00U; /* dummy byte */

    flash_cs_assert();
    rc = HAL_SPI_Transmit(hspi, hdr, 5U, FLASH_SPI_TIMEOUT_MS);
    if (rc == HAL_OK) {
        rc = HAL_SPI_Receive(hspi, buf, (uint16_t)len, FLASH_SPI_TIMEOUT_MS);
    }
    flash_cs_deassert();

    return (rc == HAL_OK) ? AGRD_OK : AGRD_ERR_TIMEOUT;
}

AgrdStatus_t Flash_WritePage(SPI_HandleTypeDef *hspi,
                             uint32_t addr,
                             const uint8_t *buf,
                             uint16_t len)
{
    HAL_StatusTypeDef rc;
    uint8_t wren = W25Q_CMD_WRITE_ENABLE;
    uint8_t hdr[4];
    AgrdStatus_t wait_rc;

    if ((buf == NULL) || (len == 0U) || (len > FLASH_PAGE_BYTES)) {
        return AGRD_ERR_PARAM;
    }

    /* Issue WREN */
    flash_cs_assert();
    rc = HAL_SPI_Transmit(hspi, &wren, 1U, FLASH_SPI_TIMEOUT_MS);
    flash_cs_deassert();
    if (rc != HAL_OK) {
        return AGRD_ERR_HAL;
    }

    /* Page Program command + address */
    flash_build_addr_cmd(hdr, W25Q_CMD_PAGE_PROGRAM, addr);

    flash_cs_assert();
    rc = HAL_SPI_Transmit(hspi, hdr, 4U, FLASH_SPI_TIMEOUT_MS);
    if (rc == HAL_OK) {
        rc = HAL_SPI_Transmit(hspi, (uint8_t *)buf, len, FLASH_SPI_TIMEOUT_MS);
    }
    flash_cs_deassert();

    if (rc != HAL_OK) {
        return AGRD_ERR_HAL;
    }

    /* Wait for page program to complete (tPP ≤ 3ms typical) */
    wait_rc = Flash_WaitReady(hspi, W25Q_WRITE_TIMEOUT_MS);
    return wait_rc;
}

AgrdStatus_t Flash_EraseSector(SPI_HandleTypeDef *hspi, uint32_t addr)
{
    HAL_StatusTypeDef rc;
    uint8_t wren = W25Q_CMD_WRITE_ENABLE;
    uint8_t hdr[4];
    AgrdStatus_t wait_rc;

    /* Align to sector boundary */
    addr &= ~((uint32_t)(FLASH_SECTOR_BYTES - 1U));

    /* Issue WREN */
    flash_cs_assert();
    rc = HAL_SPI_Transmit(hspi, &wren, 1U, FLASH_SPI_TIMEOUT_MS);
    flash_cs_deassert();
    if (rc != HAL_OK) {
        return AGRD_ERR_HAL;
    }

    flash_build_addr_cmd(hdr, W25Q_CMD_SECTOR_ERASE, addr);

    flash_cs_assert();
    rc = HAL_SPI_Transmit(hspi, hdr, 4U, FLASH_SPI_TIMEOUT_MS);
    flash_cs_deassert();

    if (rc != HAL_OK) {
        return AGRD_ERR_HAL;
    }

    /* tSE (sector erase) can be up to 400ms */
    wait_rc = Flash_WaitReady(hspi, W25Q_ERASE_TIMEOUT_MS);
    return wait_rc;
}

AgrdStatus_t Flash_ReadBitstreamChunk(SPI_HandleTypeDef *hspi,
                                      uint32_t offset,
                                      uint8_t *buf,
                                      uint16_t len)
{
    if (offset + len > FLASH_BITSTREAM_MAX_BYTES) {
        return AGRD_ERR_PARAM;
    }
    return Flash_Read(hspi, FLASH_BITSTREAM_BASE_ADDR + offset, buf, len);
}

AgrdStatus_t Flash_AppendTelemetry(SPI_HandleTypeDef *hspi,
                                   AgriSystemContext_t *ctx,
                                   const AgriTelemetryFrame_t *frame)
{
    AgrdStatus_t rc;
    uint32_t write_addr;

    if ((ctx == NULL) || (frame == NULL)) {
        return AGRD_ERR_PARAM;
    }

    /* Refuse writes when battery is critically low */
    if (ctx->flash_write_ptr == 0U) {
        ctx->flash_write_ptr = FLASH_TELEMETRY_BASE_ADDR;
    }

    write_addr = ctx->flash_write_ptr;

    /* Wrap around if we've exhausted the telemetry zone */
    if (write_addr + sizeof(AgriTelemetryFrame_t) >
        FLASH_TELEMETRY_BASE_ADDR + FLASH_TELEMETRY_BYTES)
    {
        write_addr = FLASH_TELEMETRY_BASE_ADDR;
    }

    /*
     * Erase a new sector every 128 frames (128 × 32B = 4096B = one sector).
     * Sector erase is needed only at the start of each sector boundary.
     */
    if ((write_addr % FLASH_SECTOR_BYTES) == 0U) {
        rc = Flash_EraseSector(hspi, write_addr);
        if (rc != AGRD_OK) {
            return rc;
        }
    }

    rc = Flash_WritePage(hspi, write_addr,
                         (const uint8_t *)frame,
                         (uint16_t)sizeof(AgriTelemetryFrame_t));
    if (rc != AGRD_OK) {
        return rc;
    }

    ctx->flash_write_ptr = write_addr + sizeof(AgriTelemetryFrame_t);
    return AGRD_OK;
}
