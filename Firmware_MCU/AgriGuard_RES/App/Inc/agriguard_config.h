/**
 * @file    agriguard_config.h
 * @brief   AgriGuard-RES Master Configuration Header
 *          Agrionics Systems Co. | Platform: STM32H743IIT6 + iCE40HX8K
 *
 * All physical constants, pin assignments, timing budgets, and enumerated
 * types are centralized here. No magic numbers permitted in translation units.
 *
 * HARDWARE REVISION: 1.0 (Lesotho Highland Deployment)
 * SPDX-License-Identifier: Proprietary
 */

#ifndef AGRIGUARD_CONFIG_H
#define AGRIGUARD_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "stm32h7xx_hal.h"

/* =========================================================================
 * FIRMWARE METADATA
 * ========================================================================= */
#define AGRIGUARD_FW_MAJOR          (1U)
#define AGRIGUARD_FW_MINOR          (0U)
#define AGRIGUARD_FW_PATCH          (0U)
#define AGRIGUARD_PLATFORM_ID       (0xA6U)   /* Agrionics RES node type */

/* =========================================================================
 * SYSTEM CLOCK CONFIGURATION
 * STM32H743 @ 480 MHz HSE-based PLL. Peripherals run at reduced clocks
 * during ACTIVE_COMPUTE_POLL; full speed only during FPGA DMA burst.
 * ========================================================================= */
#define SYSCLK_FULL_HZ              (480000000UL)
#define SYSCLK_ACTIVE_HZ            (120000000UL)   /* Reduced-speed polling */
#define HCLK_DIV                    (2U)
#define PCLK1_DIV                   (4U)
#define PCLK2_DIV                   (2U)

/* =========================================================================
 * POWER RAIL CONSTANTS
 * ========================================================================= */
#define VREG_MAIN_UVLO_START_MV     (5000U)   /* TPS563300 UVLO start     */
#define VREG_MAIN_UVLO_STOP_MV      (4000U)   /* TPS563300 UVLO shutdown  */
#define VREG_ALWAYS_ON_UA           (28U)     /* LMZM23600V3 quiescent    */
#define VREG_FPGA_CORE_MV           (1200U)   /* TPS62932 regulated       */
#define VREG_BATTERY_FAILOVER_MV    (5300U)   /* 2x CR2032 + Schottky     */
#define SLEEP_CURRENT_BUDGET_UA     (50U)     /* Hard ceiling on 3.3V     */

/* =========================================================================
 * GPIO PIN MAP  (Port + Pin encoded as constants)
 * Naming: AGRD_PIN_<FUNCTION>_{PORT|PIN}
 * ========================================================================= */

/* Power gating — drives EN pin of TPS563300 main 3.3V active buck */
#define AGRD_PIN_MAIN_PWR_EN_PORT   GPIOB
#define AGRD_PIN_MAIN_PWR_EN_PIN    GPIO_PIN_12

/* iCE40 SPI Slave Programming Interface (dedicated SPI2 on STM32) */
#define AGRD_ICE40_PROG_SPI         SPI2
#define AGRD_PIN_ICE40_CSN_PORT     GPIOB
#define AGRD_PIN_ICE40_CSN_PIN      GPIO_PIN_9
#define AGRD_PIN_ICE40_CDONE_PORT   GPIOB
#define AGRD_PIN_ICE40_CDONE_PIN    GPIO_PIN_8
#define AGRD_PIN_ICE40_CRESET_PORT  GPIOB
#define AGRD_PIN_ICE40_CRESET_PIN   GPIO_PIN_7
/* SPI2 SCK/MOSI/MISO handled by HAL alternate function config */

/* iCE40 → STM32 anomaly interrupt line (active-HIGH pulse from FPGA) */
#define AGRD_PIN_FPGA_IRQ_PORT      GPIOC
#define AGRD_PIN_FPGA_IRQ_PIN       GPIO_PIN_13
#define AGRD_FPGA_IRQ_EXTI_LINE     EXTI_LINE_13

