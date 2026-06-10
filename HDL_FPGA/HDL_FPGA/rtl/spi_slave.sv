/**
 * @file    spi_slave.sv
 * @brief   SPI Slave Controller — STM32 Host Interface
 *          AgriGuard-RES | Agrionics Systems Co.
 *          Target: Lattice iCE40HX8K-TQ144
 *
 * Protocol: SPI Mode 0 (CPOL=0, CPHA=0), MSB-first, 8-bit frames
 *           Max SCK frequency: 10 MHz (iCE40 GPIO constraint)
 *
 * Register Map (1-byte address + 1-byte data):
 *   0x00  STATUS_REG     [R]   bit0=faw_detected, bit1=frame_ready, bit7=busy
 *   0xA0  ANOMALY_REG    [R]   0x00=none, 0x01=FAW_DETECTED
 *   0xA1  THRESHOLD_LO   [W]   FAW energy threshold low byte
 *   0xA2  THRESHOLD_HI   [W]   FAW energy threshold high byte
 *   0xFF  CLEAR_IRQ      [W]   Any write clears the IRQ line and anomaly reg
 *
 * IRQ Behaviour:
 *   - fpga_irq output goes HIGH when faw_pulse fires
 *   - Stays HIGH until STM32 writes 0x01 to address 0xFF (CLEAR_IRQ)
 *   - STM32 reads 0xA0 after wakeup to determine anomaly type
 *
 * SPI Transaction Format (2 bytes):
 *   Byte 0: Address (bit7=R/W: 1=read, 0=write)
 *   Byte 1: Data (MISO for reads, MOSI for writes)
 *
 * iCE40 Note:
 *   All SPI inputs (SCK, MOSI, CSN) pass through iCE40 input registers
 *   to meet setup/hold. The SCK is NOT used as a clock — all logic runs
 *   on the system clock (clk) with SCK edge detection via double-flop sync.
 */

`timescale 1ns / 1ps
`default_nettype none

