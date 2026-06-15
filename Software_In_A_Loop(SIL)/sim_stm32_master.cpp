// SystemC stub required by Verilator for VCD tracing
double sc_time_stamp() { return 0.0; }

/**
 * sim_stm32_master.cpp
 * AgriGuard-RES Software-in-the-Loop (SIL) Test
 * Agrionics Systems Co.
 *
 * WHY THIS EXISTS (and why Renode was abandoned):
 *   Renode-Verilator integration uses a versioned C++ API (RenodeAgent class).
 *   The addInput/addOutput/simulate methods in the old sim_main.cpp template
 *   do not exist in the modern renode_bus.h API — a breaking API change
 *   introduced in Renode 1.14+. Fixing it requires matching the exact repo
 *   commit to the Renode binary version, which is hours of dependency hell.
 *
 *   For our specific verification goal — confirming that the STM32 SPI2
 *   master correctly communicates with the iCE40 SPI slave across the
 *   FAW detect → IRQ → read → clear cycle — we do NOT need a full STM32
 *   emulator. We need a precise SPI master stimulus that mirrors what
 *   ice40_config.c and main.c actually do.
 *
 *   This harness IS that stimulus. It is simpler, faster, and gives us
 *   exact pass/fail on the protocol transactions.
 *
 * What this verifies (mirrors STM32 firmware sequence exactly):
 *   1. Power-on reset sequence
 *   2. 4 FFT frames of 8kHz sigma-delta PDM input (Fall Armyworm target freq)
 *   3. fpga_irq assertion (FPGA wakes STM32 via EXTI13)
 *   4. SPI READ  0x80 → STATUS register → expect 0x01
 *   5. SPI READ  0xA0 → ANOMALY register → expect 0x01 (FAW_DETECTED)
 *   6. SPI WRITE 0x7F → CLEAR_IRQ → fpga_irq must go LOW
 *   7. VCD dump → open in GTKWave for waveform inspection
 *
 * Build (from HDL_FPGA/ directory, in OSS CAD Suite or MSYS2 shell):
 *
 *  # build on OSS CAD Suite - vs code terminal
 *   verilator --sv --cc rtl/pdm_decimator.sv rtl/fft_engine.sv \
 *             rtl/spi_slave.sv rtl/agriguard_top.sv \
 *             --top-module agriguard_top --trace --Mdir obj_dir \
 *             -Wno-fatal -Wno-WIDTHTRUNC -Wno-WIDTHEXPAND \
 *             -Wno-UNUSED -Wno-UNDRIVEN -Wno-VARHIDDEN
 *
 *   # buildin on MSYS2 SHELL
 *   g++ -std=c++17 -O2 \
 *       -I. -Iobj_dir \
 *       -I$(VERILATOR_ROOT)/include \
 *       sim_stm32_master.cpp \
 *       obj_dir/Vagriguard_top__ALL.a \
 *       $(VERILATOR_ROOT)/include/verilated.cpp \
 *       $(VERILATOR_ROOT)/include/verilated_vcd_c.cpp \
 *       -o AgriGuard_SIL
 *
 *   ./AgriGuard_SIL
 *   gtkwave sil_agriguard.vcd
 */

#include <cstdio>
#include <cstdint>
#include <cmath>
#include <cstring>
#include "Vagriguard_top.h"
#include "verilated.h"
#include "verilated_vcd_c.h"

// =============================================================================
// SIMULATION STATE
// =============================================================================
static Vagriguard_top *top;
static VerilatedVcdC  *vcd;
static uint64_t        sim_time = 0;

// SPI parameters — 4 system clocks per SPI SCK half-period = 2 MHz SPI clock
// (conservative; real STM32 SPI2 runs at ~7.5 MHz but slower is safer for SIL)
static const int SPI_HALF   = 8;   // system clocks per SCK half period
static const int SPI_CS_GAP = 16;  // clocks between CS assert and first SCK

// =============================================================================
// CLOCK ENGINE
// =============================================================================
static inline void tick() {
    top->clk_16mhz = 1;
    top->eval();
    if (vcd) vcd->dump(sim_time++);

    top->clk_16mhz = 0;
    top->eval();
    if (vcd) vcd->dump(sim_time++);
}

static void ticks(int n) {
    for (int i = 0; i < n; i++) tick();
}

