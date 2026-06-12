/**
 * @file    power_manager.c
 * @brief   Power Management Implementation — AgriGuard-RES
 *          Agrionics Systems Co.
 *
 * Rail sequencing for safe FPGA/sensor power-gating:
 *
 *  POWER-ON  (Enable order):
 *    1. TPS563300 3.3V active rail  (EN = HIGH, wait UVLO)
 *    2. Wait ≥1ms for 3.3V to stabilize
 *    3. TPS62932 1.2V FPGA rail     (EN = HIGH)
 *    4. Wait ≥500µs for 1.2V to stabilize before FPGA configuration
 *
 *  POWER-OFF (Disable order — reverse):
 *    1. TPS62932 1.2V FPGA rail     (EN = LOW)
 *    2. Wait ≥100µs
 *    3. TPS563300 3.3V active rail  (EN = LOW)
 *
 * STM32H743 STOP2 achieves ~10–15µA CPU core current.
 * With LMZM23600V3 quiescent at 28µA and RTC at ~1µA, total < 50µA ✓
 */

#include "power_manager.h"

/* =========================================================================
 * STATIC HANDLE REFERENCES
 * These are assigned by main.c during initialization via extern references.
 * ========================================================================= */
extern ADC_HandleTypeDef hadc3;   /* VBAT measurement ADC */
extern LPTIM_HandleTypeDef hlptim1;

/* GPIO pins for TPS563300 EN and TPS62932 EN */
/* TPS62932 EN is on a dedicated pin — add to config.h if needed */
#define AGRD_PIN_FPGA_PWR_EN_PORT   GPIOC
#define AGRD_PIN_FPGA_PWR_EN_PIN    GPIO_PIN_8

/* =========================================================================
 * INITIALIZATION
 * ========================================================================= */

void PowerMgr_Init(void)
{
    /* Both EN pins start LOW — rails disabled */
    HAL_GPIO_WritePin(AGRD_PIN_MAIN_PWR_EN_PORT,
                      AGRD_PIN_MAIN_PWR_EN_PIN,
                      GPIO_PIN_RESET);

    HAL_GPIO_WritePin(AGRD_PIN_FPGA_PWR_EN_PORT,
                      AGRD_PIN_FPGA_PWR_EN_PIN,
                      GPIO_PIN_RESET);
}

/* =========================================================================
 * ACTIVE RAIL CONTROL
 * ========================================================================= */

AgrdStatus_t PowerMgr_EnableMainRail(void)
{
    uint32_t t_start = HAL_GetTick();
    uint32_t vbat_mv;

    /*
     * TPS563300 has a hardware UVLO at 5.0V start. Before enabling EN,
     * confirm the source rail (ADP5090 output or battery) is adequate.
     */
    vbat_mv = PowerMgr_ReadVbatMV();
    if (vbat_mv < VREG_MAIN_UVLO_START_MV) {
        return AGRD_ERR_POWER;
    }

    HAL_GPIO_WritePin(AGRD_PIN_MAIN_PWR_EN_PORT,
                      AGRD_PIN_MAIN_PWR_EN_PIN,
                      GPIO_PIN_SET);

    /*
     * Wait for 3.3V to reach regulation (TPS563300 start-up < 2ms typical).
     * We use a conservative 5ms guard with a busy poll on tick.
     */
    while ((HAL_GetTick() - t_start) < 5U) {
        __NOP();
    }

    return AGRD_OK;
}

void PowerMgr_DisableMainRail(void)
{
    HAL_GPIO_WritePin(AGRD_PIN_MAIN_PWR_EN_PORT,
                      AGRD_PIN_MAIN_PWR_EN_PIN,
                      GPIO_PIN_RESET);
}

void PowerMgr_EnableFPGARail(void)
{
    HAL_GPIO_WritePin(AGRD_PIN_FPGA_PWR_EN_PORT,
                      AGRD_PIN_FPGA_PWR_EN_PIN,
                      GPIO_PIN_SET);

    /* TPS62932 start-up time ~200µs; use 500µs guard via DWT */
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = 500U * (SystemCoreClock / 1000000U);
    while ((DWT->CYCCNT - start) < ticks) {
        __NOP();
    }
}

void PowerMgr_DisableFPGARail(void)
{
    HAL_GPIO_WritePin(AGRD_PIN_FPGA_PWR_EN_PORT,
                      AGRD_PIN_FPGA_PWR_EN_PIN,
                      GPIO_PIN_RESET);

    /* Allow 100µs discharge before main rail can be disabled */
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = 100U * (SystemCoreClock / 1000000U);
    while ((DWT->CYCCNT - start) < ticks) {
        __NOP();
    }
}

/* =========================================================================
 * PERIPHERAL CLOCK GATING
 * ========================================================================= */

void PowerMgr_GatePeripheralClocks(void)
{
    /*
     * Disable peripheral clocks for all buses except:
     *  - AHB4 (GPIOC for FPGA_IRQ EXTI wakeup)
     *  - APB4 (RTC, LPTIM, SYSCFG, EXTI)
     * This ensures STOP2 wakeup via RTC/LPTIM still functions.
     */
    __HAL_RCC_SPI1_CLK_DISABLE();
    __HAL_RCC_SPI2_CLK_DISABLE();
    __HAL_RCC_SPI3_CLK_DISABLE();
    __HAL_RCC_I2C1_CLK_DISABLE();
    __HAL_RCC_USART3_CLK_DISABLE();
    __HAL_RCC_ADC3_CLK_DISABLE();
    __HAL_RCC_DMA1_CLK_DISABLE();
    __HAL_RCC_DMA2_CLK_DISABLE();
}

