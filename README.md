# Agrionics Co. Edge Node: Multi-Voltage Solar Energy Harvesting Platform

##  Project Overview
The **AG-HARV-H7** is an industrial-grade, ultra-low-power, 6-layer edge computing platform designed for long-term agricultural telemetry deployment. Engineered to solve the challenge of executing zero-overhead edge compute in power-starved remote fields, this board combines a high-performance **STM32H743** microchip with a parallel processing **Lattice iCE40HX FPGA** co-processor and a long-range sub-GHz **Semtech SX1262 LoRa** transceiver.

The entire node is sustained natively via micro-power solar harvesting, featuring dual failover backup reservoirs and highly advanced power-gating logic designed to protect and optimize every nano-joule of stored environmental energy.

---

##  1. System Power Tree & Low-Power Sleep Architecture

To maintain an unyielding 24/7 field presence on minimal solar exposure, the platform splits its power tracking domains into specialized, highly efficient regulation layers:

###  The Harvester Core (ADP5090)
* **Maximum Power Point Tracking (MPPT):** Regulated dynamically at an optimized **70% ratio** using high-impedance divider loops (`R23 = 6.34MΩ`, `R24 = 14.7MΩ`) to eliminate resistor-network leakage current.
* **Charge Termination Threshold (`TERM`):** Set safely to **5.0V** to fully charge the primary high-capacity supercapacitor reservoir.
* **Failover Backup Network:** Incorporates a dual CR2032 coin-cell pack running in series (6.0V nominal). Protected inline via a low-drop **1N4148 Schottky diode**, safely stepping down the backup entry line to **5.3V** to respect the harvester's maximum silicon limitations.

###  Always-On 3.3V Sleep Rail (LMZM23600V3)
* **Quiescent Target:** Sips a mere **28 µA** under zero-load standby states.
* **Configuration:** Pin 4 (`EN`) is tied directly to `VIN_MAIN`, while Pin 2 (`MODE/SYNC`) is bound to Ground to enforce ultra-efficient **Auto PFM (Pulse Frequency Modulation)** burst switching during deep sleep states. 
* **Target Load:** Powering the STM32H7's persistent internal memory registers and wake-up timers during low-power deep shutdown.

###  Active 3.3V Main Rail (TPS563300)
* **Undervoltage Lockout (UVLO):** Equipped with a rigid resistor supervisor network (`R1 = 590 kΩ`, `R2 = 169 kΩ`) targeting a crisp **5.0V power-on startup threshold** and a hard **4.0V safety shutdown barrier** to prevent brownout firmware corruption loops.
* **Target Load:** Powers all peripheral sensors, external microphones, line transceivers, and FPGA I/O banks when the system enters active compute mode.

###  Active 1.2V FPGA Core Rail (TPS62932)
* **Formula Execution:** Utilizing the chip's native internal **0.8V reference** and an optimized E96 1% divider ratio (`R_fbt = 5.1 kΩ`, `R_fbb = 10.2 kΩ`), dropping power smoothly down to a mathematically perfect **1.200V** core rail.
* **Switching Profile:** Pin 1 (`RT`) is tied straight to Ground, forcing a highly compact **1.2 MHz switching loop** paired with a specialized **2.2 µH low-profile inductor** to eliminate control loop ripples.
* **Target Load:** Powers the high-speed internal look-up tables (LUTs) and routing fabric of the Lattice FPGA.

---

##  2. Dual-Chip Co-Processing Matrix

The board isolates software execution from intensive, real-time sensor polling by leveraging a hardware parallel co-processing pipeline:

* **The Host Manager (STM32H743IIT6):** Coordinates global system status, schedules low-power sleep states, structures the LoRa radio telemetry payloads, and drives communication interfaces.
* **The Hardware Accelerator (Lattice iCE40HX8K-TQ144):** Operates on a strict split-rail voltage architecture (1.2V Core Logic / 3.3V I/O Banks). The FPGA acts as an always-on parallel engine, directly interfacing with high-speed environmental sensors and a digital microphone array. It executes real-time hardware-level DSP filtering and data formatting over the edge before waking up the host MCU, saving maximum system power.
* **Non-Volatile Memory (Winbond W25Q128JVS):** A high-capacity **128 Mb (16 MB) Serial NOR Flash** chip operating on the 3.3V bus. It stores the FPGA’s volatile SRAM boot bitstream, while providing extensive, non-volatile leftover sectors for localized agricultural sensor data logging.

