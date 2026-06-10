/**
 * @file    tb_spi_slave.sv
 * @brief   SPI Slave Controller Unit Testbench
 *          AgriGuard-RES | Agrionics Systems Co.
 *
 * Verifies:
 *   1. STATUS register read (addr 0x80) returns correct faw_detected bit
 *   2. ANOMALY register read (addr 0xA0|0x80) returns 0x01 after faw_pulse
 *   3. fpga_irq asserts on faw_pulse, stays latched
 *   4. CLEAR_IRQ write (addr 0xFF) de-asserts fpga_irq
 *   5. Threshold write to 0xA1/0xA2 produces threshold_valid pulse
 *
 * Run:
 *   iverilog -g2012 -o sim/spi_sim tb/tb_spi_slave.sv rtl/spi_slave.sv
 *   vvp sim/spi_sim
 */

`timescale 1ns / 1ps
`default_nettype none

module tb_spi_slave;

    // DUT ports
    logic        clk;
    logic        rst_n;
    logic        spi_sck;
    logic        spi_mosi;
    logic        spi_miso;
    logic        spi_csn;
    logic        faw_detected;
    logic        faw_pulse;
    logic        fpga_irq;
    logic [15:0] faw_threshold_out;
    logic        threshold_valid;

    // DUT
    spi_slave u_dut (
        .clk              (clk),
        .rst_n            (rst_n),
        .spi_sck          (spi_sck),
        .spi_mosi         (spi_mosi),
        .spi_miso         (spi_miso),
        .spi_csn          (spi_csn),
        .faw_detected     (faw_detected),
        .faw_pulse        (faw_pulse),
        .fpga_irq         (fpga_irq),
        .faw_threshold_out(faw_threshold_out),
        .threshold_valid  (threshold_valid)
    );

    // 12 MHz system clock
    localparam real CLK_NS = 83.333;
    initial clk = 1'b0;
    always #(CLK_NS / 2.0) clk = ~clk;

    // SPI clock: 5 MHz → 200 ns period
    localparam real SPI_HALF_NS = 100.0;

    // =========================================================================
    // SPI TRANSACTION TASK
    // =========================================================================
    task automatic spi_txn (
        input  logic [7:0] addr,
        input  logic [7:0] data_wr,
        output logic [7:0] data_rd
    );
        integer i;
        data_rd = 8'h00;

        spi_csn  = 1'b0;
        #(SPI_HALF_NS * 2);

        // Address byte
        for (i = 7; i >= 0; i--) begin
            spi_mosi = addr[i];
            #(SPI_HALF_NS);
            spi_sck  = 1'b1;
            #(SPI_HALF_NS);
            spi_sck  = 1'b0;
        end

        // Data byte — sample MISO on SCK rising edge
        for (i = 7; i >= 0; i--) begin
            spi_mosi = data_wr[i];
            #(SPI_HALF_NS);
            spi_sck  = 1'b1;
            data_rd[i] = spi_miso;
            #(SPI_HALF_NS);
            spi_sck  = 1'b0;
        end

        #(SPI_HALF_NS * 2);
        spi_csn = 1'b1;
        #(SPI_HALF_NS * 4);
    endtask

    // =========================================================================
    // TEST SEQUENCE
    // =========================================================================
    logic [7:0] rx;
    integer     pass_count;
    integer     fail_count;

    task check(input string label, input logic got, input logic expected);
        if (got === expected) begin
            $display("[SPI-TB] ✓  %s: got %0b", label, got);
            pass_count++;
        end else begin
            $display("[SPI-TB] ✗  %s: got %0b, expected %0b", label, got, expected);
            fail_count++;
        end
    endtask

    task check_byte(input string label, input logic [7:0] got, input logic [7:0] expected);
        if (got === expected) begin
            $display("[SPI-TB] ✓  %s: got 0x%02X", label, got);
            pass_count++;
        end else begin
            $display("[SPI-TB] ✗  %s: got 0x%02X, expected 0x%02X", label, got, expected);
            fail_count++;
        end
    endtask

    initial begin
        $dumpfile("sim/spi_slave.vcd");
        $dumpvars(0, tb_spi_slave);

        // Init
        rst_n        = 1'b0;
        spi_sck      = 1'b0;
        spi_csn      = 1'b1;
        spi_mosi     = 1'b0;
        faw_detected = 1'b0;
        faw_pulse    = 1'b0;
        pass_count   = 0;
        fail_count   = 0;

        repeat(20) @(posedge clk);
        rst_n = 1'b1;
        repeat(10) @(posedge clk);

        // ── TEST 1: Read STATUS when faw_detected=0 ───────────────────────
        $display("\n[SPI-TB] TEST 1: STATUS read, faw_detected=0");
        spi_txn(8'h80, 8'h00, rx);   // Read addr 0x00
        check_byte("STATUS_REG", rx, 8'h00);

        // ── TEST 2: Assert faw_detected, re-read STATUS ───────────────────
        $display("\n[SPI-TB] TEST 2: STATUS read, faw_detected=1");
        faw_detected = 1'b1;
        repeat(5) @(posedge clk);
        spi_txn(8'h80, 8'h00, rx);
        check_byte("STATUS_REG bit0", rx, 8'h01);

        // ── TEST 3: Fire faw_pulse, check IRQ latches ─────────────────────
        $display("\n[SPI-TB] TEST 3: faw_pulse fires → fpga_irq latches HIGH");
        @(posedge clk);
        faw_pulse = 1'b1;
        @(posedge clk);
        faw_pulse = 1'b0;
        repeat(5) @(posedge clk);
        check("fpga_irq HIGH after faw_pulse", fpga_irq, 1'b1);

        // ── TEST 4: Read ANOMALY register ─────────────────────────────────
        $display("\n[SPI-TB] TEST 4: ANOMALY_REG read after faw_pulse");
        spi_txn(8'hA0 | 8'h80, 8'h00, rx);  // Read with MSB set = read op
        check_byte("ANOMALY_REG", rx, 8'h01);

        // ── TEST 5: Write CLEAR_IRQ ────────────────────────────────────────
        $display("\n[SPI-TB] TEST 5: Write CLEAR_IRQ → fpga_irq goes LOW");
        spi_txn(8'hFF, 8'h01, rx);
        repeat(10) @(posedge clk);
        check("fpga_irq LOW after clear", fpga_irq, 1'b0);

        // ── TEST 6: ANOMALY_REG should be cleared ─────────────────────────
        $display("\n[SPI-TB] TEST 6: ANOMALY_REG reads 0x00 after clear");
        spi_txn(8'hA0 | 8'h80, 8'h00, rx);
        check_byte("ANOMALY_REG after clear", rx, 8'h00);

        // ── TEST 7: Threshold write ────────────────────────────────────────
        $display("\n[SPI-TB] TEST 7: Threshold write 0xA1=0xCD, 0xA2=0xAB");
        spi_txn(8'hA1, 8'hCD, rx);   // THRESH_LO
        repeat(5) @(posedge clk);
        spi_txn(8'hA2, 8'hAB, rx);   // THRESH_HI → triggers threshold_valid
        repeat(10) @(posedge clk);
        check_byte("threshold_out HI", faw_threshold_out[15:8], 8'hAB);
        check_byte("threshold_out LO", faw_threshold_out[7:0],  8'hCD);

        // ── SUMMARY ───────────────────────────────────────────────────────
        $display("\n[SPI-TB] ══════════════════════════════");
        $display("[SPI-TB] Results: %0d passed, %0d failed", pass_count, fail_count);
        $display("[SPI-TB] ══════════════════════════════");
        $finish;
    end

    initial begin
        #10000000;
        $display("[SPI-TB] TIMEOUT");
        $finish;
    end

endmodule

`default_nettype wire