void PowerMgr_UngatePeripheralClocks(void)
{
    __HAL_RCC_SPI1_CLK_ENABLE();
    __HAL_RCC_SPI2_CLK_ENABLE();
    __HAL_RCC_SPI3_CLK_ENABLE();
    __HAL_RCC_I2C1_CLK_ENABLE();
    __HAL_RCC_USART3_CLK_ENABLE();
    __HAL_RCC_ADC3_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE();
}

/* =========================================================================
 * DEEP SLEEP ENTRY
 * ========================================================================= */

void PowerMgr_DeepSleep(uint32_t duration_s)
{
    /*
     * Full shutdown sequence before STM32H743 STOP2 entry.
     *
     * 1. Reset FPGA (CRESET_B asserted)
     * 2. Flash power-down
     * 3. SX1262 NRESET asserted
     * 4. Disable FPGA 1.2V rail
     * 5. Disable main 3.3V active rail
     * 6. Gate peripheral clocks
     * 7. Configure LPTIM1 for wakeup at duration_s
     * 8. Enter STOP2
     * 9. On wakeup: SystemClock_Config(), re-enable rails, re-enable clocks
     */

    /* Step 1: Assert FPGA CRESET */
    ICE40_AssertReset();

    /* Step 2: Flash power-down — needs SPI3 still clocked */
    extern SPI_HandleTypeDef hspi3;
    Flash_PowerDown(&hspi3);
    /* tDP minimum 3µs before rail goes down */
    for (volatile uint32_t i = 0U; i < 480U; i++) { __NOP(); } /* ~1µs@480MHz */

    /* Step 3: Assert SX1262 NRESET */
    HAL_GPIO_WritePin(AGRD_PIN_LORA_NRESET_PORT,
                      AGRD_PIN_LORA_NRESET_PIN,
                      GPIO_PIN_RESET);

    /* Step 4 & 5: Power rail disable (FPGA first, then 3.3V) */
    PowerMgr_DisableFPGARail();
    PowerMgr_DisableMainRail();

    /* Step 6: Gate peripheral clocks */
    PowerMgr_GatePeripheralClocks();

    /*
     * Step 7: Program LPTIM1 for wakeup.
     * LPTIM1 sourced from LSE (32.768 kHz).
     * Prescaler /128 → tick period = 3906µs ≈ 3.9ms
     * Counter compare = duration_s * (32768 / 128) = duration_s * 256
     */
    uint32_t lptim_compare = duration_s * 256U;
    if (lptim_compare > 0xFFFFU) {
        lptim_compare = 0xFFFFU; /* clamp to 16-bit max (~255s) */
    }

    HAL_LPTIM_SetOnce_Start_IT(&hlptim1, 0xFFFFU, (uint32_t)lptim_compare);

    /* Step 8: Enter STOP2 mode — deepest mode that retains SRAM */
    HAL_PWREx_EnterSTOP2Mode(PWR_STOPENTRY_WFI);

    /*
     * ---- EXECUTION RESUMES HERE AFTER LPTIM WAKEUP ----
     *
     * After STOP2, the system clock is running from HSI (64 MHz default).
     * The application must restore the full PLL clock configuration.
     */

    /* Step 9a: Restore system clock to full speed PLL */
    extern void SystemClock_Config(void);
    SystemClock_Config();

    /* Step 9b: Restore SysTick for HAL_GetTick() */
    HAL_InitTick(TICK_INT_PRIORITY);

    /* Step 9c: Re-enable peripheral clocks */
    PowerMgr_UngatePeripheralClocks();

    /* Step 9d: Re-enable power rails (main 3.3V, then FPGA 1.2V) */
    (void)PowerMgr_EnableMainRail();

    /* Release SX1262 NRESET */
    HAL_GPIO_WritePin(AGRD_PIN_LORA_NRESET_PORT,
                      AGRD_PIN_LORA_NRESET_PIN,
                      GPIO_PIN_SET);

    /* Flash wakeup */
    extern SPI_HandleTypeDef hspi3;
    Flash_WakeUp(&hspi3);

    /*
     * NOTE: FPGA 1.2V rail and FPGA configuration are NOT re-enabled here.
     * The state machine in main.c will transition to SYS_STATE_FPGA_BITSTREAM_BOOT
     * and handle that sequence explicitly for clean separation of concerns.
     */
}

/* =========================================================================
 * VBAT ADC MEASUREMENT
 * ========================================================================= */

uint32_t PowerMgr_ReadVbatMV(void)
{
    HAL_StatusTypeDef rc;
    uint32_t raw_adc = 0U;

    /*
     * ADC3 channel configured for the VBAT divider input.
     * Hardware divider: 100kΩ / 100kΩ → ADC sees Vbat/2
     * Vref = 3.3V, 12-bit resolution → LSB = 3300/4096 mV = 0.806 mV
     * Vbat = (raw_adc * 3300 / 4096) * 2
     */
    rc = HAL_ADC_Start(&hadc3);
    if (rc != HAL_OK) {
        return 0U;
    }

    rc = HAL_ADC_PollForConversion(&hadc3, 10U);
    if (rc != HAL_OK) {
        (void)HAL_ADC_Stop(&hadc3);
        return 0U;
    }

    raw_adc = HAL_ADC_GetValue(&hadc3);
    (void)HAL_ADC_Stop(&hadc3);

    /* Scale: Vbat_mV = raw × (3300 × 2) / 4096 */
    return (raw_adc * 6600U) / 4096U;
}

bool PowerMgr_IsBatteryOK(void)
{
    return (PowerMgr_ReadVbatMV() >= VBAT_LOW_THRESHOLD_MV);
}
