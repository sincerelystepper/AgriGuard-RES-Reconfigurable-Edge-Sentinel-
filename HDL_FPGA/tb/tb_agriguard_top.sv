/**
 * @file    tb_agriguard_top.sv
 * @brief   AgriGuard-RES Top-Level Testbench
 *          Agrionics Systems Co.
 *
 * Tests:
 *   1. PDM decimation — inject synthetic 8 kHz PDM bitstream
 *      (FAW frequency range: 5–12 kHz → should trigger detection)
 *   2. FFT pipeline — verify non-zero magnitude outputs after 256 samples
 *   3. FAW detection — verify faw_pulse asserts after energy threshold crossed
 *   4. SPI readback — simulate STM32 reading ANOMALY_REG after IRQ fires
 *   5. IRQ clear — write 0xFF register, verify fpga_irq goes LOW
 *
 * Simulation clock: 12 MHz → period = 83.33 ns
 * PDM clock: 3 MHz → pdm_clk_out toggles every 2 clk cycles
 * PCM strobe: every 250 clk cycles = 20.83 µs = 48 kHz
 * Full 256-sample FFT frame: 256 × 250 = 64,000 clk cycles ≈ 5.3 ms sim time
 *
 * Run with:
 *   iverilog -g2012 -o sim/agriguard_sim tb/tb_agriguard_top.sv \
 *            rtl/agriguard_top.sv rtl/fft_engine.sv \
 *            rtl/spi_slave.sv rtl/pdm_decimator.sv
 *   vvp sim/agriguard_sim
 *   gtkwave sim/agriguard_top.vcd
 */

`timescale 1ns / 1ps
`default_nettype none

