/**
 * @file    flash_driver.h
 * @brief   W25Q128JVS 128Mb Serial NOR Flash Driver Interface
 *          AgriGuard-RES | Agrionics Systems Co.
 *
 * Provides raw read/write/erase primitives and higher-level helpers for
 * bitstream block reads and telemetry frame appends. All operations are
 * blocking with explicit timeout guards via HAL tick.
 */

#ifndef FLASH_DRIVER_H
#define FLASH_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "agriguard_config.h"

/* =========================================================================
 * DRIVER INITIALIZATION
 * ========================================================================= */

/**
 * @brief  Initialize the flash SPI bus and verify JEDEC ID.
 * @retval AGRD_OK on success, AGRD_ERR_FLASH if JEDEC ID mismatch.
 */
AgrdStatus_t Flash_Init(SPI_HandleTypeDef *hspi);

/**
 * @brief  Release the flash from power-down mode (send 0xAB).
 * @retval AGRD_OK or AGRD_ERR_TIMEOUT.
 */
AgrdStatus_t Flash_WakeUp(SPI_HandleTypeDef *hspi);

/**
 * @brief  Send the flash into power-down mode (send 0xB9, < 1µA standby).
 */
void Flash_PowerDown(SPI_HandleTypeDef *hspi);

/* =========================================================================
 * CORE PRIMITIVES
 * ========================================================================= */

/**
 * @brief  Fast Read — burst-read arbitrary length from 24-bit address.
 * @param  addr     24-bit byte address (must be within device range).
 * @param  buf      Destination buffer.
 * @param  len      Number of bytes to read.
 * @retval AGRD_OK or AGRD_ERR_TIMEOUT / AGRD_ERR_PARAM.
 */
AgrdStatus_t Flash_Read(SPI_HandleTypeDef *hspi,
                        uint32_t addr,
                        uint8_t *buf,
                        uint32_t len);

/**
 * @brief  Page Program — write up to 256 bytes to an erased page.
 * @note   Caller must ensure the target page is erased.
 * @param  addr     Page-aligned 24-bit address.
 * @param  buf      Source data.
 * @param  len      Bytes to write (1–256).
 * @retval AGRD_OK or AGRD_ERR_TIMEOUT / AGRD_ERR_FLASH.
 */
AgrdStatus_t Flash_WritePage(SPI_HandleTypeDef *hspi,
                             uint32_t addr,
                             const uint8_t *buf,
                             uint16_t len);

/**
 * @brief  Sector Erase (4KB) — erase the 4KB sector containing addr.
 * @param  addr  Any byte address within the target sector.
 * @retval AGRD_OK or AGRD_ERR_TIMEOUT.
 */
AgrdStatus_t Flash_EraseSector(SPI_HandleTypeDef *hspi, uint32_t addr);

/* =========================================================================
 * HIGHER-LEVEL HELPERS
 * ========================================================================= */

/**
 * @brief  Stream-read a contiguous chunk for FPGA bitstream loading.
 *         Intended to be called in a loop by the iCE40 config engine.
 * @param  offset   Byte offset from FLASH_BITSTREAM_BASE_ADDR.
 * @param  buf      Destination buffer of exactly len bytes.
 * @param  len      Chunk size (FPGA_CONF_CHUNK_BYTES recommended).
 * @retval AGRD_OK or error code.
 */
AgrdStatus_t Flash_ReadBitstreamChunk(SPI_HandleTypeDef *hspi,
                                      uint32_t offset,
                                      uint8_t *buf,
                                      uint16_t len);

/**
 * @brief  Append a packed telemetry frame to the NOR ring buffer.
 * @note   Automatically erases a fresh sector every 128 frames (4KB / 32B).
 * @param  ctx   Pointer to the system runtime context.
 * @param  frame Pointer to the completed, CRC-populated frame.
 * @retval AGRD_OK or AGRD_ERR_FLASH / AGRD_ERR_POWER (on critical VBAT).
 */
AgrdStatus_t Flash_AppendTelemetry(SPI_HandleTypeDef *hspi,
                                   AgriSystemContext_t *ctx,
                                   const AgriTelemetryFrame_t *frame);

/* =========================================================================
 * UTILITIES
 * ========================================================================= */

/**
 * @brief  Poll WIP (Write In Progress) bit until clear or timeout.
 * @param  timeout_ms  Maximum wait in milliseconds.
 * @retval AGRD_OK or AGRD_ERR_TIMEOUT.
 */
AgrdStatus_t Flash_WaitReady(SPI_HandleTypeDef *hspi, uint32_t timeout_ms);

/**
 * @brief  Read 3-byte JEDEC ID into id_buf[3].
 */
AgrdStatus_t Flash_ReadJEDECID(SPI_HandleTypeDef *hspi, uint8_t id_buf[3]);

#ifdef __cplusplus
}
#endif

#endif /* FLASH_DRIVER_H */
