/**
 * @file    power_manager.h
 * @brief   Power Management Interface — AgriGuard-RES
 *          Agrionics Systems Co.
 *
 * Controls:
 *  - TPS563300 active 3.3V rail via EN GPIO (power-gates sensors + FPGA IO)
 *  - TPS62932 FPGA 1.2V rail (same EN strategy, sequenced after 3.3V)
 *  - LMZM23600V3 always-on 3.3V rail (MCU + RTC, never gated)
 *  - STM32H743 CStop / CSLEEP low-power mode sequencing
 *  - ADC-based VBAT monitoring via internal divider path
 *
 * Sleep target: < 50µA total on the always-on 3.3V LMZM rail.
 * Achieved via: STOP2 mode + peripheral clock gate + Flash power-down.
 */

#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "agriguard_config.h"

/* =========================================================================
 * INITIALIZATION
 * ========================================================================= */

/**
 * @brief  Configure EN GPIO outputs for TPS563300 and TPS62932.
 *         Both rails start DISABLED (EN = LOW) until explicitly enabled.
 *         LMZM23600 is always-on by hardware — not controlled here.
 */
void PowerMgr_Init(void);

/* =========================================================================
 * ACTIVE RAIL CONTROL
 * ========================================================================= */

/**
 * @brief  Enable the active 3.3V TPS563300 rail.
 *         Blocks until UVLO startup condition (≥5.0V) is met or times out.
 * @note   Must be called before sensor or FPGA access.
 * @retval AGRD_OK or AGRD_ERR_POWER if rail does not start.
 */
AgrdStatus_t PowerMgr_EnableMainRail(void);

/**
 * @brief  Disable the active 3.3V TPS563300 rail (EN = LOW).
 *         Power-gates all sensors, FPGA IO, and LoRa module.
 * @note   Caller must ensure FPGA is reset and all SPI CS lines are HIGH
 *         before calling, to prevent backfeed via GPIO protection diodes.
 */
void PowerMgr_DisableMainRail(void);

/**
 * @brief  Enable the FPGA 1.2V TPS62932 core rail.
 * @note   Must be called AFTER EnableMainRail() — iCE40 requires 3.3V IO
 *         to be present before 1.2V core powers up.
 */
void PowerMgr_EnableFPGARail(void);

/**
 * @brief  Disable the FPGA 1.2V core rail.
 * @note   Must be called BEFORE DisableMainRail() — reverse of enable order.
 */
void PowerMgr_DisableFPGARail(void);

/* =========================================================================
 * LOW-POWER SLEEP ENTRY
 * ========================================================================= */

/**
 * @brief  Execute a controlled deep sleep cycle of duration_s seconds.
 *
 * Sequence:
 *   1. Assert FPGA CRESET_B (unconfigure FPGA)
 *   2. Send W25Q to power-down (0xB9)
 *   3. Disable SX1262 via NRESET
 *   4. Disable TPS62932 FPGA 1.2V rail
 *   5. Disable TPS563300 main 3.3V rail
 *   6. Gate all STM32 peripheral clocks except RTC and LPTIM
 *   7. Enter STM32H743 STOP2 mode (Cortex-M7 off, RTC alive)
 *   8. LPTIM wakeup after duration_s → resume from STOP2
 *   9. Re-enable clocks and rails in reverse order
 *
 * @param  duration_s  Sleep duration in seconds (uses LPTIM1 for wakeup).
 */
void PowerMgr_DeepSleep(uint32_t duration_s);

/* =========================================================================
 * BATTERY MONITORING
 * ========================================================================= */

/**
 * @brief  Read the battery / MPPT rail voltage via internal ADC.
 * @retval Voltage in millivolts.  Returns 0 on ADC error.
 */
uint32_t PowerMgr_ReadVbatMV(void);

/**
 * @brief  Check if VBAT is above the operational threshold.
 * @retval true if VBAT >= VBAT_LOW_THRESHOLD_MV, false otherwise.
 */
bool PowerMgr_IsBatteryOK(void);

/* =========================================================================
 * PERIPHERAL CLOCK GATING HELPERS
 * ========================================================================= */

/** Disable clocks for all non-essential peripherals before STOP2. */
void PowerMgr_GatePeripheralClocks(void);

/** Re-enable peripheral clocks after wakeup from STOP2. */
void PowerMgr_UngatePeripheralClocks(void);

#ifdef __cplusplus
}
#endif

#endif /* POWER_MANAGER_H */