/* W25Q128JVS NOR Flash (SPI3, dedicated bus) */
#define AGRD_FLASH_SPI              SPI3
#define AGRD_PIN_FLASH_CSN_PORT     GPIOA
#define AGRD_PIN_FLASH_CSN_PIN      GPIO_PIN_4

/* SX1262 LoRa (SPI1, dedicated bus) */
#define AGRD_LORA_SPI               SPI1
#define AGRD_PIN_LORA_CSN_PORT      GPIOA
#define AGRD_PIN_LORA_CSN_PIN       GPIO_PIN_15
#define AGRD_PIN_LORA_BUSY_PORT     GPIOC
#define AGRD_PIN_LORA_BUSY_PIN      GPIO_PIN_2
#define AGRD_PIN_LORA_DIO1_PORT     GPIOC
#define AGRD_PIN_LORA_DIO1_PIN      GPIO_PIN_3
#define AGRD_PIN_LORA_NRESET_PORT   GPIOC
#define AGRD_PIN_LORA_NRESET_PIN    GPIO_PIN_4

/* RS-485 MAX3485E direction GPIO (HIGH = transmit, LOW = receive) */
#define AGRD_PIN_RS485_DE_PORT      GPIOD
#define AGRD_PIN_RS485_DE_PIN       GPIO_PIN_4

/* Sensor bus I2C (I2C1 — BME680 + AS7261 isolated via dual buffers) */
#define AGRD_SENSOR_I2C             I2C1

/* =========================================================================
 * I2C DEVICE ADDRESSES (7-bit, not shifted)
 * ========================================================================= */
#define AGRD_I2C_ADDR_BME680        (0x76U)   /* SDO tied to GND         */
#define AGRD_I2C_ADDR_AS7261        (0x49U)   /* Factory default         */

/* =========================================================================
 * W25Q128JVS FLASH LAYOUT
 * 16 MiB total. Sectors = 4KB, Blocks = 64KB.
 * Sector 0 is write-protected for boot metadata.
 * ========================================================================= */
#define FLASH_TOTAL_BYTES           (16777216UL)  /* 128 Mbit = 16 MiB    */
#define FLASH_SECTOR_BYTES          (4096U)
#define FLASH_PAGE_BYTES            (256U)

/* iCE40HX8K bitstream: max ~1 MiB, stored at base address */
#define FLASH_BITSTREAM_BASE_ADDR   (0x00000000UL)
#define FLASH_BITSTREAM_MAX_BYTES   (1048576UL)   /* 1 MiB reserved       */

/* Telemetry ring buffer starts after bitstream zone */
#define FLASH_TELEMETRY_BASE_ADDR   (0x00100000UL)
#define FLASH_TELEMETRY_BYTES       (15728640UL)  /* 15 MiB               */

/* =========================================================================
 * SPI BUS TIMING CONSTANTS
 * ========================================================================= */
#define FLASH_SPI_TIMEOUT_MS        (100U)
#define ICE40_SPI_TIMEOUT_MS        (500U)
#define ICE40_CONF_CHUNK_BYTES      (256U)    /* DMA burst chunk size     */
#define ICE40_CRESET_PULSE_US       (200U)    /* Minimum per datasheet    */
#define ICE40_CDONE_POLL_TIMEOUT_MS (5000U)

/* W25Q128 SPI command opcodes */
#define W25Q_CMD_WRITE_ENABLE       (0x06U)
#define W25Q_CMD_READ_STATUS1       (0x05U)
#define W25Q_CMD_READ_STATUS2       (0x35U)
#define W25Q_CMD_FAST_READ          (0x0BU)
#define W25Q_CMD_SECTOR_ERASE       (0x20U)
#define W25Q_CMD_PAGE_PROGRAM       (0x02U)
#define W25Q_CMD_CHIP_ERASE         (0xC7U)
#define W25Q_CMD_POWER_DOWN         (0xB9U)
#define W25Q_CMD_RELEASE_POWER_DOWN (0xABU)
#define W25Q_CMD_READ_JEDEC_ID      (0x9FU)
#define W25Q_JEDEC_MANUF_ID         (0xEFU)   /* Winbond manufacturer    */
#define W25Q_STATUS1_BUSY_MASK      (0x01U)
#define W25Q_ERASE_TIMEOUT_MS       (3000U)
#define W25Q_WRITE_TIMEOUT_MS       (50U)