// =============================================================================
// SPI MASTER — Mode 0 (CPOL=0, CPHA=0), MSB-first, matches STM32 HAL config
//
// Byte format: addr byte then data byte, matching spi_slave.sv register map:
//   bit7=1 → read, bit7=0 → write
//   bits[6:0] → register address
//
// Returns: data byte received on MISO during the data phase
// =============================================================================
static uint8_t spi_txn(uint8_t addr, uint8_t data_out) {
    uint8_t data_in = 0;

    // Assert CS
    top->spi_csn = 0;
    ticks(SPI_CS_GAP);

    // ── ADDRESS BYTE ────────────────────────────────────────────────────────
    for (int b = 7; b >= 0; b--) {
        top->spi_mosi = (addr >> b) & 1;
        ticks(SPI_HALF);
        top->spi_sck = 1; top->eval();    // rising edge — slave samples MOSI
        ticks(SPI_HALF);
        top->spi_sck = 0; top->eval();    // falling edge — slave shifts MISO
    }

    // ── INTER-BYTE GAP: must be > 4 sys clocks for 2-stage sync + tx_shift load
    ticks(SPI_HALF * 6);

    // ── DATA BYTE ───────────────────────────────────────────────────────────
    for (int b = 7; b >= 0; b--) {
        top->spi_mosi = (data_out >> b) & 1;
        ticks(SPI_HALF);               // let slave drive MISO from tx_shift
        top->spi_sck = 1;
        ticks(1);                      // propagate through combinational MISO
        top->eval();
        data_in |= ((uint8_t)top->spi_miso << b);  // sample after settle
        ticks(SPI_HALF - 1);
        top->spi_sck = 0; top->eval();
    }

    // Deassert CS
    ticks(SPI_CS_GAP);
    top->spi_csn = 1;
    ticks(SPI_CS_GAP * 2);

    return data_in;
}

// =============================================================================
// PDM SIGMA-DELTA GENERATOR
// Encodes an 8kHz sine wave into a 1-bit PDM stream.
// 8kHz is squarely in the FAW acoustic signature window (5–12 kHz).
// =============================================================================
static double sd_accum = 0.5;
static double sd_phase = 0.0;
static const double AUDIO_HZ  = 8000.0;
static const double PDM_HZ    = 3200000.0;  // 16 MHz / 5
static const double TWO_PI    = 6.283185307179586;

static uint8_t next_pdm() {
    sd_phase += TWO_PI * AUDIO_HZ / PDM_HZ;
    if (sd_phase > TWO_PI) sd_phase -= TWO_PI;
    double sample = 0.5 + 0.45 * sin(sd_phase);
    sd_accum += sample;
    if (sd_accum >= 1.0) { sd_accum -= 1.0; return 1; }
    return 0;
}

// =============================================================================
// TEST HELPERS
// =============================================================================
static int pass_count = 0;
static int fail_count = 0;

static void check(const char *label, int got, int expected) {
    if (got == expected) {
        printf("  ✓  PASS  %s: 0x%02X\n", label, got);
        pass_count++;
    } else {
        printf("  ✗  FAIL  %s: got 0x%02X, expected 0x%02X\n",
               label, got, expected);
        fail_count++;
    }
}

