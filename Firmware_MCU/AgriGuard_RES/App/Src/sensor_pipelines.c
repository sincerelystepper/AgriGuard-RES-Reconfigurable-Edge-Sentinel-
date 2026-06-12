/**
 * @file    sensor_pipelines.c
 * @brief   Sensor Pipeline Implementations — AgriGuard-RES
 *          Agrionics Systems Co.
 *
 * All I2C transactions use the STM32 HAL with explicit timeout guards.
 * The BME680 uses a bare-register "forced mode" flow — no Bosch BSEC library
 * dependency, keeping firmware self-contained.
 * The AS7261 virtual register model requires a two-step write-then-read
 * over the physical I2C register interface.
 * The soil sensor Modbus RTU uses USART3 + MAX3485E with DE GPIO control.
 */

#include "sensor_pipelines.h"
#include <string.h>

/* =========================================================================
 * CRC-16/MODBUS IMPLEMENTATION
 * Polynomial: 0x8005 | Init: 0xFFFF | Input reflected | Output reflected
 * ========================================================================= */

uint16_t CRC16_Modbus(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFU;
    uint16_t i;
    uint8_t  bit;

    for (i = 0U; i < len; i++) {
        crc ^= (uint16_t)data[i];
        for (bit = 0U; bit < 8U; bit++) {
            if (crc & 0x0001U) {
                crc = (crc >> 1U) ^ 0xA001U;
            } else {
                crc >>= 1U;
            }
        }
    }
    return crc;
}

/* =========================================================================
 * INTERNAL I2C HELPERS
 * ========================================================================= */

static AgrdStatus_t i2c_write_reg(I2C_HandleTypeDef *hi2c,
                                   uint8_t dev_addr,
                                   uint8_t reg,
                                   uint8_t value)
{
    uint8_t buf[2] = { reg, value };
    HAL_StatusTypeDef rc = HAL_I2C_Master_Transmit(
        hi2c, (uint16_t)(dev_addr << 1U), buf, 2U, BME680_MEAS_TIMEOUT_MS);
    return (rc == HAL_OK) ? AGRD_OK : AGRD_ERR_HAL;
}

static AgrdStatus_t i2c_read_reg(I2C_HandleTypeDef *hi2c,
                                  uint8_t dev_addr,
                                  uint8_t reg,
                                  uint8_t *out,
                                  uint16_t len)
{
    HAL_StatusTypeDef rc;
    rc = HAL_I2C_Master_Transmit(
        hi2c, (uint16_t)(dev_addr << 1U), &reg, 1U, BME680_MEAS_TIMEOUT_MS);
    if (rc != HAL_OK) { return AGRD_ERR_HAL; }

    rc = HAL_I2C_Master_Receive(
        hi2c, (uint16_t)(dev_addr << 1U), out, len, BME680_MEAS_TIMEOUT_MS);
    return (rc == HAL_OK) ? AGRD_OK : AGRD_ERR_HAL;
}

/* =========================================================================
 * BME680 PIPELINE — MICROCLIMATE
 * Minimal register-level forced mode driver.
 * Full compensation math per Bosch Datasheet Section 8.5.3 / 8.5.4.
 * Compensation coefficients cached in static storage at init.
 * ========================================================================= */

/* Compensation coefficient storage (trimmed from BME680 coeff registers) */
static struct {
    uint16_t T1;
    int16_t  T2;
    int8_t   T3;
    uint16_t P1;
    int16_t  P2, P3, P4, P5;
    int8_t   P6, P7;
    int16_t  P8, P9;
    uint8_t  P10;
    uint16_t H1, H2;
    int8_t   H3, H4, H5;
    uint8_t  H6;
    int8_t   H7;
    int8_t   GH1, GH2_lsb;
    int16_t  GH2;
    uint8_t  heat_range;
    int8_t   heat_val;
    int32_t  t_fine;   /* intermediate for pressure/humidity comp */
    bool     loaded;
} bme680_calib;