---

##  3. 50-Ω Sub-GHz LoRa RF Design & 6-Layer Layout

To eliminate signal degradation and ensure long-range radio propagation through heavy crop canopies, the RF front-end architecture is routed to strict high-frequency standards:


| Layer ID | Functional Domain | Copper Allocation Profile |
| :--- | :--- | :--- |
| **Layer 1 (Top)** | High-Speed Signal | RF Transmission Line, Power Components, Component Pads |
| **Layer 2 (Int 1)** | Shield Plane | **Solid, Unbroken Ground Plane (GND)** |
| **Layer 3 (Int 2)** | Slow Signal | Microcontroller Breakout, Dense Control Busses |
| **Layer 4 (Int 3)** | Power Domain | Split Copper Rails (`+3V3`, `+1V2`, Always-On Buses) |
| **Layer 5 (Int 4)** | Aux Plane | Secondary Solid Reference Ground Plane (GND) |
| **Layer 6 (Bottom)**| Low-Speed Signal| External Sensor Traces, Local Decoupling Routing Channels |

###  Key RF/High-Frequency Constraints:
1. **The Straight-Line Mandate:** The antenna path runs on a perfectly straight horizontal axis from the Semtech SX1262 output, through the passive Pi-network filter, directly across the flipped RF Switch pins, into the U.FL connector. Bends, kinks, and signal vias are completely eliminated from the RF line.
2. **The Faraday Cage Shield Wall:** The top-layer coplanar ground pour tracking the RF trace is stitched into Layer 2 GND using multiple shielding vias spaced tightly at **1.5 mm to 2.0 mm increments**, trapping radio emissions and protecting internal digital buses.
3. **USB 2.0 Differential Routing:** The `USB_D_P` and `USB_D_N` debug data lines are locked side-by-side as a uniform parallel track (**0.15mm trace width / 0.15mm separation gap**) over an unbroken Layer 2 GND mirror to enforce a rigid 90-Ω differential impedance profile.

---

##  4. Initial Hardware Bring-Up & Safety Validation Steps

When your completed board arrives from the manufacturing facility, execute these validation diagnostics **before** plugging in any programming tools or power components:

### Phase A: Unpowered Shorts Inspection (Multimeter Check)
Set your digital multimeter to **Continuity/Resistance Mode** and verify that no direct manufacturing copper shorts exist across your major rails:
1. Measure between `+3V3` and `GND`. Ensure resistance climbs safely into the Kilo-Ohm range as the decoupling capacitors charge.
2. Measure between `+1V2` and `GND`. Ensure the core logic loops are isolated.
3. Verify that the center tap of the high-speed crystal load capacitors (`C50`/`C51`) maintains solid, uncompromised continuity directly to the system Ground plane.

### Phase B: Controlled Power Testing (LDO & Buck Diagnostics)
1. Hook your lab bench power supply to the solar terminal block `J3`, setting a strict current limit of **50 mA** and a voltage input of **5.0V**.
2. Measure the output pad of the **LMZM23600V3 sleep module** to ensure a rock-solid **3.30V** always-on rail is generated.
3. Verify that the **TPS62932 regulator output** sits precisely within the FPGA's mandatory core logic window (**1.14V to 1.26V**).
4. Connect a PC via a standard USB-C cable into debug port `J7`. Ensure the Configuration Channel pins (`CC1`/`CC2`) register a clean drop down across their independent **5.1 kΩ resistors**, forcing the host machine to deliver a standard 5V VBUS rail to the sensing pin `PA9`.

---
*Developed and maintained by the engineering division at **Agrionics Co.** for next-generation agricultural edge autonomy.*
