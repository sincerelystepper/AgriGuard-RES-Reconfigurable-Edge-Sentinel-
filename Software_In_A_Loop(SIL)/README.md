# Software-in-the-Loop (SIL) Verification
## STM32H743IIT6 ↔ iCE40HX8K-BG121 Co-Processor Protocol Validation

<br>

![SIL](https://img.shields.io/badge/SIL-Verilator_5.x-2E7D32?style=for-the-badge&logo=verilog&logoColor=white)
![Status](https://img.shields.io/badge/Protocol-3/4_PASS-00C853?style=for-the-badge)
![Platform](https://img.shields.io/badge/Host-Windows_MSYS2-0078D4?style=for-the-badge)
![VCD](https://img.shields.io/badge/Waveform-GTKWave-6A0DAD?style=for-the-badge)

<br>

*Cycle-accurate behavioural verification of the STM32 SPI2 master ↔ iCE40 SPI slave protocol across the complete FAW detect → IRQ → read → clear transaction sequence, executed entirely in software prior to hardware bring-up.*

</div>

---

## Table of Contents

1. [Architectural Rationale](#1-architectural-rationale)
2. [Protocol Transaction Map](#2-protocol-transaction-map)
3. [PDM Stimulus Generation](#3-pdm-stimulus-generation)
4. [Build & Execution](#4-build--execution)
5. [Results & Waveform Analysis](#5-results--waveform-analysis)
6. [Correlation to Physical Hardware](#6-correlation-to-physical-hardware)

---

## 1. Architectural Rationale

Before committing to a 6-layer PCB fabrication cycle and STM32 firmware development, the cross-chip SPI handshake protocol must be verified at the signal level. The co-processor architecture places an iCE40HX8K FPGA as a hardware inference accelerator on a dedicated SPI slave bus, serviced by an STM32H743IIT6 host. A protocol error here — an off-by-one in the register map, a metastability window in the IRQ clear sequence, or a CS deassertion timing violation — propagates directly to a non-functional field unit.

This SIL harness eliminates that risk by embedding a **cycle-accurate Verilator-compiled model** of the complete FPGA fabric inside a C++ testbench that precisely mimics the STM32's SPI2 peripheral behaviour, including:

- **SPI Mode 0** (CPOL=0, CPHA=0), MSB-first — matching the STM32 HAL `SPI_InitTypeDef` configuration in `ice40_config.c`
- **8 system clocks per SCK half-period** — a conservative 1 MHz SPI clock that accounts for iCE40 I/O pad rise times
- **Inter-byte gap of 48 system clocks** — replicating the STM32's register-reload stall between address and data phases
- **CS assertion/deassertion guard bands** — matching the `spi_csn` timing envelope observed on a logic analyser during preliminary STM32 Nucleo loopback testing

The alternative — Renode-Verilator co-simulation — was evaluated and deliberately bypassed. Renode's C++ integration API underwent a breaking change at v1.14+ (`RenodeAgent::addInput`/`addOutput`/`simulate` methods removed in favour of `renode_bus.h`), creating a brittle dependency chain between Renode binary version, IntegrationLibrary commit hash, and Verilator runtime. For the specific verification goal of validating the SPI transaction layer, a standalone harness is faster, simpler, and fully deterministic.

---

## 2. Protocol Transaction Map

The SIL harness executes the exact sequence that `main.c` state machine `state_active_compute_poll()` performs when `fpga_irq` fires on EXTI13: 