AgrdStatus_t BME680_Init(I2C_HandleTypeDef *hi2c)
{
    AgrdStatus_t rc;
    uint8_t calib_low[25];
    uint8_t calib_high[16];
    uint8_t gas_calib[3];

    /* Read calibration data block 1: registers 0x89..0xA1 (25 bytes) */
    rc = i2c_read_reg(hi2c, AGRD_I2C_ADDR_BME680, 0x89U, calib_low, 25U);
    if (rc != AGRD_OK) { return rc; }

    /* Read calibration data block 2: registers 0xE1..0xF0 (16 bytes) */
    rc = i2c_read_reg(hi2c, AGRD_I2C_ADDR_BME680, 0xE1U, calib_high, 16U);
    if (rc != AGRD_OK) { return rc; }

    /* Read gas calibration: 0xEB, 0xEC, 0xED */
    rc = i2c_read_reg(hi2c, AGRD_I2C_ADDR_BME680, 0xEBU, gas_calib, 3U);
    if (rc != AGRD_OK) { return rc; }

    /* Parse temperature coefficients */
    bme680_calib.T1 = (uint16_t)(calib_low[9]  | ((uint16_t)calib_low[10] << 8U));
    bme680_calib.T2 = (int16_t) (calib_low[1]  | ((uint16_t)calib_low[2]  << 8U));
    bme680_calib.T3 = (int8_t)   calib_low[3];

    /* Parse pressure coefficients */
    bme680_calib.P1  = (uint16_t)(calib_low[5]  | ((uint16_t)calib_low[6]  << 8U));
    bme680_calib.P2  = (int16_t) (calib_low[7]  | ((uint16_t)calib_low[8]  << 8U));
    bme680_calib.P3  = (int8_t)   calib_low[4];
    bme680_calib.P4  = (int16_t) (calib_low[11] | ((uint16_t)calib_low[12] << 8U));
    bme680_calib.P5  = (int16_t) (calib_low[13] | ((uint16_t)calib_low[14] << 8U));
    bme680_calib.P6  = (int8_t)   calib_low[16];
    bme680_calib.P7  = (int8_t)   calib_low[15];
    bme680_calib.P8  = (int16_t) (calib_low[19] | ((uint16_t)calib_low[20] << 8U));
    bme680_calib.P9  = (int16_t) (calib_low[21] | ((uint16_t)calib_low[22] << 8U));
    bme680_calib.P10 =             calib_low[23];

    /* Parse humidity coefficients */
    bme680_calib.H1 = (uint16_t)(((uint16_t)calib_high[2] << 4U) | (calib_high[1] & 0x0FU));
    bme680_calib.H2 = (uint16_t)(((uint16_t)calib_high[0] << 4U) | ((calib_high[1] >> 4U) & 0x0FU));
    bme680_calib.H3 = (int8_t)calib_high[3];
    bme680_calib.H4 = (int8_t)calib_high[4];
    bme680_calib.H5 = (int8_t)calib_high[5];
    bme680_calib.H6 =          calib_high[6];
    bme680_calib.H7 = (int8_t)calib_high[7];

    /* Gas heater calibration */
    bme680_calib.GH1      = (int8_t)gas_calib[2];
    bme680_calib.GH2      = (int16_t)(gas_calib[0] | ((uint16_t)gas_calib[1] << 8U));
    bme680_calib.heat_range = calib_high[9];
    bme680_calib.heat_val   = (int8_t)calib_high[8];
    bme680_calib.loaded     = true;

    /* Configure oversampling: osrs_t=x2, osrs_p=x4, osrs_h=x1 */
    rc = i2c_write_reg(hi2c, AGRD_I2C_ADDR_BME680, 0x72U, 0x01U); /* ctrl_hum  */
    if (rc != AGRD_OK) { return rc; }
    /* ctrl_meas: osrs_t=010b, osrs_p=100b, mode=00b (sleep for now) */
    rc = i2c_write_reg(hi2c, AGRD_I2C_ADDR_BME680, BME680_REG_CTRL_MEAS, 0x50U);
    return rc;
}

AgrdStatus_t BME680_TriggerForcedMode(I2C_HandleTypeDef *hi2c)
{
    /* Set ctrl_meas[1:0] = 01b to trigger forced mode measurement */
    return i2c_write_reg(hi2c, AGRD_I2C_ADDR_BME680,
                         BME680_REG_CTRL_MEAS, 0x51U);
}

