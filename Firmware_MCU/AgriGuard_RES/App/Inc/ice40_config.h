/**
 * @file    ice40_config.h
 * @brief   iCE40HX8K SPI Slave Configuration Engine Interface
 *          AgriGuard-RES | Agrionics Systems Co.
 *
 * Implements the complete iCE40 SPI Slave Programming Mode sequence:
 *   1. CRESET_B assertion (reset)
 *   2. SS_B (CSN) de-assertion for slave mode selection
 *   3. CRESET_B de-assertion + tCRESET guard delay
 *   4. Chunked DMA bitstream transfer from NOR Flash via SPI2
 *   5. CDONE polling until HIGH (configuration complete)
 *   6. 49+ dummy clock cycles to enter user mode
 */

#ifndef ICE40_CONFIG_H
#define ICE40_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "agriguard_config.h"
#include "flash_driver.h"

/* =========================================================================
 * CONFIGURATION ENGINE
 * ========================================================================= */

/**
 * @brief  Execute the full iCE40 SPI slave configuration sequence.
 *
 * Reads the bitstream from NOR Flash in ICE40_CONF_CHUNK_BYTES chunks and
 * clocks them out on SPI2 (CPOL=0, CPHA=0, MSB-first, max 25 MHz).
 * Monitors CDONE to completion and issues 49 dummy clocks for user-mode entry.
 *
 * @param  hspi_conf    SPI handle bound to the iCE40 programming bus (SPI2).
 * @param  hspi_flash   SPI handle for the NOR Flash bus (SPI3).
 * @param  bitstream_sz Actual bitstream size in bytes (read from flash header
 *                      or predetermined at build time). Pass 0 to auto-detect
 *                      from a stored 4-byte little-endian length at offset 0.
 * @retval AGRD_OK            — CDONE asserted, FPGA in user mode.
 *         AGRD_ERR_FPGA_CONF — CDONE not asserted within timeout.
 *         AGRD_ERR_FLASH     — Flash read error during streaming.
 *         AGRD_ERR_TIMEOUT   — CRESET or SPI operation timeout.
 */
AgrdStatus_t ICE40_Configure(SPI_HandleTypeDef *hspi_conf,
                             SPI_HandleTypeDef *hspi_flash,
                             uint32_t           bitstream_sz);

/**
 * @brief  Assert CRESET_B to force the FPGA into unconfigured state.
 *         Call before power-gating the FPGA 1.2V rail.
 */
void ICE40_AssertReset(void);

/**
 * @brief  Sample the CDONE line (HIGH = configured).
 * @retval true  if FPGA is configured and in user mode.
 *         false otherwise.
 */
bool ICE40_IsCDONE(void);

#ifdef __cplusplus
}
#endif

#endif /* ICE40_CONFIG_H */
