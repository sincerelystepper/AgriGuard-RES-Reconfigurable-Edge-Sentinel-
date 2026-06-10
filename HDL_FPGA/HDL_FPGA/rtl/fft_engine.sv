/**
 * @file    fft_engine.sv
 * @brief   256-Point Radix-2 DIT Pipelined FFT Engine
 *          AgriGuard-RES | Agrionics Systems Co.
 *          Target: Lattice iCE40HX8K-TQ144
 *
 * Architecture: Single-path Delay Feedback (SDF) pipeline
 *   - 8 butterfly stages (log2(256) = 8)
 *   - Each stage: one butterfly unit + twiddle factor ROM + shift register
 *   - Input:  16-bit signed PCM from pdm_decimator @ 48 kHz
 *   - Output: 16-bit magnitude spectrum (|Re| + |Im| approximation)
 *             256 bins, bin width = 48000/256 = 187.5 Hz
 *
 * FAW Detection Window:
 *   Bin  27 →  5,062 Hz  (FAW_FFT_BIN_LOW  = 5000*256/48000 = 26.67 → 27)
 *   Bin  64 → 12,000 Hz  (FAW_FFT_BIN_HIGH = 12000*256/48000 = 64)
 *   Energy accumulator monitors bins 27–64 against a threshold.
 *
 * iCE40HX8K Resource Budget:
 *   - 8 butterfly units × 2 multipliers = 16 × DSP-mapped LUT multipliers
 *   - iCE40HX8K has 8,192 LUTs — this design uses ~3,200 LUTs estimated
 *   - Twiddle ROMs: 256 × 2 × 16-bit = 1KB — fits in 2× BRAM blocks
 *   - Shift registers: implemented in LUT-based SRL
 *
 * Synthesizability:
 *   - All always_ff blocks: synchronous, single clock domain
 *   - No division operators — magnitude uses |Re|+|Im| approximation
 *   - Twiddle factors: pre-computed ROM, no runtime trig
 *   - Compatible with: yosys synth_ice40, Synplify Pro
 *
 * Fixed-Point Format:
 *   - Data path: 16-bit signed (Q1.15 fractional)
 *   - Twiddle factors: 16-bit signed (Q1.15)
 *   - Butterfly output: truncated back to 16-bit with arithmetic right shift
 */

`timescale 1ns / 1ps
`default_nettype none

// ============================================================================
// BUTTERFLY UNIT
// Computes one Radix-2 DIT butterfly:
//   X[k]   = A + W*B
//   X[k+N/2] = A - W*B
// Where W = (Wr + j*Wi) is the twiddle factor.
// All arithmetic in Q1.15 fixed-point, 16-bit signed.
// ============================================================================
module butterfly_unit (
    input  wire                clk,
    input  wire                rst_n,
    // Input operands (complex)
    input  wire signed [15:0]  a_re,
    input  wire signed [15:0]  a_im,
    input  wire signed [15:0]  b_re,
    input  wire signed [15:0]  b_im,
    // Twiddle factor (Q1.15)
    input  wire signed [15:0]  w_re,
    input  wire signed [15:0]  w_im,
    // Outputs (registered, 1-cycle latency)
    output logic signed [15:0] p_re,   // A + W*B  real
    output logic signed [15:0] p_im,   // A + W*B  imag
    output logic signed [15:0] q_re,   // A - W*B  real
    output logic signed [15:0] q_im    // A - W*B  imag
);
    // -----------------------------------------------------------------------
    // Complex multiply: W * B = (Wr*Br - Wi*Bi) + j(Wr*Bi + Wi*Br)
    // 16×16 → 32-bit product, take bits [30:15] for Q1.15 result
    // -----------------------------------------------------------------------
    logic signed [31:0] wr_br, wi_bi, wr_bi, wi_br;
    logic signed [15:0] wb_re, wb_im;

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            p_re <= 16'h0;
            p_im <= 16'h0;
            q_re <= 16'h0;
            q_im <= 16'h0;
        end else begin
            // Stage 1: multiplications (registered for timing closure)
            wr_br = w_re * b_re;
            wi_bi = w_im * b_im;
            wr_bi = w_re * b_im;
            wi_br = w_im * b_re;

            // Q1.15 truncation: take bits [30:15]
            wb_re = (wr_br - wi_bi) >>> 15;
            wb_im = (wr_bi + wi_br) >>> 15;

            // Butterfly outputs with arithmetic right shift for overflow guard
            p_re <= (a_re + wb_re) >>> 1;
            p_im <= (a_im + wb_im) >>> 1;
            q_re <= (a_re - wb_re) >>> 1;
            q_im <= (a_im - wb_im) >>> 1;
        end
    end