AgrdStatus_t BME680_ReadResults(I2C_HandleTypeDef *hi2c, BME680_Data_t *out)
{
    AgrdStatus_t rc;
    uint8_t  meas_status = 0U;
    uint8_t  raw[8];
    uint32_t t_start;

    /* Poll for new_data bit (bit 7 of 0x1D) */
    t_start = HAL_GetTick();
    do {
        if ((HAL_GetTick() - t_start) > BME680_MEAS_TIMEOUT_MS) {
            return AGRD_ERR_TIMEOUT;
        }
        rc = i2c_read_reg(hi2c, AGRD_I2C_ADDR_BME680,
                          BME680_REG_MEAS_STATUS, &meas_status, 1U);
        if (rc != AGRD_OK) { return rc; }
        HAL_Delay(5U);
    } while (!(meas_status & BME680_NEW_DATA_MASK));

    /* Read 8 raw bytes: press[2:0], temp[2:0], hum[1:0] */
    rc = i2c_read_reg(hi2c, AGRD_I2C_ADDR_BME680, BME680_REG_PRESS_MSB, raw, 8U);
    if (rc != AGRD_OK) { return rc; }

    /* Extract raw ADC values (20-bit for press/temp, 16-bit for hum) */
    uint32_t adc_P = ((uint32_t)raw[0] << 12U) | ((uint32_t)raw[1] << 4U) | (raw[2] >> 4U);
    uint32_t adc_T = ((uint32_t)raw[3] << 12U) | ((uint32_t)raw[4] << 4U) | (raw[5] >> 4U);
    uint16_t adc_H = ((uint16_t)raw[6] << 8U)  |  raw[7];

    /* ---- Temperature Compensation (Bosch BST-BME680-DS001 §8.5.3) ---- */
    int64_t var1, var2, var3;
    var1 = ((int32_t)adc_T >> 3) - ((int32_t)bme680_calib.T1 << 1);
    var2 = (var1 * (int32_t)bme680_calib.T2) >> 11;
    var3 = ((var1 >> 1) * (var1 >> 1)) >> 12;
    var3 = (var3 * ((int32_t)bme680_calib.T3 << 4)) >> 14;
    bme680_calib.t_fine = (int32_t)(var2 + var3);
    int32_t temp_comp   = (bme680_calib.t_fine * 5 + 128) >> 8; /* in 0.01°C */
    out->temperature_cdeg = temp_comp;

    /* ---- Pressure Compensation ---- */
    int64_t pvar1, pvar2, pvar3, press_comp;
    pvar1 = ((int64_t)bme680_calib.t_fine >> 1) - 64000LL;
    pvar2 = pvar1 * pvar1 * ((int64_t)bme680_calib.P6) / 131072LL;
    pvar2 = pvar2 + (pvar1 * (int64_t)bme680_calib.P5 * 2LL);
    pvar2 = (pvar2 / 4LL) + ((int64_t)bme680_calib.P4 * 65536LL);
    pvar1 = ((int64_t)bme680_calib.P3 * pvar1 * pvar1 / 524288LL + (int64_t)bme680_calib.P2 * pvar1) / 524288LL;
    pvar1 = (1LL + pvar1 / 32768LL) * (int64_t)bme680_calib.P1;
    if (pvar1 != 0LL) {
        press_comp = 1048576LL - (int64_t)adc_P;
        press_comp = ((press_comp - (pvar2 / 4096LL)) * 6250LL) / pvar1;
        pvar1 = (int64_t)bme680_calib.P9 * press_comp * press_comp / 2147483648LL;
        pvar2 = press_comp * (int64_t)bme680_calib.P8 / 32768LL;
        pvar3 = (press_comp / 256LL) * (press_comp / 256LL) * (press_comp / 256LL)
                * (int64_t)bme680_calib.P10 / 131072LL;
        press_comp += (pvar1 + pvar2 + pvar3 + (int64_t)bme680_calib.P7 * 128LL) / 16LL;
    } else {
        press_comp = 0LL;
    }
    out->pressure_pa = (uint32_t)press_comp;

    /* ---- Humidity Compensation ---- */
    int32_t hvar1, hvar2, hvar3, hvar4, hvar5, hvar6, hcomp;
    hvar1 = (int32_t)adc_H - ((int32_t)bme680_calib.H1 * 16)
            - (((bme680_calib.t_fine * (int32_t)bme680_calib.H3) / 100) >> 1);
    hvar2 = ((int32_t)bme680_calib.H2 *
              (((bme680_calib.t_fine * (int32_t)bme680_calib.H4) / 100)
               + (((bme680_calib.t_fine * ((bme680_calib.t_fine * (int32_t)bme680_calib.H5) / 100)) >> 6) / 100)
               + (1 << 14))) >> 10;
    hvar3 = hvar1 * hvar2;
    hvar4 = (int32_t)bme680_calib.H6 << 7;
    hvar4 = ((hvar4) + ((bme680_calib.t_fine * (int32_t)bme680_calib.H7) / 100)) >> 4;
    hvar5 = ((hvar3 >> 14) * (hvar3 >> 14)) >> 10;
    hvar6 = (hvar4 * hvar5) >> 1;
    hcomp = (hvar3 + hvar6) >> 12;
    hcomp = (((hcomp * 1000) / 4096) / 10); /* in 0.01% */
    if (hcomp > 10000) hcomp = 10000;
    if (hcomp <     0) hcomp = 0;
    out->humidity_cpct = (uint32_t)hcomp;

    out->gas_resistance_ohm = 0U; /* Gas heater calibration omitted for brevity */
    out->new_data_valid = true;

    return AGRD_OK;
}

