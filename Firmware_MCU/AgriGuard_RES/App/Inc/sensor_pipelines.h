/**
 * @file    sensor_pipelines.h
 * @brief   Sensor Pipeline Interface — AgriGuard-RES
 *          Agrionics Systems Co.
 *
 * Pipelines:
 *   B. Spectral Crop   — AS7261 6-channel NIR via I2C1 (isolated bus)
 *   C. Soil Intel      — 5-param Modbus RTU RS-485 via USART3 + MAX3485E
 *   D. Microclimate    — BME680 Temp/RH/Press/Gas via I2C1 (shared bus)
 *
 * Pipeline A (Acoustic/FFT) runs entirely in FPGA hardware; the MCU only
 * reads the FPGA result register via SPI on anomaly interrupt.
 */

#ifndef SENSOR_PIPELINES_H
#define SENSOR_PIPELINES_H

#ifdef __cplusplus
extern "C" {
#endif

#include "agriguard_config.h"

/* =========================================================================
 * RAW SENSOR RESULT TYPES
 * ========================================================================= */

/** BME680 compensated output */
typedef struct {
    int32_t  temperature_cdeg;  /* ×100 — e.g. 2350 = 23.50 °C          */
    uint32_t pressure_pa;       /* Absolute pressure in Pa               */
    uint32_t humidity_cpct;     /* ×100 — e.g. 6543 = 65.43 %RH         */
    uint32_t gas_resistance_ohm;
    bool     new_data_valid;
} BME680_Data_t;

/** AS7261 6-channel spectral output (raw 16-bit ADC counts) */
typedef struct {
    uint16_t ch_610nm;   /* Orange-red / vegetation stress proxy        */
    uint16_t ch_680nm;   /* Red — chlorophyll absorption                 */
    uint16_t ch_730nm;   /* Far-red — plant stress transition            */
    uint16_t ch_760nm;   /* NIR — healthy leaf reflectance               */
    uint16_t ch_810nm;   /* NIR plateau                                  */
    uint16_t ch_860nm;   /* NIR reference                                */
    int8_t   device_temp_deg;
} AS7261_Data_t;

/** 5-parameter soil sensor Modbus RTU response */
typedef struct {
    uint16_t moisture_cpct;   /* ×10 — e.g. 325 = 32.5% VWC             */
    int16_t  temperature_cdeg;
    uint16_t ec_us_cm;        /* Electrical conductivity                  */
    uint16_t ph_x10;          /* pH × 10 — e.g. 68 = pH 6.8              */
    uint16_t nitrogen_mg_kg;
} Soil_Data_t;

/* =========================================================================
 * BME680 PIPELINE (Microclimate)
 * ========================================================================= */

AgrdStatus_t BME680_Init(I2C_HandleTypeDef *hi2c);
AgrdStatus_t BME680_TriggerForcedMode(I2C_HandleTypeDef *hi2c);
AgrdStatus_t BME680_ReadResults(I2C_HandleTypeDef *hi2c, BME680_Data_t *out);

/* =========================================================================
 * AS7261 PIPELINE (Spectral Crop)
 * ========================================================================= */

AgrdStatus_t AS7261_Init(I2C_HandleTypeDef *hi2c);

/**
 * @brief  Trigger a single-shot integration and block until complete.
 *         Typical integration time = AS7261_INTEGRATION_MS ms.
 */
AgrdStatus_t AS7261_ReadSpectral(I2C_HandleTypeDef *hi2c, AS7261_Data_t *out);

/**
 * @brief  Compute a simple NDVI-proxy from AS7261 channels.
 *         NDVI_proxy = (ch_760nm - ch_680nm) / (ch_760nm + ch_680nm) × 1000
 * @retval Scaled integer [0..1000], or UINT16_MAX on divide-by-zero.
 */
uint16_t AS7261_ComputeNDVI(const AS7261_Data_t *spectral);

/* =========================================================================
 * SOIL PIPELINE (Modbus RTU over RS-485)
 * ========================================================================= */

/**
 * @brief  Initialize USART3 for 9600-8-N-1 Modbus and set DE/RE GPIO low.
 */
AgrdStatus_t Soil_Init(UART_HandleTypeDef *huart);

/**
 * @brief  Perform a complete Modbus RTU Read Holding Registers transaction.
 *         Drives DE pin HIGH for TX, waits for full response, drives LOW.
 */
AgrdStatus_t Soil_ReadParameters(UART_HandleTypeDef *huart, Soil_Data_t *out);

/* =========================================================================
 * CRC-16/CCITT (IBM) UTILITY
 * Used for both Modbus RTU frame validation and AgriTelemetryFrame_t signing.
 * ========================================================================= */

/**
 * @brief  Compute CRC-16 (poly=0x8005, init=0xFFFF, input/output NOT reflected).
 *         This matches the Modbus RTU CRC-16 specification exactly.
 */
uint16_t CRC16_Modbus(const uint8_t *data, uint16_t len);

/* =========================================================================
 * COMBINED PIPELINE RUNNER
 * ========================================================================= */

/**
 * @brief  Execute all four sensor pipelines sequentially and pack results
 *         into a telemetry frame (without transmitting). Populates all
 *         fields except crc16 and fpga_anomaly (set by caller from FPGA).
 *
 * @param  hi2c   Shared I2C1 handle.
 * @param  huart  USART3 handle for RS-485.
 * @param  ctx    System context (increments sequence_counter).
 * @param  frame  Output telemetry frame (caller provides pre-zeroed struct).
 * @retval AGRD_OK if all pipelines succeeded.
 *         Returns first encountered error code; frame may be partially filled.
 */
AgrdStatus_t Pipelines_RunAll(I2C_HandleTypeDef    *hi2c,
                              UART_HandleTypeDef   *huart,
                              AgriSystemContext_t  *ctx,
                              AgriTelemetryFrame_t *frame);

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_PIPELINES_H */