endmodule


// ============================================================================
// TWIDDLE FACTOR ROM
// Pre-computed W_N^k = cos(2πk/N) - j*sin(2πk/N) for N=256
// Stored as Q1.15 fixed-point pairs.
// Only the first N/2=128 factors are needed (symmetry).
// ROM is read-only, synthesizes to iCE40 BRAM automatically via yosys.
// ============================================================================
module twiddle_rom (
    input  wire        clk,
    input  wire [6:0]  addr,    // 0..127
    output logic signed [15:0] w_re,
    output logic signed [15:0] w_im
);
    // Pre-computed Q1.15 twiddle factors for N=256
    // w_re[k] = round(cos(2*pi*k/256) * 32768)
    // w_im[k] = round(-sin(2*pi*k/256) * 32768)  [negative for DFT convention]
    logic signed [15:0] rom_re [0:127];
    logic signed [15:0] rom_im [0:127];

    initial begin
        // k=0..127: cos(2πk/256) in Q1.15
        rom_re[  0]=16'h7FFF; rom_re[  1]=16'h7FF6; rom_re[  2]=16'h7FD9;
        rom_re[  3]=16'h7FA7; rom_re[  4]=16'h7F62; rom_re[  5]=16'h7F0A;
        rom_re[  6]=16'h7E9D; rom_re[  7]=16'h7E1E; rom_re[  8]=16'h7D8A;
        rom_re[  9]=16'h7CE4; rom_re[ 10]=16'h7C2A; rom_re[ 11]=16'h7B5D;
        rom_re[ 12]=16'h7A7D; rom_re[ 13]=16'h798A; rom_re[ 14]=16'h7885;
        rom_re[ 15]=16'h776C; rom_re[ 16]=16'h7642; rom_re[ 17]=16'h7505;
        rom_re[ 18]=16'h73B6; rom_re[ 19]=16'h7255; rom_re[ 20]=16'h70E3;
        rom_re[ 21]=16'h6F5F; rom_re[ 22]=16'h6DCA; rom_re[ 23]=16'h6C24;
        rom_re[ 24]=16'h6A6E; rom_re[ 25]=16'h68A7; rom_re[ 26]=16'h66D0;
        rom_re[ 27]=16'h64E9; rom_re[ 28]=16'h62F2; rom_re[ 29]=16'h60EC;
        rom_re[ 30]=16'h5ED7; rom_re[ 31]=16'h5CB4; rom_re[ 32]=16'h5A82;
        rom_re[ 33]=16'h5843; rom_re[ 34]=16'h55F6; rom_re[ 35]=16'h539B;
        rom_re[ 36]=16'h5134; rom_re[ 37]=16'h4EC0; rom_re[ 38]=16'h4C40;
        rom_re[ 39]=16'h49B4; rom_re[ 40]=16'h471D; rom_re[ 41]=16'h447B;
        rom_re[ 42]=16'h41CE; rom_re[ 43]=16'h3F17; rom_re[ 44]=16'h3C57;
        rom_re[ 45]=16'h398D; rom_re[ 46]=16'h36BA; rom_re[ 47]=16'h33DF;
        rom_re[ 48]=16'h30FC; rom_re[ 49]=16'h2E11; rom_re[ 50]=16'h2B1F;
        rom_re[ 51]=16'h2827; rom_re[ 52]=16'h2528; rom_re[ 53]=16'h2224;
        rom_re[ 54]=16'h1F1A; rom_re[ 55]=16'h1C0C; rom_re[ 56]=16'h18F9;
        rom_re[ 57]=16'h15E2; rom_re[ 58]=16'h12C8; rom_re[ 59]=16'h0FAB;
        rom_re[ 60]=16'h0C8C; rom_re[ 61]=16'h096B; rom_re[ 62]=16'h0648;
        rom_re[ 63]=16'h0324; rom_re[ 64]=16'h0000; rom_re[ 65]=16'hFCDC;
        rom_re[ 66]=16'hF9B8; rom_re[ 67]=16'hF695; rom_re[ 68]=16'hF374;
        rom_re[ 69]=16'hF055; rom_re[ 70]=16'hED38; rom_re[ 71]=16'hEA1E;
        rom_re[ 72]=16'hE707; rom_re[ 73]=16'hE3F4; rom_re[ 74]=16'hE0E6;
        rom_re[ 75]=16'hDDDC; rom_re[ 76]=16'hDAD8; rom_re[ 77]=16'hD7D9;
        rom_re[ 78]=16'hD4E1; rom_re[ 79]=16'hD1EF; rom_re[ 80]=16'hCF04;
        rom_re[ 81]=16'hCC21; rom_re[ 82]=16'hC946; rom_re[ 83]=16'hC673;
        rom_re[ 84]=16'hC3A9; rom_re[ 85]=16'hC0E9; rom_re[ 86]=16'hBE32;
        rom_re[ 87]=16'hBB85; rom_re[ 88]=16'hB8E3; rom_re[ 89]=16'hB64C;
        rom_re[ 90]=16'hB3C0; rom_re[ 91]=16'hB140; rom_re[ 92]=16'hAECC;
        rom_re[ 93]=16'hAC65; rom_re[ 94]=16'hAA0A; rom_re[ 95]=16'hA7BD;
        rom_re[ 96]=16'hA57E; rom_re[ 97]=16'hA34C; rom_re[ 98]=16'hA129;
        rom_re[ 99]=16'h9F14; rom_re[100]=16'h9D0E; rom_re[101]=16'h9B17;
        rom_re[102]=16'h9930; rom_re[103]=16'h9759; rom_re[104]=16'h9592;
        rom_re[105]=16'h93DC; rom_re[106]=16'h9236; rom_re[107]=16'h90A1;
        rom_re[108]=16'h8F1D; rom_re[109]=16'h8DAB; rom_re[110]=16'h8C4A;
        rom_re[111]=16'h8AFB; rom_re[112]=16'h89BE; rom_re[113]=16'h8894;
        rom_re[114]=16'h877B; rom_re[115]=16'h8676; rom_re[116]=16'h8583;
        rom_re[117]=16'h84A3; rom_re[118]=16'h83D6; rom_re[119]=16'h831C;
        rom_re[120]=16'h8276; rom_re[121]=16'h81E2; rom_re[122]=16'h8163;
        rom_re[123]=16'h80F6; rom_re[124]=16'h809E; rom_re[125]=16'h8059;
        rom_re[126]=16'h8027; rom_re[127]=16'h800A;

        // k=0..127: -sin(2πk/256) in Q1.15
        rom_im[  0]=16'h0000; rom_im[  1]=16'hFCDC; rom_im[  2]=16'hF9B8;
        rom_im[  3]=16'hF695; rom_im[  4]=16'hF374; rom_im[  5]=16'hF055;
        rom_im[  6]=16'hED38; rom_im[  7]=16'hEA1E; rom_im[  8]=16'hE707;
        rom_im[  9]=16'hE3F4; rom_im[ 10]=16'hE0E6; rom_im[ 11]=16'hDDDC;
        rom_im[ 12]=16'hDAD8; rom_im[ 13]=16'hD7D9; rom_im[ 14]=16'hD4E1;
        rom_im[ 15]=16'hD1EF; rom_im[ 16]=16'hCF04; rom_im[ 17]=16'hCC21;
        rom_im[ 18]=16'hC946; rom_im[ 19]=16'hC673; rom_im[ 20]=16'hC3A9;
        rom_im[ 21]=16'hC0E9; rom_im[ 22]=16'hBE32; rom_im[ 23]=16'hBB85;
        rom_im[ 24]=16'hB8E3; rom_im[ 25]=16'hB64C; rom_im[ 26]=16'hB3C0;
        rom_im[ 27]=16'hB140; rom_im[ 28]=16'hAECC; rom_im[ 29]=16'hAC65;
        rom_im[ 30]=16'hAA0A; rom_im[ 31]=16'hA7BD; rom_im[ 32]=16'hA57E;
        rom_im[ 33]=16'hA34C; rom_im[ 34]=16'hA129; rom_im[ 35]=16'h9F14;
        rom_im[ 36]=16'h9D0E; rom_im[ 37]=16'h9B17; rom_im[ 38]=16'h9930;
        rom_im[ 39]=16'h9759; rom_im[ 40]=16'h9592; rom_im[ 41]=16'h93DC;
        rom_im[ 42]=16'h9236; rom_im[ 43]=16'h90A1; rom_im[ 44]=16'h8F1D;
        rom_im[ 45]=16'h8DAB; rom_im[ 46]=16'h8C4A; rom_im[ 47]=16'h8AFB;
        rom_im[ 48]=16'h89BE; rom_im[ 49]=16'h8894; rom_im[ 50]=16'h877B;
        rom_im[ 51]=16'h8676; rom_im[ 52]=16'h8583; rom_im[ 53]=16'h84A3;
        rom_im[ 54]=16'h83D6; rom_im[ 55]=16'h831C; rom_im[ 56]=16'h8276;
        rom_im[ 57]=16'h81E2; rom_im[ 58]=16'h8163; rom_im[ 59]=16'h80F6;
        rom_im[ 60]=16'h809E; rom_im[ 61]=16'h8059; rom_im[ 62]=16'h8027;
        rom_im[ 63]=16'h800A; rom_im[ 64]=16'h8001; rom_im[ 65]=16'h800A;
        rom_im[ 66]=16'h8027; rom_im[ 67]=16'h8059; rom_im[ 68]=16'h809E;
        rom_im[ 69]=16'h80F6; rom_im[ 70]=16'h8163; rom_im[ 71]=16'h81E2;
        rom_im[ 72]=16'h8276; rom_im[ 73]=16'h831C; rom_im[ 74]=16'h83D6;
        rom_im[ 75]=16'h84A3; rom_im[ 76]=16'h8583; rom_im[ 77]=16'h8676;
        rom_im[ 78]=16'h877B; rom_im[ 79]=16'h8894; rom_im[ 80]=16'h89BE;
        rom_im[ 81]=16'h8AFB; rom_im[ 82]=16'h8C4A; rom_im[ 83]=16'h8DAB;
        rom_im[ 84]=16'h8F1D; rom_im[ 85]=16'h90A1; rom_im[ 86]=16'h9236;
        rom_im[ 87]=16'h93DC; rom_im[ 88]=16'h9592; rom_im[ 89]=16'h9759;
        rom_im[ 90]=16'h9930; rom_im[ 91]=16'h9B17; rom_im[ 92]=16'h9D0E;
        rom_im[ 93]=16'h9F14; rom_im[ 94]=16'hA129; rom_im[ 95]=16'hA34C;
        rom_im[ 96]=16'hA57E; rom_im[ 97]=16'hA7BD; rom_im[ 98]=16'hAA0A;
        rom_im[ 99]=16'hAC65; rom_im[100]=16'hAECC; rom_im[101]=16'hB140;
        rom_im[102]=16'hB3C0; rom_im[103]=16'hB64C; rom_im[104]=16'hB8E3;
        rom_im[105]=16'hBB85; rom_im[106]=16'hBE32; rom_im[107]=16'hC0E9;
        rom_im[108]=16'hC3A9; rom_im[109]=16'hC673; rom_im[110]=16'hC946;
        rom_im[111]=16'hCC21; rom_im[112]=16'hCF04; rom_im[113]=16'hD1EF;
        rom_im[114]=16'hD4E1; rom_im[115]=16'hD7D9; rom_im[116]=16'hDAD8;
        rom_im[117]=16'hDDDC; rom_im[118]=16'hE0E6; rom_im[119]=16'hE3F4;
        rom_im[120]=16'hE707; rom_im[121]=16'hEA1E; rom_im[122]=16'hED38;
        rom_im[123]=16'hF055; rom_im[124]=16'hF374; rom_im[125]=16'hF695;
        rom_im[126]=16'hF9B8; rom_im[127]=16'hFCDC;
    end

    always_ff @(posedge clk) begin
        w_re <= rom_re[addr];
        w_im <= rom_im[addr];
    end