/* =========================================================================
 * SENSOR PIPELINE TIMING
 * ========================================================================= */
#define PIPELINE_POLL_INTERVAL_MS   (5000U)   /* 5-second measurement cycle */
#define BME680_MEAS_TIMEOUT_MS      (200U)
#define AS7261_INTEGRATION_MS       (100U)
#define SOIL_MODBUS_TIMEOUT_MS      (300U)
#define SOIL_MODBUS_BAUD            (9600U)

/* AS7261 register addresses (virtual, accessed via I2C command port) */
#define AS7261_REG_DEV_TEMP         (0x06U)
#define AS7261_REG_RAW_X            (0x08U)
#define AS7261_VIRTUAL_READ_FLAG    (0x80U)

/* BME680 register addresses */
#define BME680_REG_STATUS           (0x73U)
#define BME680_REG_CTRL_MEAS        (0x74U)
#define BME680_REG_MEAS_STATUS      (0x1DU)
#define BME680_REG_PRESS_MSB        (0x1FU)
#define BME680_FORCED_MODE          (0x01U)
#define BME680_NEW_DATA_MASK        (0x80U)

/* Modbus RTU soil sensor (RS-485) */
#define SOIL_MODBUS_ADDR            (0x01U)   /* Factory default slave ID */
#define SOIL_MODBUS_FUNC_READ_HOLD  (0x03U)
#define SOIL_MODBUS_REG_MOISTURE    (0x0000U)
#define SOIL_MODBUS_REG_COUNT       (5U)      /* Moisture,Temp,EC,pH,N   */
#define SOIL_MODBUS_FRAME_BYTES     (8U)      /* ADU: addr+func+addr+cnt+CRC */
#define SOIL_MODBUS_RESP_BYTES      (15U)     /* Response frame length   */

/* =========================================================================
 * LORA SX1262 CONFIGURATION (433 MHz Mesh)
 * ========================================================================= */
#define LORA_FREQ_HZ                (433000000UL)
#define LORA_SPREADING_FACTOR       (7U)
#define LORA_BANDWIDTH_KHZ          (125U)
#define LORA_CODING_RATE            (1U)      /* 4/5                     */
#define LORA_TX_POWER_DBM           (14U)
#define LORA_PREAMBLE_LEN           (8U)
#define LORA_BUSY_TIMEOUT_MS        (10U)
#define LORA_PACKET_MAX_BYTES       (64U)

/* =========================================================================
 * ACOUSTIC / FPGA FFT PARAMETERS
 * ========================================================================= */
#define FFT_POINTS                  (256U)
#define PDM_DECIMATION_RATIO        (64U)     /* PDM 3.072MHz → PCM 48kHz */
#define PDM_SAMPLE_RATE_HZ          (3072000U)
#define PCM_SAMPLE_RATE_HZ          (PDM_SAMPLE_RATE_HZ / PDM_DECIMATION_RATIO)

/* FAW (Fall Armyworm) acoustic signature: stridulation ~5–12 kHz */
#define FAW_FREQ_LOW_HZ             (5000U)
#define FAW_FREQ_HIGH_HZ            (12000U)
#define FAW_FFT_BIN_LOW             ((FAW_FREQ_LOW_HZ  * FFT_POINTS) / PCM_SAMPLE_RATE_HZ)
#define FAW_FFT_BIN_HIGH            ((FAW_FREQ_HIGH_HZ * FFT_POINTS) / PCM_SAMPLE_RATE_HZ)

/* FPGA anomaly detection threshold encoded as SPI payload byte */
#define FPGA_ANOMALY_TYPE_FAW       (0x01U)
#define FPGA_ANOMALY_TYPE_NONE      (0x00U)

/* =========================================================================
 * POWER MANAGEMENT SLEEP THRESHOLDS
 * ========================================================================= */