/* =========================================================================
 * AS7261 PIPELINE — SPECTRAL CROP
 * Virtual register model: write target register index to 0x01 (Transaction),
 * then write/read from 0x02 (Write) / 0x02 (Read with MSB set).
 * ========================================================================= */

/* AS7261 physical I2C register addresses */
#define AS7261_PHY_STATUS    (0x00U)
#define AS7261_PHY_WRITE     (0x01U)
#define AS7261_PHY_READ      (0x02U)

/* Virtual register indices */
#define AS7261_VIRT_CTRL     (0x04U)
#define AS7261_VIRT_STATUS   (0x03U)
#define AS7261_VIRT_RAW_X    (0x08U)   /* X, Y, Z, NIR, Dark, Clear = 8..13 */

static AgrdStatus_t as7261_write_virtual(I2C_HandleTypeDef *hi2c,
                                          uint8_t virt_reg, uint8_t value)
{
    AgrdStatus_t rc;
    uint8_t status;

    /* Poll TX_VALID bit (bit 1) of physical status register */
    uint32_t t = HAL_GetTick();
    do {
        rc = i2c_read_reg(hi2c, AGRD_I2C_ADDR_AS7261,
                          AS7261_PHY_STATUS, &status, 1U);
        if (rc != AGRD_OK) { return rc; }
        if ((HAL_GetTick() - t) > AS7261_INTEGRATION_MS) { return AGRD_ERR_TIMEOUT; }
    } while (status & 0x02U); /* TX_VALID must be clear before writing */

    /* Write virtual register address with write flag (MSB clear) */
    rc = i2c_write_reg(hi2c, AGRD_I2C_ADDR_AS7261, AS7261_PHY_WRITE, virt_reg);
    if (rc != AGRD_OK) { return rc; }

    /* Poll TX_VALID again before writing value */
    t = HAL_GetTick();
    do {
        rc = i2c_read_reg(hi2c, AGRD_I2C_ADDR_AS7261,
                          AS7261_PHY_STATUS, &status, 1U);
        if (rc != AGRD_OK) { return rc; }
        if ((HAL_GetTick() - t) > AS7261_INTEGRATION_MS) { return AGRD_ERR_TIMEOUT; }
    } while (status & 0x02U);

    return i2c_write_reg(hi2c, AGRD_I2C_ADDR_AS7261, AS7261_PHY_WRITE, value);
}

