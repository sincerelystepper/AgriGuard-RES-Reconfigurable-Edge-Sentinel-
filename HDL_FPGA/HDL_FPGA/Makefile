# =============================================================================
# AgriGuard-RES FPGA Build System
# Agrionics Systems Co. | Target: iCE40HX8K-TQ144
#
# Usage:
#   make sim      — compile and run testbench (iverilog + vvp)
#   make synth    — synthesize RTL to netlist (yosys)
#   make route    — place and route (nextpnr-ice40)
#   make pack     — pack bitstream (icepack)
#   make all      — sim + synth + route + pack
#   make wave     — open last simulation waveform in GTKWave
#   make clean    — remove all build artifacts
#
# Prerequisites: OSS CAD Suite in PATH
#   Run: cmd /c "C:\oss-cad-suite\environment.bat && powershell"
#   Then: make sim
# =============================================================================

PROJ     = agriguard_top
DEVICE   = hx8k
PACKAGE  = tq144
FREQ     = 12

# --- Source files (order matters for yosys — bottom-up hierarchy) ------------
SV_SRCS  = rtl/pdm_decimator.sv \
           rtl/fft_engine.sv    \
           rtl/spi_slave.sv     \
           rtl/agriguard_top.sv

TB_TOP   = tb/tb_agriguard_top.sv
PCF      = constraints/agriguard_res.pcf
SIM_OUT  = sim/$(PROJ)_sim
VCD_OUT  = sim/$(PROJ).vcd

# =============================================================================
# SIMULATION (iverilog + vvp)
# =============================================================================
.PHONY: sim
sim: $(SIM_OUT)
	@echo ""
	@echo ">>> Running simulation..."
	vvp $(SIM_OUT)
	@echo ""
	@echo ">>> VCD written to $(VCD_OUT)"

$(SIM_OUT): $(SV_SRCS) $(TB_TOP)
	@mkdir -p sim
	@echo ">>> Compiling testbench..."
	iverilog -g2012 -Wall \
	         -o $(SIM_OUT) \
	         $(TB_TOP) $(SV_SRCS)

.PHONY: wave
wave:
	gtkwave $(VCD_OUT) &

# =============================================================================
# SYNTHESIS (yosys)
# =============================================================================
.PHONY: synth
synth: $(PROJ).json

$(PROJ).json: $(SV_SRCS)
	@echo ""
	@echo ">>> Synthesizing for iCE40HX8K..."
	yosys -p "read_verilog -sv $(SV_SRCS); \
	          synth_ice40 -top $(PROJ) \
	                      -device hx \
	                      -json $(PROJ).json \
	                      -retime" \
	      -l synth.log
	@echo ""
	@echo ">>> Synthesis complete. Log: synth.log"

# =============================================================================
# PLACE AND ROUTE (nextpnr-ice40)
# =============================================================================
.PHONY: route
route: $(PROJ).asc

$(PROJ).asc: $(PROJ).json $(PCF)
	@echo ""
	@echo ">>> Place and route..."
	nextpnr-ice40 --$(DEVICE)         \
	              --package $(PACKAGE) \
	              --json $(PROJ).json  \
	              --pcf $(PCF)         \
	              --asc $(PROJ).asc    \
	              --freq $(FREQ)       \
	              --opt-timing         \
	              --log pnr.log
	@echo ""
	@echo ">>> PnR complete. Log: pnr.log"

# =============================================================================
# BITSTREAM PACK (icepack)
# =============================================================================
.PHONY: pack
pack: $(PROJ).bin

$(PROJ).bin: $(PROJ).asc
	@echo ""
	@echo ">>> Packing bitstream..."
	icepack $(PROJ).asc $(PROJ).bin
	@echo ""
	@echo ">>> Bitstream: $(PROJ).bin"
	@echo ">>> Size:"
	@wc -c < $(PROJ).bin

# =============================================================================
# TIMING ANALYSIS (icetime)
# =============================================================================
.PHONY: timing
timing: $(PROJ).asc
	icetime -d $(DEVICE) -p $(PCF) \
	        -t -r timing.rpt $(PROJ).asc
	@cat timing.rpt

# =============================================================================
# FULL BUILD PIPELINE
# =============================================================================
.PHONY: all
all: sim synth route pack
	@echo ""
	@echo "======================================="
	@echo "  AgriGuard-RES build complete"
	@echo "  Bitstream: $(PROJ).bin"
	@echo "======================================="

# =============================================================================
# CLEAN
# =============================================================================
.PHONY: clean
clean:
	rm -f $(PROJ).json $(PROJ).asc $(PROJ).bin
	rm -f synth.log pnr.log timing.rpt
	rm -rf sim/
	@echo ">>> Clean complete"