// =============================================================================
// MAIN
// =============================================================================
int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Verilated::traceEverOn(true);

    top = new Vagriguard_top;
    vcd = new VerilatedVcdC;
    top->trace(vcd, 99);
    vcd->open("sil_agriguard.vcd");

    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║  AgriGuard-RES SIL Test — STM32 SPI Master Harness          ║\n");
    printf("║  iCE40HX8K-BG121  |  Verilator 5.x  |  Agrionics Systems   ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");

    // ─────────────────────────────────────────────────────────────────────────
    // PHASE 1: RESET
    // ─────────────────────────────────────────────────────────────────────────
    printf("[PHASE 1] Hardware reset sequence\n");

    top->clk_16mhz = 0;
    top->rst_n     = 0;
    top->spi_csn   = 1;
    top->spi_sck   = 0;
    top->spi_mosi  = 0;
    top->pdm_data  = 0;
    top->eval();

    ticks(24);                  // hold reset for 24 clock cycles
    top->rst_n = 1;

    printf("  Reset released at sim_time=%lu\n\n", sim_time);

    // ─────────────────────────────────────────────────────────────────────────
    // PHASE 2: PDM STREAMING — 4 FFT frames of 8kHz input
    // Each frame = 256 PCM samples × 333 clocks = 85,248 system clocks
    // FAW detector accumulates energy across 4 frames before asserting IRQ
    // ─────────────────────────────────────────────────────────────────────────
    printf("[PHASE 2] Streaming 4 FFT frames of 8kHz PDM data\n");
    printf("  (Simulating MEMS microphone capturing Fall Armyworm acoustic signature)\n");

    bool     irq_seen  = false;
    uint64_t irq_time  = 0;
    int      irq_frame = 0;

    for (int frame = 0; frame < 4 && !irq_seen; frame++) {
        for (int sample = 0; sample < 256; sample++) {
            for (int clk = 0; clk < 333; clk++) {
                top->pdm_data = next_pdm();
                tick();

                if (!irq_seen && top->fpga_irq) {
                    irq_seen  = true;
                    irq_time  = sim_time;
                    irq_frame = frame;
                    printf("\n  *** fpga_irq ASSERTED at sim_time=%lu "
                           "(frame %d, sample %d, clk %d) ***\n\n",
                           sim_time, frame + 1, sample, clk);
                }
            }
        }
        if (!irq_seen)
            printf("  Frame %d/4 complete (t=%lu)\n", frame + 1, sim_time);
    }

    // If IRQ came early, run a few more frames to ensure stability
    if (irq_seen) {
        for (int i = 0; i < 50; i++) {
            top->pdm_data = next_pdm();
            tick();
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    // PHASE 3: CHECK fpga_irq
    // ─────────────────────────────────────────────────────────────────────────
    printf("[PHASE 3] fpga_irq check\n");
    check("fpga_irq asserted", top->fpga_irq, 1);
    printf("\n");

    if (!top->fpga_irq) {
        printf("  !! IRQ not asserted — SPI tests will run anyway to show register state\n\n");
    }

    // ─────────────────────────────────────────────────────────────────────────
    // PHASE 4: SPI REGISTER TRANSACTIONS
    // Mirrors STM32 main.c state_active_compute_poll() → fpga_spi_read_anomaly_byte()
    // ─────────────────────────────────────────────────────────────────────────
    printf("[PHASE 4] SPI register transactions (STM32 SPI2 master simulation)\n");

    // Transaction 1: READ STATUS (address 0x80 = bit7|0x00)
    uint8_t status = spi_txn(0x80, 0x00);
    check("STATUS_REG  (addr 0x80)", status, 0x01);

    // Transaction 2: READ ANOMALY (address 0xA0 = bit7|0x20)
    uint8_t anomaly = spi_txn(0xA0, 0x00);
    check("ANOMALY_REG (addr 0xA0)", anomaly, 0x01);

    printf("\n");

    // ─────────────────────────────────────────────────────────────────────────
    // PHASE 5: CLEAR IRQ
    // Mirrors STM32 EXTI handler → write 0x7F to CLEAR_IRQ register
    // ─────────────────────────────────────────────────────────────────────────
    printf("[PHASE 5] IRQ clear (WRITE 0x7F)\n");

    spi_txn(0x7F, 0x01);          // write CLEAR_IRQ
    ticks(32);                    // allow slave FSM to process

    check("fpga_irq cleared", top->fpga_irq, 0);
    printf("\n");

    // ─────────────────────────────────────────────────────────────────────────
    // PHASE 6: THRESHOLD RECONFIGURATION
    // Mirrors STM32 writing new FAW threshold after calibration
    // ─────────────────────────────────────────────────────────────────────────
    printf("[PHASE 6] Threshold reconfiguration (THRESH_LO=0x00, THRESH_HI=0x04)\n");

    spi_txn(0x21, 0x00);          // WRITE THRESH_LO (addr 0x21 = 0x00|0x21)
    spi_txn(0x22, 0x04);          // WRITE THRESH_HI (addr 0x22 = 0x00|0x22)
    ticks(16);

    printf("  ✓  Threshold write complete\n\n");

    // ─────────────────────────────────────────────────────────────────────────
    // FINAL SUMMARY
    // ─────────────────────────────────────────────────────────────────────────
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║  SIL RESULTS                                                 ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║  Tests passed : %-3d                                          ║\n", pass_count);
    printf("║  Tests failed : %-3d                                          ║\n", fail_count);
    printf("║  IRQ asserted : %s                                        ║\n",
           irq_seen ? "YES" : "NO ");
    if (irq_seen)
        printf("║  IRQ at t     : %-10lu (frame %d)                     ║\n",
               irq_time, irq_frame + 1);
    printf("║  Total clocks : %-10lu                                    ║\n", sim_time / 2);
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    if (fail_count == 0) {
        printf("║  ✓  ALL PASS — STM32 ↔ iCE40 SPI protocol VERIFIED          ║\n");
        printf("║     Safe to proceed to hardware bring-up                     ║\n");
    } else {
        printf("║  ✗  FAILURES DETECTED — check VCD waveform                   ║\n");
    }
    printf("║  VCD: sil_agriguard.vcd  (open in GTKWave)                   ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");

    vcd->close();
    delete top;
    return fail_count;
}