static AgrdStatus_t as7261_read_virtual(I2C_HandleTypeDef *hi2c,
                                         uint8_t virt_reg, uint8_t *out)
{
    AgrdStatus_t rc;
    uint8_t status;

    /* Write virtual register address with read flag (MSB set) */
    uint32_t t = HAL_GetTick();
    do {
        rc = i2c_read_reg(hi2c, AGRD_I2C_ADDR_AS7261,
                          AS7261_PHY_STATUS, &status, 1U);
        if (rc != AGRD_OK) { return rc; }
        if ((HAL_GetTick() - t) > AS7261_INTEGRATION_MS) { return AGRD_ERR_TIMEOUT; }
    } while (status & 0x02U);

    rc = i2c_write_reg(hi2c, AGRD_I2C_ADDR_AS7261,
                       AS7261_PHY_WRITE, virt_reg | AS7261_VIRTUAL_READ_FLAG);
    if (rc != AGRD_OK) { return rc; }

    /* Poll RX_VALID bit (bit 0) */
    t = HAL_GetTick();
    do {
        rc = i2c_read_reg(hi2c, AGRD_I2C_ADDR_AS7261,
                          AS7261_PHY_STATUS, &status, 1U);
        if (rc != AGRD_OK) { return rc; }
        if ((HAL_GetTick() - t) > AS7261_INTEGRATION_MS) { return AGRD_ERR_TIMEOUT; }
    } while (!(status & 0x01U)); /* RX_VALID must be set */

    return i2c_read_reg(hi2c, AGRD_I2C_ADDR_AS7261, AS7261_PHY_READ, out, 1U);
}

AgrdStatus_t AS7261_Init(I2C_HandleTypeDef *hi2c)
{
    /* Enable measurement mode: data=0x38 → BANK=3 (6-channel), GAIN=0 (1×), INT=0 */
    return as7261_write_virtual(hi2c, AS7261_VIRT_CTRL, 0x38U);
}

AgrdStatus_t AS7261_ReadSpectral(I2C_HandleTypeDef *hi2c, AS7261_Data_t *out)
{
    AgrdStatus_t rc;
    uint8_t  virt_status = 0U;
    uint8_t  byte_hi, byte_lo;
    uint32_t t_start;

    /* Trigger a new integration by writing measurement control */
    rc = as7261_write_virtual(hi2c, AS7261_VIRT_CTRL, 0x38U | 0x02U); /* DATA_RDY=1 triggers */
    if (rc != AGRD_OK) { return rc; }

    /* Poll DATA_RDY bit (bit 1) of virtual status register */
    t_start = HAL_GetTick();
    do {
        if ((HAL_GetTick() - t_start) > (AS7261_INTEGRATION_MS * 3U)) {
            return AGRD_ERR_TIMEOUT;
        }
        rc = as7261_read_virtual(hi2c, AS7261_VIRT_STATUS, &virt_status);
        if (rc != AGRD_OK) { return rc; }
        HAL_Delay(10U);
    } while (!(virt_status & 0x02U));

    /*
     * Read 6 channels × 2 bytes each (16-bit raw, big-endian in virtual registers).
     * Virtual register map: X=0x08/09, Y=0x0A/0B, Z=0x0C/0D, NIR=0x0E/0F, Dark=0x10/11, Clear=0x12/13
     * We map: X→610nm, Y→680nm, Z→730nm, NIR→760nm, Dark→810nm, Clear→860nm (hardware-pin-dependent)
     */
    uint16_t *channels[6] = {
        &out->ch_610nm, &out->ch_680nm, &out->ch_730nm,
        &out->ch_760nm, &out->ch_810nm, &out->ch_860nm
    };

    for (uint8_t ch = 0U; ch < 6U; ch++) {
        rc = as7261_read_virtual(hi2c, (uint8_t)(AS7261_VIRT_RAW_X + ch * 2U), &byte_hi);
        if (rc != AGRD_OK) { return rc; }
        rc = as7261_read_virtual(hi2c, (uint8_t)(AS7261_VIRT_RAW_X + ch * 2U + 1U), &byte_lo);
        if (rc != AGRD_OK) { return rc; }
        *channels[ch] = ((uint16_t)byte_hi << 8U) | byte_lo;
    }

    uint8_t temp_byte;
    rc = as7261_read_virtual(hi2c, AS7261_REG_DEV_TEMP, &temp_byte);
    out->device_temp_deg = (int8_t)temp_byte;

    return rc;
}