module spi_slave (
    // System clock and reset
    input  wire        clk,         // 12 MHz system clock
    input  wire        rst_n,

    // SPI interface (from STM32 SPI2 master)
    input  wire        spi_sck,     // SPI clock (max 10 MHz)
    input  wire        spi_mosi,    // Master out, slave in
    output logic       spi_miso,    // Master in, slave out
    input  wire        spi_csn,     // Chip select, active LOW

    // Internal connections to FFT engine
    input  wire        faw_detected,    // Level: FAW energy above threshold
    input  wire        faw_pulse,       // Single-cycle detection pulse

    // IRQ output to STM32 (active HIGH, latched until cleared)
    output logic       fpga_irq,

    // Threshold configuration output to FFT engine (future use)
    output logic [15:0] faw_threshold_out,
    output logic        threshold_valid
);

    // -------------------------------------------------------------------------
    // REGISTER ADDRESSES
    // -------------------------------------------------------------------------
    localparam logic [7:0] ADDR_STATUS      = 8'h00;
    localparam logic [7:0] ADDR_ANOMALY     = 8'hA0;
    localparam logic [7:0] ADDR_THRESH_LO   = 8'hA1;
    localparam logic [7:0] ADDR_THRESH_HI   = 8'hA2;
    localparam logic [7:0] ADDR_CLEAR_IRQ   = 8'hFF;

    localparam logic [7:0] ANOMALY_NONE     = 8'h00;
    localparam logic [7:0] ANOMALY_FAW      = 8'h01;

    // -------------------------------------------------------------------------
    // SCK EDGE DETECTION via 3-stage synchronizer
    // SCK runs at up to 10 MHz; system clock at 12 MHz.
    // We detect rising and falling edges of SCK in the clk domain.
    // -------------------------------------------------------------------------
    logic [2:0] sck_sync;
    logic [2:0] csn_sync;
    logic [1:0] mosi_sync;

    logic sck_rising, sck_falling;
    logic csn_active;    // CSN deglitched, active HIGH when CSN=LOW

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            sck_sync  <= 3'b111;
            csn_sync  <= 3'b111;
            mosi_sync <= 2'b00;
        end else begin
            sck_sync  <= {sck_sync[1:0],  spi_sck};
            csn_sync  <= {csn_sync[1:0],  spi_csn};
            mosi_sync <= {mosi_sync[0],   spi_mosi};
        end
    end

    assign sck_rising  = ( sck_sync[2:1] == 2'b01 );  // 0→1 transition
    assign sck_falling = ( sck_sync[2:1] == 2'b10 );  // 1→0 transition
    assign csn_active  = !csn_sync[2];                  // Active when CSN=LOW

    // -------------------------------------------------------------------------
    // SPI SHIFT REGISTER — 8-bit, MSB first
    // Shift in on SCK rising edge, shift out on SCK falling edge
    // -------------------------------------------------------------------------
    logic [7:0]  rx_shift;    // Incoming byte shift register
    logic [7:0]  tx_shift;    // Outgoing byte shift register
    logic [2:0]  bit_cnt;     // 0..7 bit counter
    logic        byte_done;   // Pulses HIGH when 8th bit clocked in

    // Byte phase: 0 = receiving address, 1 = receiving/sending data
    logic        byte_phase;
    logic [7:0]  addr_latch;  // Latched address byte
    logic        is_read;     // bit7 of address: 1=read, 0=write

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            rx_shift   <= 8'h00;
            tx_shift   <= 8'hFF;  // MISO idles HIGH
            bit_cnt    <= 3'h0;
            byte_done  <= 1'b0;
            byte_phase <= 1'b0;
            addr_latch <= 8'h00;
            is_read    <= 1'b0;
            spi_miso   <= 1'b1;
        end else begin
            byte_done <= 1'b0;

            if (!csn_active) begin
                // CS deasserted — reset state machine
                bit_cnt    <= 3'h0;
                byte_phase <= 1'b0;
                tx_shift   <= 8'hFF;
                spi_miso   <= 1'b1;
            end else begin
                // SCK rising edge — sample MOSI
                if (sck_rising) begin
                    rx_shift <= {rx_shift[6:0], mosi_sync[1]};
                    bit_cnt  <= bit_cnt + 1'b1;
                    if (bit_cnt == 3'h7) begin
                        byte_done <= 1'b1;
                    end
                end

                // SCK falling edge — shift out MISO (MSB first)
                if (sck_falling) begin
                    spi_miso <= tx_shift[7];
                    tx_shift <= {tx_shift[6:0], 1'b1};
                end
            end
        end
    end

    // -------------------------------------------------------------------------
    // REGISTER FILE
    // -------------------------------------------------------------------------
    logic [7:0]  reg_status;
    logic [7:0]  reg_anomaly;
    logic [7:0]  reg_thresh_lo;
    logic [7:0]  reg_thresh_hi;

    // IRQ latch
    logic        irq_latch;

    // Status register: [7]=busy(always 0 when readable), [1]=frame_ready, [0]=faw
    assign reg_status = {6'b000000, 1'b0, faw_detected};

    // -------------------------------------------------------------------------
    // BYTE DECODE STATE MACHINE
    // On byte_done:
    //   Phase 0 → latch address, load TX shift register with read data
    //   Phase 1 → if write, perform register write; if read, already done
    // -------------------------------------------------------------------------
    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            byte_phase     <= 1'b0;
            addr_latch     <= 8'h00;
            is_read        <= 1'b0;
            reg_anomaly    <= ANOMALY_NONE;
            reg_thresh_lo  <= 8'h00;
            reg_thresh_hi  <= 8'h08;   // Default threshold high byte
            irq_latch      <= 1'b0;
            fpga_irq       <= 1'b0;
            threshold_valid <= 1'b0;
            faw_threshold_out <= 16'h0800;
        end else begin
            threshold_valid <= 1'b0;

            // Latch FAW pulse into anomaly register and IRQ
            if (faw_pulse) begin
                reg_anomaly <= ANOMALY_FAW;
                irq_latch   <= 1'b1;
                fpga_irq    <= 1'b1;
            end

            if (byte_done && csn_active) begin
                if (!byte_phase) begin
                    // ── PHASE 0: Address byte received ──
                    addr_latch <= rx_shift;
                    is_read    <= rx_shift[7];
                    byte_phase <= 1'b1;

                    // Pre-load TX shift register for read operations
                    if (rx_shift[7]) begin
                        case (rx_shift[6:0])
                            ADDR_STATUS[6:0]:  tx_shift <= reg_status;
                            ADDR_ANOMALY[6:0]: tx_shift <= reg_anomaly;
                            default:           tx_shift <= 8'hDE; // 0xDE = undefined
                        endcase
                    end

                end else begin
                    // ── PHASE 1: Data byte received ──
                    byte_phase <= 1'b0;

                    if (!is_read) begin
                        // Write operation
                        case (addr_latch)
                            ADDR_THRESH_LO: begin
                                reg_thresh_lo <= rx_shift;
                            end
                            ADDR_THRESH_HI: begin
                                reg_thresh_hi   <= rx_shift;
                                faw_threshold_out <= {rx_shift, reg_thresh_lo};
                                threshold_valid   <= 1'b1;
                            end
                            ADDR_CLEAR_IRQ: begin
                                // Any write to 0xFF clears IRQ and anomaly register
                                irq_latch   <= 1'b0;
                                fpga_irq    <= 1'b0;
                                reg_anomaly <= ANOMALY_NONE;
                            end
                            default: ; // Ignore writes to unknown addresses
                        endcase
                    end
                    // Read: data already loaded into tx_shift in phase 0
                end
            end

            // CS deassert — clean up phase
            if (!csn_active) begin
                byte_phase <= 1'b0;
            end
        end
    end

endmodule

`default_nettype wire