#define VBAT_LOW_THRESHOLD_MV       (3400U)   /* Trigger emergency sleep  */
#define VBAT_CRIT_THRESHOLD_MV      (3000U)   /* Halt all writes to flash */
#define DEEP_SLEEP_DURATION_S       (300U)    /* 5-minute hibernate cycle */
#define WAKEUP_RETRY_MAX            (3U)

/* ==========================================================================
 * In agriguard_config.h — extern declarations for ISR access */
/* FPGA INTERRUPT CATCHES
 * =========================================================================== */

extern volatile bool       g_fpga_irq_pending;
extern AgriSystemContext_t g_ctx;

/* =========================================================================
 * TELEMETRY FRAME STRUCTURE (packed, stored to NOR Flash + LoRa TX)
 * 32 bytes per record — sector-aligned for efficient flash writes.
 * ========================================================================= */
#pragma pack(push, 1)
typedef struct {
    uint8_t     node_id;          /* Agrionics network node ID            */
    uint8_t     frame_type;       /* 0x01=telemetry, 0x02=anomaly alert   */
    uint16_t    sequence_num;     /* Rolling frame counter                */
    uint32_t    unix_timestamp;   /* RTC epoch seconds                    */
    int16_t     temperature_cdeg; /* BME680 temp × 100 (e.g. 2350 = 23.50°C) */
    uint16_t    humidity_cpct;    /* BME680 RH × 100                      */
    uint16_t    pressure_pa;      /* BME680 pressure in Pa (truncated)    */
    uint16_t    gas_resistance_kohm; /* BME680 gas sensor                 */
    uint16_t    soil_moisture_cpct;  /* Soil sensor × 100                 */
    int16_t     soil_temp_cdeg;
    uint16_t    soil_ec_us_cm;    /* Electrical conductivity              */
    uint16_t    ndvi_proxy;       /* AS7261 NIR ratio (scaled ×1000)      */
    uint8_t     fpga_anomaly;     /* FPGA_ANOMALY_TYPE_* byte             */
    uint8_t     alert_flags;      /* Bit-field: [7]=FAW [6]=drought [5]=EC_high */
    uint16_t    crc16;            /* CRC-16/CCITT of preceding 30 bytes   */
} AgriTelemetryFrame_t;
#pragma pack(pop)

_Static_assert(sizeof(AgriTelemetryFrame_t) == 32U,
    "AgriTelemetryFrame_t must be exactly 32 bytes for flash alignment");

/* =========================================================================
 * SYSTEM STATE MACHINE STATES
 * ========================================================================= */
typedef enum {
    SYS_STATE_INITIALIZATION      = 0x00U,
    SYS_STATE_FPGA_BITSTREAM_BOOT = 0x01U,
    SYS_STATE_ACTIVE_COMPUTE_POLL = 0x02U,
    SYS_STATE_DEEP_SLEEP_SHUTDOWN = 0x03U,
    SYS_STATE_FAULT               = 0xFFU
} SystemState_t;

/* =========================================================================
 * RETURN / STATUS CODES
 * ========================================================================= */
typedef enum {
    AGRD_OK              = 0x00U,
    AGRD_ERR_TIMEOUT     = 0x01U,
    AGRD_ERR_HAL         = 0x02U,
    AGRD_ERR_FLASH       = 0x03U,
    AGRD_ERR_FPGA_CONF   = 0x04U,
    AGRD_ERR_SENSOR      = 0x05U,
    AGRD_ERR_LORA        = 0x06U,
    AGRD_ERR_CRC         = 0x07U,
    AGRD_ERR_PARAM       = 0x08U,
    AGRD_ERR_POWER       = 0x09U
} AgrdStatus_t;

/* =========================================================================
 * RUNTIME CONTEXT (singleton, allocated in main.c)
 * ========================================================================= */
typedef struct {
    SystemState_t       state;
    AgriTelemetryFrame_t latest_frame;
    uint16_t            sequence_counter;
    uint32_t            sleep_cycles;
    uint8_t             wakeup_retry_count;
    bool                fpga_configured;
    bool                anomaly_pending;
    uint32_t            flash_write_ptr;   /* Next free telemetry address */
} AgriSystemContext_t;

#ifdef __cplusplus
}
#endif

#endif /* AGRIGUARD_CONFIG_H */