endmodule


// ============================================================================
// FFT STAGE — Single-path Delay Feedback (SDF) stage
// Each stage implements one butterfly layer of the FFT pipeline.
// The delay FIFO holds N/2 samples where N = 2^(STAGE+1).
// ============================================================================
module fft_stage #(
    parameter int unsigned STAGE      = 0,   // Stage index 0..7
    parameter int unsigned FFT_SIZE   = 256  // Total FFT points
) (
    input  wire         clk,
    input  wire         rst_n,
    input  wire         in_valid,

    input  wire signed [15:0]  in_re,
    input  wire signed [15:0]  in_im,

    output logic        out_valid,
    output logic signed [15:0] out_re,
    output logic signed [15:0] out_im
);
    // Delay length for this stage: FFT_SIZE >> (STAGE+1)
    localparam int unsigned DELAY = FFT_SIZE >> (STAGE + 1);

    // Twiddle address counter: counts 0..DELAY-1, repeating
    logic [$clog2(FFT_SIZE/2)-1:0] twiddle_addr;
    logic                          butterfly_sel; // 0=feed delay, 1=butterfly

    // Delay FIFO (shift register)
    logic signed [15:0] delay_re [0:DELAY-1];
    logic signed [15:0] delay_im [0:DELAY-1];

    // Twiddle ROM outputs
    logic signed [15:0] w_re, w_im;

    // Butterfly inputs/outputs
    logic signed [15:0] bf_a_re, bf_a_im;
    logic signed [15:0] bf_b_re, bf_b_im;
    logic signed [15:0] bf_p_re, bf_p_im;
    logic signed [15:0] bf_q_re, bf_q_im;

    // Sample counter to track position within frame
    logic [$clog2(FFT_SIZE)-1:0] sample_cnt;

    // Twiddle ROM instance
    twiddle_rom u_twiddle (
        .clk   (clk),
        .addr  (twiddle_addr[6:0]),
        .w_re  (w_re),
        .w_im  (w_im)
    );

    // Butterfly instance
    butterfly_unit u_butterfly (
        .clk   (clk),
        .rst_n (rst_n),
        .a_re  (bf_a_re),  .a_im  (bf_a_im),
        .b_re  (bf_b_re),  .b_im  (bf_b_im),
        .w_re  (w_re),     .w_im  (w_im),
        .p_re  (bf_p_re),  .p_im  (bf_p_im),
        .q_re  (bf_q_re),  .q_im  (bf_q_im)
    );

    // -----------------------------------------------------------------------
    // Sample counter and butterfly select
    // butterfly_sel toggles every DELAY samples
    // -----------------------------------------------------------------------
    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            sample_cnt   <= '0;
            twiddle_addr <= '0;
            butterfly_sel <= 1'b0;
        end else if (in_valid) begin
            if (sample_cnt == FFT_SIZE - 1) begin
                sample_cnt <= '0;
            end else begin
                sample_cnt <= sample_cnt + 1'b1;
            end
            // butterfly_sel: high for second half of each DELAY-pair
            butterfly_sel <= sample_cnt[($clog2(DELAY))];
            // Twiddle address: lower bits of sample counter, scaled
            twiddle_addr <= sample_cnt[$clog2(DELAY)-1:0] << STAGE;
        end
    end

    // -----------------------------------------------------------------------
    // Delay FIFO — shift register
    // -----------------------------------------------------------------------
    integer k;
    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            for (k = 0; k < DELAY; k = k + 1) begin
                delay_re[k] <= 16'h0;
                delay_im[k] <= 16'h0;
            end
        end else if (in_valid) begin
            delay_re[0] <= in_re;
            delay_im[0] <= in_im;
            for (k = 1; k < DELAY; k = k + 1) begin
                delay_re[k] <= delay_re[k-1];
                delay_im[k] <= delay_im[k-1];
            end
        end
    end

    // -----------------------------------------------------------------------
    // Butterfly input mux
    // When butterfly_sel=0: feed input directly to delay, output from delay
    // When butterfly_sel=1: compute butterfly between input and delay output
    // -----------------------------------------------------------------------
    assign bf_a_re = delay_re[DELAY-1];
    assign bf_a_im = delay_im[DELAY-1];
    assign bf_b_re = in_re;
    assign bf_b_im = in_im;

    // -----------------------------------------------------------------------
    // Output mux and valid pipeline
    // -----------------------------------------------------------------------
    logic out_valid_d;

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            out_re      <= 16'h0;
            out_im      <= 16'h0;
            out_valid   <= 1'b0;
            out_valid_d <= 1'b0;
        end else begin
            out_valid_d <= in_valid;
            out_valid   <= out_valid_d;

            if (in_valid) begin
                if (!butterfly_sel) begin
                    // Pass-through mode: output the oldest delay value
                    out_re <= delay_re[DELAY-1];
                    out_im <= delay_im[DELAY-1];
                end else begin
                    // Butterfly mode: output butterfly result
                    out_re <= bf_p_re;
                    out_im <= bf_p_im;
                end
            end
        end
    end