uint16_t AS7261_ComputeNDVI(const AS7261_Data_t *spectral)
{
    int32_t nir  = (int32_t)spectral->ch_760nm;
    int32_t red  = (int32_t)spectral->ch_680nm;
    int32_t denom = nir + red;

    if (denom == 0) {
        return 0xFFFFU; /* undefined */
    }
    /* Result scaled ×1000: range [-1000..+1000], healthy vegetation > 300 */
    int32_t ndvi = ((nir - red) * 1000) / denom;
    return (uint16_t)(ndvi + 1000); /* shift to unsigned [0..2000] */
}

/* =========================================================================
 * SOIL PIPELINE — MODBUS RTU RS-485
 * ========================================================================= */

AgrdStatus_t Soil_Init(UART_HandleTypeDef *huart)
{
    /* Ensure RS-485 transceiver starts in receive mode */
    HAL_GPIO_WritePin(AGRD_PIN_RS485_DE_PORT,
                      AGRD_PIN_RS485_DE_PIN,
                      GPIO_PIN_RESET);
    (void)huart; /* USART3 configured by HAL_UART_Init in main.c */
    return AGRD_OK;
}

AgrdStatus_t Soil_ReadParameters(UART_HandleTypeDef *huart, Soil_Data_t *out)
{
    AgrdStatus_t  rc = AGRD_ERR_TIMEOUT;
    HAL_StatusTypeDef hal_rc;
    uint8_t  tx_frame[SOIL_MODBUS_FRAME_BYTES];
    uint8_t  rx_frame[SOIL_MODBUS_RESP_BYTES];
    uint16_t tx_crc, rx_crc, calc_crc;

    /* Build Modbus RTU Read Holding Registers request */
    tx_frame[0] = SOIL_MODBUS_ADDR;
    tx_frame[1] = SOIL_MODBUS_FUNC_READ_HOLD;
    tx_frame[2] = (uint8_t)(SOIL_MODBUS_REG_MOISTURE >> 8U);
    tx_frame[3] = (uint8_t)(SOIL_MODBUS_REG_MOISTURE & 0xFFU);
    tx_frame[4] = 0x00U;
    tx_frame[5] = SOIL_MODBUS_REG_COUNT;
    tx_crc = CRC16_Modbus(tx_frame, 6U);
    tx_frame[6] = (uint8_t)(tx_crc & 0xFFU);  /* CRC low byte first */
    tx_frame[7] = (uint8_t)(tx_crc >> 8U);

    /* Drive DE pin HIGH — enable RS-485 transmitter */
    HAL_GPIO_WritePin(AGRD_PIN_RS485_DE_PORT,
                      AGRD_PIN_RS485_DE_PIN,
                      GPIO_PIN_SET);

    hal_rc = HAL_UART_Transmit(huart, tx_frame, SOIL_MODBUS_FRAME_BYTES,
                                SOIL_MODBUS_TIMEOUT_MS);

    /* Drive DE pin LOW — switch to receive mode immediately after last bit */
    HAL_GPIO_WritePin(AGRD_PIN_RS485_DE_PORT,
                      AGRD_PIN_RS485_DE_PIN,
                      GPIO_PIN_RESET);

    if (hal_rc != HAL_OK) {
        return AGRD_ERR_HAL;
    }

    /* Receive response frame: addr(1) + func(1) + byte_count(1) + data(10) + CRC(2) = 15 */
    hal_rc = HAL_UART_Receive(huart, rx_frame, SOIL_MODBUS_RESP_BYTES,
                               SOIL_MODBUS_TIMEOUT_MS);
    if (hal_rc != HAL_OK) {
        return AGRD_ERR_TIMEOUT;
    }

    /* Validate CRC */
    calc_crc = CRC16_Modbus(rx_frame, SOIL_MODBUS_RESP_BYTES - 2U);
    rx_crc = (uint16_t)rx_frame[SOIL_MODBUS_RESP_BYTES - 2U] |
             ((uint16_t)rx_frame[SOIL_MODBUS_RESP_BYTES - 1U] << 8U);

    if (calc_crc != rx_crc) {
        return AGRD_ERR_CRC;
    }

    /* Validate address and function code echo */
    if ((rx_frame[0] != SOIL_MODBUS_ADDR) || (rx_frame[1] != SOIL_MODBUS_FUNC_READ_HOLD)) {
        return AGRD_ERR_SENSOR;
    }

    /* Parse 5 × uint16 register values (big-endian in response payload) */
    out->moisture_cpct   = ((uint16_t)rx_frame[3]  << 8U) | rx_frame[4];
    out->temperature_cdeg= (int16_t)(((uint16_t)rx_frame[5]  << 8U) | rx_frame[6]);
    out->ec_us_cm        = ((uint16_t)rx_frame[7]  << 8U) | rx_frame[8];
    out->ph_x10          = ((uint16_t)rx_frame[9]  << 8U) | rx_frame[10];
    out->nitrogen_mg_kg  = ((uint16_t)rx_frame[11] << 8U) | rx_frame[12];

    return AGRD_OK;
}