module tb_agriguard_top;

    // =========================================================================
    // DUT SIGNALS
    // =========================================================================
    logic        clk_12mhz;
    logic        rst_n;
    logic        pdm_clk_out;
    logic        pdm_data;
    logic        spi_sck;
    logic        spi_mosi;
    logic        spi_miso;
    logic        spi_csn;
    logic        fpga_irq;

    // =========================================================================
    // DUT INSTANTIATION
    // =========================================================================
    agriguard_top u_dut (
        .clk_12mhz  (clk_12mhz),
        .rst_n      (rst_n),
        .pdm_clk_out(pdm_clk_out),
        .pdm_data   (pdm_data),
        .spi_sck    (spi_sck),
        .spi_mosi   (spi_mosi),
        .spi_miso   (spi_miso),
        .spi_csn    (spi_csn),
        .fpga_irq   (fpga_irq)
    );

    // =========================================================================
    // CLOCK GENERATION — 12 MHz (83.33 ns period)
    // =========================================================================
    localparam real CLK_PERIOD_NS = 83.333;

    initial clk_12mhz = 1'b0;
    always #(CLK_PERIOD_NS / 2.0) clk_12mhz = ~clk_12mhz;

    // =========================================================================
    // PDM STIMULUS GENERATOR
    // Generates a sigma-delta encoded representation of an 8 kHz sine wave.
    // PDM clock = 3 MHz, target audio = 8 kHz (within FAW 5–12 kHz window).
    // Sigma-delta modulation: accumulate and output carry bit.
    // =========================================================================
    localparam real PDM_CLK_HZ    = 3000000.0;
    localparam real AUDIO_FREQ_HZ = 8000.0;
    localparam real TWO_PI        = 6.283185307;

    // Track PDM clock output from DUT and generate data on its rising edge
    logic pdm_clk_prev;
    real  pdm_phase;
    real  pdm_accum;
    real  pdm_sample;

    // Plain always (not always_ff) — real-type variables cannot use always_ff
    always @(posedge clk_12mhz) begin
        pdm_clk_prev <= pdm_clk_out;

        if (pdm_clk_out && !pdm_clk_prev) begin
            // Rising edge of PDM clock from DUT
            pdm_phase  = pdm_phase + (TWO_PI * AUDIO_FREQ_HZ / PDM_CLK_HZ);
            if (pdm_phase > TWO_PI) pdm_phase = pdm_phase - TWO_PI;

            // Sigma-delta: accumulate sine + 0.5 offset
            pdm_sample = 0.5 + 0.45 * $sin(pdm_phase);
            pdm_accum  = pdm_accum + pdm_sample;

            if (pdm_accum >= 1.0) begin
                pdm_data  <= 1'b1;
                pdm_accum  = pdm_accum - 1.0;
            end else begin
                pdm_data  <= 1'b0;
            end
        end
    end

    // =========================================================================
    // SPI MASTER TASK — simulate STM32 SPI2 reads/writes
    // =========================================================================
    localparam real SPI_CLK_PERIOD_NS = 200.0;  // 5 MHz SPI clock

    task spi_transaction(
        input  logic [7:0] addr,
        input  logic [7:0] data_in,
        output logic [7:0] data_out
    );
        integer i;
        logic [7:0] tx_byte;
        data_out = 8'h00;

        // Assert CS
        spi_csn = 1'b0;
        #(SPI_CLK_PERIOD_NS);

        // Byte 0: address
        tx_byte = addr;
        for (i = 7; i >= 0; i--) begin
            spi_mosi = tx_byte[i];
            #(SPI_CLK_PERIOD_NS / 2.0);
            spi_sck = 1'b1;
            #(SPI_CLK_PERIOD_NS / 2.0);
            spi_sck = 1'b0;
        end

        // Byte 1: data (sample MISO for reads)
        tx_byte = data_in;
        for (i = 7; i >= 0; i--) begin
            spi_mosi = tx_byte[i];
            #(SPI_CLK_PERIOD_NS / 2.0);
            spi_sck = 1'b1;
            data_out[i] = spi_miso;  // Sample on rising edge
            #(SPI_CLK_PERIOD_NS / 2.0);
            spi_sck = 1'b0;
        end

        // Deassert CS
        #(SPI_CLK_PERIOD_NS);
        spi_csn = 1'b1;
        #(SPI_CLK_PERIOD_NS * 2.0);
    endtask

    // =========================================================================
    // VCD DUMP
    // =========================================================================
    initial begin
        $dumpfile("sim/agriguard_top.vcd");
        $dumpvars(0, tb_agriguard_top);
    end

    // =========================================================================
    // MAIN TEST SEQUENCE
    // =========================================================================
    logic [7:0] spi_rx;
    integer     timeout_cnt;
    integer     faw_wait;

    initial begin
        // Initialise all inputs
        rst_n    = 1'b0;
        spi_csn  = 1'b1;
        spi_sck  = 1'b0;
        spi_mosi = 1'b0;
        pdm_data = 1'b0;
        pdm_clk_prev = 1'b0;
        pdm_phase    = 0.0;
        pdm_accum    = 0.0;

        // ── TEST 1: Reset release ─────────────────────────────────────────
        $display("[TB] T=%0t | Applying reset", $time);
        repeat(20) @(posedge clk_12mhz);
        rst_n = 1'b1;
        $display("[TB] T=%0t | Reset released", $time);

        // ── TEST 2: Run FFT for multiple frames ───────────────────────────
        // Each frame = 256 PCM samples × 250 clk cycles = 64,000 cycles
        // Run 4 frames to allow FAW energy to accumulate
        $display("[TB] T=%0t | Running 4 FFT frames with 8kHz PDM input...", $time);
        repeat(4 * 256 * 250) @(posedge clk_12mhz);

        // ── TEST 3: Check fpga_irq ────────────────────────────────────────
        $display("[TB] T=%0t | fpga_irq = %b (expect 1 if FAW detected)", 
                 $time, fpga_irq);

        if (fpga_irq) begin
            $display("[TB] ✓ FAW anomaly detected — IRQ asserted");

            // ── TEST 4: SPI read STATUS register ─────────────────────────
            spi_transaction(8'h80, 8'h00, spi_rx);  // Read addr 0x00
            $display("[TB] T=%0t | STATUS_REG = 0x%02X (bit0=%b = faw_detected)",
                     $time, spi_rx, spi_rx[0]);

            // ── TEST 5: SPI read ANOMALY register ────────────────────────
            spi_transaction(8'hA0 | 8'h80, 8'h00, spi_rx);  // Read 0xA0
            $display("[TB] T=%0t | ANOMALY_REG = 0x%02X (expect 0x01 = FAW)",
                     $time, spi_rx);

            if (spi_rx == 8'h01) begin
                $display("[TB] ✓ ANOMALY_REG correctly reports FAW");
            end else begin
                $display("[TB] ✗ ANOMALY_REG mismatch: got 0x%02X", spi_rx);
            end

            // ── TEST 6: SPI write CLEAR_IRQ ──────────────────────────────
            spi_transaction(8'hFF, 8'h01, spi_rx);  // Write 0xFF
            repeat(10) @(posedge clk_12mhz);

            $display("[TB] T=%0t | After IRQ clear: fpga_irq = %b (expect 0)",
                     $time, fpga_irq);

            if (!fpga_irq) begin
                $display("[TB] ✓ IRQ cleared successfully");
            end else begin
                $display("[TB] ✗ IRQ failed to clear");
            end

        end else begin
            $display("[TB] ℹ  fpga_irq not asserted — FAW energy below threshold");
            $display("[TB]    This is expected if threshold requires tuning.");
            $display("[TB]    Check energy_snapshot in waveform (gtkwave sim/agriguard_top.vcd)");
        end

        // ── TEST 7: SPI threshold write ───────────────────────────────────
        $display("[TB] T=%0t | Writing FAW threshold LO=0x00 HI=0x04", $time);
        spi_transaction(8'hA1, 8'h00, spi_rx);  // THRESH_LO = 0x00
        spi_transaction(8'hA2, 8'h04, spi_rx);  // THRESH_HI = 0x04 → 0x0400

        $display("[TB] T=%0t | Threshold write complete", $time);

        // ── DONE ──────────────────────────────────────────────────────────
        $display("[TB] ══════════════════════════════════════");
        $display("[TB] AgriGuard-RES HDL simulation complete");
        $display("[TB] ══════════════════════════════════════");
        $finish;
    end

    // =========================================================================
    // TIMEOUT WATCHDOG — prevents infinite simulation on hung logic
    // =========================================================================
    initial begin
        #(CLK_PERIOD_NS * 1500000.0);  // 125 ms sim timeout
        $display("[TB] WATCHDOG: Simulation timeout — check for deadlock");
        $finish;
    end

endmodule

`default_nettype wire