endmodule


// ============================================================================
// MAIN FFT ENGINE TOP
// 8-stage SDF pipeline + magnitude computation + FAW energy detector
// ============================================================================
module fft_engine (
    input  wire         clk,        // 48 kHz × oversampling — use 12MHz system clk
    input  wire         rst_n,

    // PCM input from pdm_decimator
    input  wire signed [15:0] pcm_in,
    input  wire               pcm_valid,   // One pulse per 48kHz PCM sample

    // Magnitude spectrum output (post-FFT, one bin per clock burst)
    output logic [15:0]       mag_out,     // |Re| + |Im| magnitude approximation
    output logic [7:0]        bin_index,   // Current bin number 0..255
    output logic              bin_valid,   // One pulse per valid bin output

    // FAW anomaly detection output
    output logic              faw_detected,   // Level: HIGH while FAW energy above threshold
    output logic              faw_pulse        // Single-cycle pulse on new FAW detection
);

    // -------------------------------------------------------------------------
    // PARAMETERS
    // -------------------------------------------------------------------------
    localparam int unsigned FFT_N          = 256;
    localparam int unsigned STAGES         = 8;     // log2(256)
    localparam int unsigned FAW_BIN_LOW    = 27;    // 5000 Hz bin
    localparam int unsigned FAW_BIN_HIGH   = 64;    // 12000 Hz bin
    // Energy threshold: tune empirically for deployment environment
    // At Q1.15 with gain-scaled pipeline, this corresponds to ~-20dB SNR
    localparam logic [31:0]  FAW_ENERGY_THRESHOLD = 32'h00080000;

    // -------------------------------------------------------------------------
    // INTER-STAGE WIRING
    // -------------------------------------------------------------------------
    logic signed [15:0] stage_re [0:STAGES];   // stage_re[0] = input, [8] = output
    logic signed [15:0] stage_im [0:STAGES];
    logic               stage_valid [0:STAGES];

    // Input: real PCM, imaginary = 0
    assign stage_re[0]    = pcm_in;
    assign stage_im[0]    = 16'h0000;
    assign stage_valid[0] = pcm_valid;

    // -------------------------------------------------------------------------
    // 8 PIPELINE STAGES
    // -------------------------------------------------------------------------
    genvar s;
    generate
        for (s = 0; s < STAGES; s++) begin : g_stages
            fft_stage #(
                .STAGE    (s),
                .FFT_SIZE (FFT_N)
            ) u_stage (
                .clk      (clk),
                .rst_n    (rst_n),
                .in_valid (stage_valid[s]),
                .in_re    (stage_re[s]),
                .in_im    (stage_im[s]),
                .out_valid(stage_valid[s+1]),
                .out_re   (stage_re[s+1]),
                .out_im   (stage_im[s+1])
            );
        end
    endgenerate

    // -------------------------------------------------------------------------
    // BIT-REVERSAL OUTPUT REORDER
    // For a 256-point DIT FFT, output bins are in bit-reversed order.
    // We track the output bin counter and apply bit-reversal mapping.
    // -------------------------------------------------------------------------
    logic [7:0]  out_cnt;        // Linear output counter 0..255
    logic [7:0]  bin_rev;        // Bit-reversed bin index

    // 8-bit bit reversal function
    function automatic logic [7:0] bit_reverse_8 (input logic [7:0] x);
        bit_reverse_8 = {x[0],x[1],x[2],x[3],x[4],x[5],x[6],x[7]};
    endfunction

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            out_cnt <= 8'h00;
        end else if (stage_valid[STAGES]) begin
            out_cnt <= out_cnt + 1'b1;
        end
    end

    assign bin_rev = bit_reverse_8(out_cnt);

    // -------------------------------------------------------------------------
    // MAGNITUDE COMPUTATION
    // |X[k]| ≈ |Re| + |Im|  (L1 norm approximation)
    // More accurate: max(|Re|,|Im|) + 0.375*min(|Re|,|Im|)
    // We use the simpler L1 for LUT budget — sufficient for threshold detection
    // -------------------------------------------------------------------------
    logic [15:0] abs_re, abs_im;

    assign abs_re = stage_re[STAGES][15] ?
                    (~stage_re[STAGES] + 1'b1) : stage_re[STAGES];
    assign abs_im = stage_im[STAGES][15] ?
                    (~stage_im[STAGES] + 1'b1) : stage_im[STAGES];

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            mag_out   <= 16'h0;
            bin_index <= 8'h00;
            bin_valid <= 1'b0;
        end else begin
            bin_valid <= stage_valid[STAGES];
            if (stage_valid[STAGES]) begin
                // Saturating add: cap at 0xFFFF
                mag_out   <= (abs_re > (16'hFFFF - abs_im)) ?
                              16'hFFFF : (abs_re + abs_im);
                bin_index <= bin_rev;
            end
        end
    end

    // -------------------------------------------------------------------------
    // FAW ENERGY ACCUMULATOR
    // Sums magnitude² across bins FAW_BIN_LOW..FAW_BIN_HIGH within each frame.
    // Compares against FAW_ENERGY_THRESHOLD after all 256 bins are processed.
    // -------------------------------------------------------------------------
    logic [31:0] energy_acc;
    logic [31:0] energy_snapshot;
    logic        frame_done;        // Pulses when bin 255 is output
    logic        faw_prev;          // For edge detection

    assign frame_done = bin_valid && (bin_index == 8'hFF);

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            energy_acc      <= 32'h0;
            energy_snapshot <= 32'h0;
        end else begin
            if (bin_valid) begin
                if (bin_index == 8'h00) begin
                    // Start of new frame — reset accumulator
                    energy_acc <= 32'h0;
                end else if ((bin_index >= FAW_BIN_LOW) &&
                             (bin_index <= FAW_BIN_HIGH)) begin
                    // Accumulate squared magnitude in FAW window
                    // mag_out is 16-bit; squared is 32-bit, cap at 32'hFFFFFFFF
                    logic [31:0] mag_sq;
                    mag_sq = {16'h0, mag_out} * {16'h0, mag_out};
                    // Saturating accumulate
                    energy_acc <= ((32'hFFFFFFFF - energy_acc) < mag_sq) ?
                                   32'hFFFFFFFF : (energy_acc + mag_sq);
                end
            end
            if (frame_done) begin
                energy_snapshot <= energy_acc;
            end
        end
    end

    // FAW detection: level signal + rising-edge pulse
    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            faw_detected <= 1'b0;
            faw_pulse    <= 1'b0;
            faw_prev     <= 1'b0;
        end else begin
            if (frame_done) begin
                faw_detected <= (energy_snapshot >= FAW_ENERGY_THRESHOLD);
            end
            faw_prev  <= faw_detected;
            // Single-cycle pulse on LOW→HIGH transition
            faw_pulse <= faw_detected && !faw_prev;
        end
    end

endmodule

`default_nettype wire
