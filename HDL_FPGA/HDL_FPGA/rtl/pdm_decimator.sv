/**
 * @file    pdm_decimator.sv
 * @brief   64:1 PDM to 16-bit PCM Decimator
 *          AgriGuard-RES | Agrionics Systems Co.
 *
 * Architecture: Simple Sinc1 (moving average) + bit growth compensation
 * Decimation ratio: 64
 * Input:  1-bit PDM @ ~3 MHz
 * Output: 16-bit PCM @ ~48 kHz (one pcm_valid pulse every 64 clocks)
 */

`timescale 1ns / 1ps
`default_nettype none

module pdm_decimator (
    input  wire        clk,
    input  wire        rst_n,
    input  wire        pdm_data,
    output logic [15:0] pcm_out,
    output logic        pcm_valid
);

    // =====================================================================
    // Accumulator + Decimation Counter
    // =====================================================================
    logic [6:0]  ones_count;    // Counts number of 1's (0-64)
    logic [5:0]  decim_cnt;     // Counts 0..63

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            ones_count  <= '0;
            decim_cnt   <= '0;
            pcm_out     <= '0;
            pcm_valid   <= 1'b0;
        end
        else begin
            pcm_valid <= 1'b0;

            // Count '1's in the PDM bitstream
            if (pdm_data)
                ones_count <= ones_count + 1'd1;

            decim_cnt <= decim_cnt + 1'b1;

            // Decimate every 64 clocks
            if (decim_cnt == 6'd63) begin
                pcm_out   <= saturate(ones_count);
                pcm_valid <= 1'b1;
                
                ones_count <= '0;
                decim_cnt  <= '0;
            end
        end
    end

    // =====================================================================
    // Scaling + Saturation
    // =====================================================================
    function automatic logic [15:0] saturate(input logic [6:0] count);
        logic signed [15:0] pcm_temp;
        
        // Map 0..64 → -32768 .. +32767 (32 ones ≈ 0)
        pcm_temp = $signed(count) * 16'sd1024 - 16'sd32768;

        // Saturate to 16-bit signed range
        if (pcm_temp > 16'sd32767)
            return 16'sd32767;
        else if (pcm_temp < -16'sd32768)
            return 16'sd32768;
        else
            return pcm_temp;
    endfunction

endmodule

`default_nettype wire