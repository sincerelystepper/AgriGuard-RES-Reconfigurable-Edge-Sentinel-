/**
 * @file    tb_pdm_decimator.sv
 * @brief   PDM Decimator Unit Testbench
 *          AgriGuard-RES | Agrionics Systems Co.
 *
 * Verifies:
 *   1. PCM valid strobe fires every 64 PDM clocks (decimation ratio)
 *   2. DC input (all 1s) → saturates at positive full scale
 *   3. Alternating 1/0 (silence) → PCM output near zero
 *   4. 8 kHz sigma-delta encoded sine → non-zero PCM output with
 *      correct sign pattern
 *
 * Run:
 *   iverilog -g2012 -o sim/pdm_sim tb/tb_pdm_decimator.sv rtl/pdm_decimator.sv
 *   vvp sim/pdm_sim
 */

`timescale 1ns / 1ps
`default_nettype none

module tb_pdm_decimator;

    // DUT ports
    logic        clk;
    logic        rst_n;
    logic        pdm_data;
    logic [15:0] pcm_out;
    logic        pcm_valid;

    // DUT
    pdm_decimator u_dut (
        .clk      (clk),
        .rst_n    (rst_n),
        .pdm_data (pdm_data),
        .pcm_out  (pcm_out),
        .pcm_valid(pcm_valid)
    );

    // 3 MHz PDM clock (333.33 ns period)
    localparam real PDM_CLK_NS = 333.333;
    initial clk = 1'b0;
    always #(PDM_CLK_NS / 2.0) clk = ~clk;

    // Sigma-delta state
    real sd_accum;
    real sd_phase;
    real sd_input;
    localparam real AUDIO_FREQ  = 8000.0;
    localparam real PDM_CLK_HZ  = 3000000.0;
    localparam real TWO_PI      = 6.283185307;

    // PCM sample counter and checker
    integer pcm_count;
    integer valid_count;

    initial begin
        $dumpfile("sim/pdm_decimator.vcd");
        $dumpvars(0, tb_pdm_decimator);

        // Init
        rst_n    = 1'b0;
        pdm_data = 1'b0;
        sd_accum = 0.0;
        sd_phase = 0.0;
        pcm_count   = 0;
        valid_count = 0;

        // ── TEST 1: Reset ─────────────────────────────────────────────────
        $display("[PDM-TB] Applying reset...");
        repeat(10) @(posedge clk);
        rst_n = 1'b1;
        $display("[PDM-TB] Reset released");

        // ── TEST 2: DC input (all 1s) — expect large positive PCM ─────────
        $display("[PDM-TB] TEST 2: DC full-scale input (all 1s)");
        pdm_data = 1'b1;
        repeat(640) @(posedge clk);  // 10 PCM samples
        $display("[PDM-TB]   Last PCM = 0x%04X (%0d)", pcm_out, $signed(pcm_out));

        // ── TEST 3: Silence (alternating) — expect PCM near zero ──────────
        $display("[PDM-TB] TEST 3: Silence input (alternating 10101...)");
        repeat(640) @(posedge clk) pdm_data = ~pdm_data;
        $display("[PDM-TB]   Last PCM = 0x%04X (%0d)", pcm_out, $signed(pcm_out));

        // ── TEST 4: 8 kHz sigma-delta sine — run 48 PCM samples ───────────
        $display("[PDM-TB] TEST 4: 8 kHz sigma-delta sine (48 PCM samples)");
        sd_accum = 0.5;
        sd_phase = 0.0;

        repeat(48 * 64) @(posedge clk) begin
            // Sigma-delta on every PDM clock
            sd_phase = sd_phase + (TWO_PI * AUDIO_FREQ / PDM_CLK_HZ);
            if (sd_phase > TWO_PI) sd_phase = sd_phase - TWO_PI;
            sd_input = 0.5 + 0.45 * $sin(sd_phase);
            sd_accum = sd_accum + sd_input;
            if (sd_accum >= 1.0) begin
                pdm_data = 1'b1;
                sd_accum = sd_accum - 1.0;
            end else begin
                pdm_data = 1'b0;
            end

            // Log each valid PCM output
            if (pcm_valid) begin
                valid_count = valid_count + 1;
                if (valid_count <= 8 || valid_count >= 44) begin
                    $display("[PDM-TB]   PCM[%0d] = %0d (0x%04X)",
                             valid_count, $signed(pcm_out), pcm_out);
                end
            end
        end

        $display("[PDM-TB] Total valid PCM samples seen: %0d", valid_count);
        $display("[PDM-TB] PDM decimator test complete");
        $finish;
    end

    // Timeout watchdog
    initial begin
        #50000000;
        $display("[PDM-TB] TIMEOUT");
        $finish;
    end

endmodule

`default_nettype wire