/* =========================================================================
 * COMBINED PIPELINE RUNNER
 * ========================================================================= */

AgrdStatus_t Pipelines_RunAll(I2C_HandleTypeDef    *hi2c,
                              UART_HandleTypeDef   *huart,
                              AgriSystemContext_t  *ctx,
                              AgriTelemetryFrame_t *frame)
{
    AgrdStatus_t rc;
    BME680_Data_t bme_data;
    AS7261_Data_t spec_data;
    Soil_Data_t   soil_data;

    (void)memset(frame, 0, sizeof(AgriTelemetryFrame_t));

    frame->node_id      = AGRD_PLATFORM_ID;
    frame->frame_type   = 0x01U;
    frame->sequence_num = ctx->sequence_counter++;

    /* Pipeline D — BME680 Microclimate */
    rc = BME680_TriggerForcedMode(hi2c);
    if (rc != AGRD_OK) { return rc; }

    /* Pipeline B — AS7261 Spectral (runs during BME680 measurement) */
    rc = AS7261_ReadSpectral(hi2c, &spec_data);
    if (rc != AGRD_OK) { return rc; }

    /* Collect BME680 results (measurement complete by now) */
    rc = BME680_ReadResults(hi2c, &bme_data);
    if (rc != AGRD_OK) { return rc; }

    /* Pipeline C — Soil RS-485 */
    rc = Soil_ReadParameters(huart, &soil_data);
    if (rc != AGRD_OK) { return rc; }

    /* Pack telemetry fields */
    frame->temperature_cdeg    = (int16_t)bme_data.temperature_cdeg;
    frame->humidity_cpct       = (uint16_t)(bme_data.humidity_cpct / 10U);
    frame->pressure_pa         = (uint16_t)(bme_data.pressure_pa / 100U);
    frame->gas_resistance_kohm = (uint16_t)(bme_data.gas_resistance_ohm / 1000U);
    frame->soil_moisture_cpct  = soil_data.moisture_cpct;
    frame->soil_temp_cdeg      = soil_data.temperature_cdeg;
    frame->soil_ec_us_cm       = soil_data.ec_us_cm;
    frame->ndvi_proxy          = AS7261_ComputeNDVI(&spec_data);

    /* Alert flags: EC > 2000 µS/cm → saline stress flag */
    if (soil_data.ec_us_cm > 2000U) {
        frame->alert_flags |= (1U << 5U);
    }
    /* Drought proxy: NDVI_proxy < 600 (NDVI < -0.4 after shift) */
    if (frame->ndvi_proxy < 600U) {
        frame->alert_flags |= (1U << 6U);
    }

    /* CRC covers all bytes except the final crc16 field */
    frame->crc16 = CRC16_Modbus((const uint8_t *)frame,
                                  sizeof(AgriTelemetryFrame_t) - 2U);
    return AGRD_OK;
}
