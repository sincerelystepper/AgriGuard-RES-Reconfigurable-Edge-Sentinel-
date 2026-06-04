<div align="center">
AgriGuard-RES
Reconfigurable Edge Sentinel for Infrastructure-Independent Agricultural Intelligence
<br>
![Platform](https://img.shields.io/badge/Platform-STM32H743IIT6-0078D4?style=for-the-badge&logo=stmicroelectronics&logoColor=white)
![FPGA](https://img.shields.io/badge/FPGA-Lattice_iCE40HX8K-6A0DAD?style=for-the-badge)
![RF](https://img.shields.io/badge/RF-SX1262_LoRa_Sub--GHz-00897B?style=for-the-badge)
![Power](https://img.shields.io/badge/Deep_Sleep-<50_µA-F4A400?style=for-the-badge)
![PCB](https://img.shields.io/badge/PCB-6--Layer_Impedance_Controlled-C0392B?style=for-the-badge)
<br>
Developed by the engineering division at Agrionics Systems for next-generation agricultural edge autonomy.
</div>
---
Overview
Across Sub-Saharan Africa, over 500 million people depend on smallholder agriculture for survival. In Lesotho, where more than 70% of the population relies on subsistence farming, the convergence of climate variability, invasive pests such as the fall armyworm, and progressive soil degradation threatens food security at a structural level. Annual crop losses range from 20% to 40% — losses that are preventable given the right information at the right time.
Existing precision agriculture platforms fail in this environment because they are engineered for reliable internet and stable grid power. In Lesotho's highlands, where mountain ridges isolate farms and cellular signal is absent for the majority of the day, cloud-dependent IoT architectures are nonfunctional.
AgriGuard-RES resolves this architectural limitation by shifting AI inference to the exact source where data originates — in the field, on-device, and in real time.
---
Table of Contents
System Topology & Hardware Acceleration Architecture
Sub-GHz Mesh Networking & 6-Layer RF Layout
Ultra-Low-Power Solar Harvesting & Power Tree
Hardware Bring-Up & Quality Verification Procedures
---
1. System Topology & Hardware Acceleration Architecture
The core innovation of this platform is the hardware co-design of an STM32H743IIT6 host microcontroller alongside a Lattice iCE40HX8K FPGA, implemented on a high-density, impedance-controlled 6-layer PCB substrate.
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>AgriGuard — Schematic Review</title>
<style>
  @import url('https://fonts.googleapis.com/css2?family=IBM+Plex+Mono:wght@400;600&family=IBM+Plex+Sans:wght@300;400;600&display=swap');
:root {
--bg: #0d0f12;
--surface: #141720;
--border: #252a35;
--red: #ff3c3c;
--amber: #f5a623;
--yellow: #ffe066;
--green: #3cffa0;
--blue: #4db8ff;
--muted: #5a6270;
--text: #d8dde8;
--text-bright: #f0f3fa;
}
{ box-sizing: border-box; margin: 0; padding: 0; }
body {
background: var(--bg);
color: var(--text);
font-family: 'IBM Plex Sans', monospace;
font-size: 13.5px;
line-height: 1.7;
}
.header {
background: var(--surface);
border-bottom: 1px solid var(--border);
padding: 28px 40px 24px;
display: flex;
align-items: flex-end;
gap: 32px;
}
.header-title { font-family: 'IBM Plex Mono', monospace; }
.header-title h1 { font-size: 22px; color: var(--text-bright); font-weight: 600; letter-spacing: 0.02em; }
.header-title p { font-size: 11px; color: var(--muted); margin-top: 4px; letter-spacing: 0.08em; text-transform: uppercase; }
.badge-row { display: flex; gap: 10px; flex-wrap: wrap; margin-left: auto; }
.badge {
font-family: 'IBM Plex Mono', monospace;
font-size: 10px;
padding: 4px 10px;
border-radius: 3px;
letter-spacing: 0.05em;
font-weight: 600;
text-transform: uppercase;
}
.badge-red    { background: rgba(255,60,60,0.12);  color: var(--red);   border: 1px solid rgba(255,60,60,0.3); }
.badge-amber  { background: rgba(245,166,35,0.12); color: var(--amber); border: 1px solid rgba(245,166,35,0.3); }
.badge-yellow { background: rgba(255,224,102,0.10);color: var(--yellow);border: 1px solid rgba(255,224,102,0.25);}
.badge-green  { background: rgba(60,255,160,0.08); color: var(--green); border: 1px solid rgba(60,255,160,0.2); }
.summary-bar {
background: var(--surface);
border-bottom: 1px solid var(--border);
padding: 16px 40px;
display: flex;
gap: 40px;
}
.stat { text-align: center; }
.stat-num { font-family: 'IBM Plex Mono', monospace; font-size: 26px; font-weight: 600; }
.stat-label { font-size: 10px; color: var(--muted); text-transform: uppercase; letter-spacing: 0.08em; margin-top: 2px; }
.stat-red    .stat-num { color: var(--red); }
.stat-amber  .stat-num { color: var(--amber); }
.stat-yellow .stat-num { color: var(--yellow); }
.stat-green  .stat-num { color: var(--green); }
.divider { width: 1px; background: var(--border); }
.content { max-width: 1100px; margin: 0 auto; padding: 36px 40px 80px; }
.section { margin-bottom: 44px; }
.section-header {
display: flex;
align-items: center;
gap: 12px;
margin-bottom: 16px;
padding-bottom: 10px;
border-bottom: 1px solid var(--border);
}
.section-tag {
font-family: 'IBM Plex Mono', monospace;
font-size: 10px;
padding: 3px 8px;
border-radius: 2px;
background: var(--border);
color: var(--blue);
letter-spacing: 0.08em;
text-transform: uppercase;
}
.section-title { font-size: 15px; color: var(--text-bright); font-weight: 600; }
.issue {
background: var(--surface);
border: 1px solid var(--border);
border-left: 3px solid;
border-radius: 4px;
padding: 14px 18px;
margin-bottom: 10px;
position: relative;
}
.issue-blocking { border-left-color: var(--red); }
.issue-major    { border-left-color: var(--amber); }
.issue-moderate { border-left-color: var(--yellow); }
.issue-minor    { border-left-color: var(--muted); }
.issue-top {
display: flex;
align-items: flex-start;
gap: 12px;
margin-bottom: 6px;
}
.severity {
font-family: 'IBM Plex Mono', monospace;
font-size: 9px;
font-weight: 600;
padding: 2px 7px;
border-radius: 2px;
white-space: nowrap;
margin-top: 2px;
text-transform: uppercase;
letter-spacing: 0.05em;
}
.sev-blocking { background: rgba(255,60,60,0.15);  color: var(--red);   }
.sev-major    { background: rgba(245,166,35,0.15); color: var(--amber); }
.sev-moderate { background: rgba(255,224,102,0.1); color: var(--yellow);}
.sev-minor    { background: rgba(90,98,112,0.3);   color: var(--muted); }
.issue-title { color: var(--text-bright); font-weight: 600; font-size: 13.5px; }
.issue-body  { color: var(--text); font-size: 13px; margin-top: 4px; }
.issue-fix {
margin-top: 8px;
padding: 8px 12px;
background: rgba(77,184,255,0.05);
border: 1px solid rgba(77,184,255,0.15);
border-radius: 3px;
font-size: 12.5px;
}
.fix-label {
font-family: 'IBM Plex Mono', monospace;
font-size: 10px;
color: var(--blue);
font-weight: 600;
letter-spacing: 0.06em;
text-transform: uppercase;
margin-bottom: 3px;
}
code {
font-family: 'IBM Plex Mono', monospace;
background: rgba(255,255,255,0.06);
padding: 1px 5px;
border-radius: 2px;
font-size: 12px;
color: var(--blue);
}
.comp-table {
width: 100%;
border-collapse: collapse;
font-family: 'IBM Plex Mono', monospace;
font-size: 12px;
margin-top: 12px;
}
.comp-table th {
text-align: left;
padding: 6px 12px;
background: var(--border);
color: var(--muted);
font-size: 10px;
letter-spacing: 0.08em;
text-transform: uppercase;
}
.comp-table td {
padding: 6px 12px;
border-bottom: 1px solid rgba(255,255,255,0.04);
color: var(--text);
}
.comp-table tr:hover td { background: rgba(255,255,255,0.02); }
.ok    { color: var(--green); }
.warn  { color: var(--amber); }
.error { color: var(--red); }
.dnp   { color: var(--muted); font-style: italic; }
.footnote {
background: var(--surface);
border: 1px solid var(--border);
border-radius: 4px;
padding: 14px 18px;
margin-top: 16px;
font-size: 12.5px;
color: var(--muted);
}
.footnote strong { color: var(--text); }
</style>
</head>
<body>
<div class="header">
  <div class="header-title">
    <h1>AgriGuard Schematic Review</h1>
    <p>FPGA Board · 5 Subsheets · Full Engineering Analysis · Rev 2026-05-30</p>
  </div>
  <div class="badge-row">
    <span class="badge badge-red">🔴 Blocking</span>
    <span class="badge badge-amber">🟡 Major</span>
    <span class="badge badge-yellow">🟠 Moderate</span>
    <span class="badge badge-green">🟢 Minor</span>
  </div>
</div>
<div class="summary-bar">
  <div class="stat stat-red"><div class="stat-num">5</div><div class="stat-label">Blocking</div></div>
  <div class="divider"></div>
  <div class="stat stat-amber"><div class="stat-num">7</div><div class="stat-label">Major</div></div>
  <div class="divider"></div>
  <div class="stat stat-yellow"><div class="stat-num">11</div><div class="stat-label">Moderate</div></div>
  <div class="divider"></div>
  <div class="stat stat-green"><div class="stat-num">4</div><div class="stat-label">Minor</div></div>
</div>
<div class="content">
<!-- ═══════════════════════════════════════ LORA ═══════════════ -->
<div class="section">
  <div class="section-header">
    <span class="section-tag">Sheet 5</span>
    <span class="section-title">LoRa — SX1262 RF Front-End</span>
  </div>
<div class="issue issue-blocking">
    <div class="issue-top">
      <span class="severity sev-blocking">Blocking</span>
      <div>
        <div class="issue-title">TCXO Y1: Symbol/Footprint package mismatch — unprintable as-is</div>
        <div class="issue-body">
          Symbol is <code>TCXO-14</code> (DIP-14, 14-pin through-hole oscillator). Footprint assigned is <code>Custom_Oscillators:ASTXH1124576MHZT</code> — the ASTXH11 series is a 4-pad SMD 5×3.2mm oscillator. A 14-pin DIP symbol driving a 4-pad SMD footprint will cause an ERC pin-count error and will not be manufacturable. KiCad will also generate incorrect netlist because DIP-14 pins 1–13 have no matching footprint pads.
        </div>
        <div class="issue-fix">
          <div class="fix-label">Fix</div>
          Create or source a proper 4-pin SMD oscillator symbol matching the ASTXH11 pinout (VDD, GND, OUT, OE/Enable). Assign that symbol to the ASTXH footprint. Map: pin 1→OUT (to XTA), pin 2→GND, pin 3→OE (to DIO3), pin 4→VDD. Alternatively, use a KiCad-standard SMD TCXO symbol from the Oscillator library if one matches.
        </div>
      </div>
    </div>
  </div>
<div class="issue issue-blocking">
    <div class="issue-top">
      <span class="severity sev-blocking">Blocking</span>
      <div>
        <div class="issue-title">RF Switch U5: Wrong footprint — MASWSS0178 is MSOP-8, not SC-70-6</div>
        <div class="issue-body">
          U5 is placed as <code>MASWSS0178</code> (MACOM SPDT, 8-pin MSOP package) but the assigned footprint is <code>RF Switch:SC-70-6_PSM-M</code>, which is a 6-pin SC-70 package. The physical pads will not match the device pinout. The device will not solder correctly and even if forced, pins 4 (VCC) and 8 (EP/GND) will be unconnected to pads. The RF switch will not function.
        </div>
        <div class="issue-fix">
          <div class="fix-label">Fix</div>
          Change footprint to <code>Package_SO:MSOP-8-1EP_3x3mm_P0.65mm_EP1.73x1.85mm_ThermalVias</code> which matches the MASWSS0178's actual package. Verify pin mapping: pin 1=RFC, pin 3=RF1 (TX path), pin 4=VCC, pin 5=RF2 (RX path), pin 6=CTRL2, pin 7=CTRL1. Ensure VCC (pin 4) connects to 3.3V with 100nF decoupling.
        </div>
      </div>
    </div>
  </div>
<div class="issue issue-blocking">
    <div class="issue-top">
      <span class="severity sev-blocking">Blocking</span>
      <div>
        <div class="issue-title">C25 = 470nF on DCC_SW — Semtech specifies 330nF exactly</div>
        <div class="issue-body">
          The DCC_SW pin of the SX1262 is the external capacitor node for the internal DC-DC converter. Semtech's datasheet explicitly specifies <strong>330nF</strong> here. You have 470nF — that is 42% over-value. An oversized cap on this pin increases DC-DC startup time, can cause instability in the switching regulator, and will reduce efficiency at +22dBm transmit. At high power the DC-DC converter timing matters.
        </div>
        <div class="issue-fix">
          <div class="fix-label">Fix</div>
          Change C25 from 470nF to 330nF. Use X5R or X7R 0402/0603 ceramic. A 10V or higher voltage rating on 3.3V supply is fine and gives better capacitance stability.
        </div>
      </div>
    </div>
  </div>
<div class="issue issue-major">
    <div class="issue-top">
      <span class="severity sev-major">Major</span>
      <div>
        <div class="issue-title">R1 has no value — shows symbol type "R_Small_US" as value</div>
        <div class="issue-body">
          R1 in the LoRa sheet has its value field set to the symbol name <code>R_Small_US</code> instead of an actual resistance. This will cause ERC errors, an unusable BOM entry, and the fab will not know what to place. Its position in the RF chain (appears to be series in the TX path) makes this critical — a wrong or open resistance here will kill output power or cause impedance mismatch.
        </div>
        <div class="issue-fix">
          <div class="fix-label">Fix</div>
          If this is the 0Ω series link in the TX path (which it appears to be based on placement), set value to <code>0</code> and add a proper 0402 resistor footprint. If it's an ESD protection resistor it should be 10Ω–33Ω with value assigned.
        </div>
      </div>
    </div>
  </div>
<div class="issue issue-major">
    <div class="issue-top">
      <span class="severity sev-major">Major</span>
      <div>
        <div class="issue-title">L1 = "L_Ferrite_Small" — no actual value, no footprint</div>
        <div class="issue-body">
          L1 is a ferrite bead with its value showing the symbol type only. Its position appears to be in the VDD supply path (power filtering). Without a defined impedance value and footprint, this cannot be sourced, placed, or evaluated for adequacy. A ferrite bead needs a defined impedance at 100MHz (or the switching frequency of the power supply) and a rated DC current.
        </div>
        <div class="issue-fix">
          <div class="fix-label">Fix</div>
          Replace with a specific ferrite bead such as <code>BLM18PG221SN1D</code> (220Ω at 100MHz, 200mA) in a 0402 package. Set value to that part number or impedance. Assign 0402 ferrite footprint.
        </div>
      </div>
    </div>
  </div>
<div class="issue issue-moderate">
    <div class="issue-top">
      <span class="severity sev-moderate">Moderate</span>
      <div>
        <div class="issue-title">Multiple RF passives missing footprints — blocks PCB layout</div>
        <div class="issue-body">
          The following RF components have no footprint assigned: <code>L8</code> (8.2nH RX balun), <code>L9</code> (3.9nH TX matching), <code>L10</code> (5.6nH filter), <code>L11</code> (0Ω DNP antenna link), <code>C29</code> (5.6pF), <code>C31</code> (100pF DC block), <code>C37</code> (2.2pF), <code>C39</code> (5.6pF), <code>C40</code> (3.3pF), <code>C42</code> (DNP), <code>C43</code> (DNP), <code>R2</code> (100Ω), <code>R3</code> (100Ω). These are all in the RF chain and TX/RX matching network — none of them can be placed without footprints.
        </div>
        <div class="issue-fix">
          <div class="fix-label">Fix</div>
          RF inductors (1–10nH range): <code>Inductor_SMD:L_0402_1005Metric</code>. Capacitors ≤10pF (RF precision): <code>Capacitor_SMD:C_0402_1005Metric</code>. Resistors: <code>Resistor_SMD:R_0402_1005Metric</code>. DNP components still need footprints — assign them and mark DNP in BOM.
        </div>
      </div>
    </div>
  </div>
<div class="issue issue-moderate">
    <div class="issue-top">
      <span class="severity sev-moderate">Moderate</span>
      <div>
        <div class="issue-title">C26 = 4.7µF (VR_PA bulk) has no footprint</div>
        <div class="issue-body">
          C26 is the bulk decoupling capacitor on the VR_PA pin of the SX1262, which is the internal PA supply rail. This is one of the most noise-sensitive supply pins on the device — missing decoupling here will directly degrade transmit performance and spectral purity.
        </div>
        <div class="issue-fix">
          <div class="fix-label">Fix</div>
          Assign <code>Capacitor_SMD:C_0805_2012Metric</code> to C26 (4.7µF X5R/X7R typically needs 0805 for adequate capacitance at 3.3V bias). Place as close as physically possible to the VR_PA pin.
        </div>
      </div>
    </div>
  </div>
</div>
<!-- ═══════════════════════════════════════ MCU ═══════════════ -->
<div class="section">
  <div class="section-header">
    <span class="section-tag">Sheet 2</span>
    <span class="section-title">MCU — STM32H743IIT6</span>
  </div>
<div class="issue issue-blocking">
    <div class="issue-top">
      <span class="severity sev-blocking">Blocking</span>
      <div>
        <div class="issue-title">STM32H743 decoupling capacitors entirely absent — device will not boot reliably</div>
        <div class="issue-body">
          The STM32H743IIT6 in LQFP-176 requires decoupling on every VDD pin (there are 17 VDD/VDDA/VDDIO pins on this package) plus VCAP1 and VCAP2 (internal voltage regulator output capacitors, 2.2µF each, mandatory per ST datasheet). Without these: (1) the internal core voltage regulator will oscillate or fail to start, (2) digital noise will couple into VDDA causing ADC errors, (3) the device may not pass ERC or even boot on first power-up. No decoupling capacitors are visible anywhere in the MCU sheet.
        </div>
        <div class="issue-fix">
          <div class="fix-label">Fix</div>
          Add: 100nF 0402 ceramic on each VDD pin (×17, place within 0.5mm of each pin in PCB layout), 1µF 0402 on VDDA, 2.2µF 0805 on VCAP1 and VCAP2 (these are mandatory — ST will not support boards without them). Add a bulk 10µF on the 3.3V supply entry to the MCU. Reference ST AN4611 for full decoupling recommendations.
        </div>
      </div>
    </div>
  </div>
<div class="issue issue-major">
    <div class="issue-top">
      <span class="severity sev-major">Major</span>
      <div>
        <div class="issue-title">Duplicate FLASH_CS hierarchical label — one floating with no_connect marker</div>
        <div class="issue-body">
          The MCU subsheet exports <code>FLASH_CS</code> at two positions: one is connected to the FPGA (correct), one has a no_connect marker at position 57.15. Having two hierarchical labels with the same name where one is intentionally floating is confusing, creates ERC noise, and suggests a copy-paste error during schematic editing.
        </div>
        <div class="issue-fix">
          <div class="fix-label">Fix</div>
          Delete the second FLASH_CS hierarchical label at position 57.15. If it was intended to connect to something else, rename it appropriately.
        </div>
      </div>
    </div>
  </div>
<div class="issue issue-moderate">
    <div class="issue-top">
      <span class="severity sev-moderate">Moderate</span>
      <div>
        <div class="issue-title">No I2C pull-up resistors visible anywhere in the design</div>
        <div class="issue-body">
          I2C_SDA and I2C_SCL are used across the MCU and sensor sheets. I2C is an open-drain bus and requires pull-up resistors to VDD for correct operation. Without pull-ups, the bus lines cannot transition to logic HIGH and the entire I2C peripheral chain (BME680, AS7261) will not communicate. No pull-up resistors appear in any sheet.
        </div>
        <div class="issue-fix">
          <div class="fix-label">Fix</div>
          Add 4.7kΩ pull-up from I2C_SDA to +3.3V and 4.7kΩ from I2C_SCL to +3.3V. Place these in the MCU sheet or sensor sheet — one location is enough. Use 0402 resistors.
        </div>
      </div>
    </div>
  </div>
</div>
<!-- ═══════════════════════════════════════ FPGA ═══════════════ -->
<div class="section">
  <div class="section-header">
    <span class="section-tag">Sheet 4</span>
    <span class="section-title">FPGA — iCE40HX8K-BG121</span>
  </div>
<div class="issue issue-blocking">
    <div class="issue-top">
      <span class="severity sev-blocking">Blocking</span>
      <div>
        <div class="issue-title">+1V2 power rail used in FPGA sheet but NO 1.2V regulator exists in the power sheet</div>
        <div class="issue-body">
          The FPGA sheet uses a <code>+1V2</code> global power symbol to supply the iCE40HX8K core VCC pins (required range: 1.14V–1.26V). The power sheet contains: ADP5090 (MPPT), TPS62932 (adjustable), TPS563300 (3.3V), LMZM23600V3 (fixed 3.3V always-on). None of these generate 1.2V. Similarly, the FPGA sheet uses <code>+2V5</code> which also has no generation source. The FPGA core will be unpowered and the device will not configure.
        </div>
        <div class="issue-fix">
          <div class="fix-label">Fix</div>
          Add a dedicated 1.2V LDO to the power sheet. Options: <code>MIC5504-1.2YM5</code> or <code>TLV70012DBVR</code> (SOT-23-5, 300mA, from 3.3V input). Wire its output to the +1V2 global net. Add 100nF + 10µF decoupling at input and output. Also decide whether +2V5 is actually needed — if all VCCIO banks run at 3.3V, remove the +2V5 rail entirely.
        </div>
      </div>
    </div>
  </div>
<div class="issue issue-major">
    <div class="issue-top">
      <span class="severity sev-major">Major</span>
      <div>
        <div class="issue-title">Y2 oscillator = 16MHz, not 45MHz as in the original design intent</div>
        <div class="issue-body">
          The original design concept described a 45MHz oscillator for the iCE40 to support faster SPI throughput and data processing. The FPGA sheet now uses <code>ASDMB-16MHz</code>. At 16MHz, maximum SPI slave throughput to the STM32 is roughly 8MB/s (practical limit), which is adequate for this application but is a significant reduction from 45MHz capabilities. The SPI clock frequency the STM32 drives must not exceed what the FPGA can handle at 16MHz — so the STM32 SPI clock should be ≤8MHz for reliable transfers.
        </div>
        <div class="issue-fix">
          <div class="fix-label">Fix</div>
          If 16MHz is intentional, document it. Constrain firmware SPI clock to ≤8MHz for the FPGA bus. If 45MHz was desired, swap to the ASDMB-45MHz variant (same footprint). Either way, ensure the firmware and FPGA Verilog are clocked consistently.
        </div>
      </div>
    </div>
  </div>
<div class="issue issue-moderate">
    <div class="issue-top">
      <span class="severity sev-moderate">Moderate</span>
      <div>
        <div class="issue-title">R12 = 0.476kΩ is inconsistent with R10/R11 = 0.47kΩ — likely typo</div>
        <div class="issue-body">
          R10 and R11 are 0.47kΩ (470Ω) LED current limiting resistors. R12 is 0.476kΩ which does not correspond to any E96 standard resistor value and appears to be a transcription error. The LED current difference is trivial but the BOM will list two different resistor values that should be identical.
        </div>
        <div class="issue-fix">
          <div class="fix-label">Fix</div>
          Change R12 from 0.476K to 0.47K (470Ω) to match R10/R11.
        </div>
      </div>
    </div>
  </div>
<div class="issue issue-moderate">
    <div class="issue-top">
      <span class="severity sev-moderate">Moderate</span>
      <div>
        <div class="issue-title">R16 has no value — shows symbol type as value</div>
        <div class="issue-body">
          R16 has its value field set to <code>R_US</code> (the symbol name) instead of an actual resistance value. Unvalued components create ERC errors and unusable BOM lines.
        </div>
        <div class="issue-fix">
          <div class="fix-label">Fix</div>
          Determine R16's intended value from context (likely a pull-up or current limiter) and set it explicitly.
        </div>
      </div>
    </div>
  </div>
<div class="issue issue-moderate">
    <div class="issue-top">
      <span class="severity sev-moderate">Moderate</span>
      <div>
        <div class="issue-title">D2–D5 LEDs and TP8–TP12 test points have no footprints</div>
        <div class="issue-body">
          All four status LEDs and five test points have no footprint assigned. These will be missing from the PCB layout. Test points in particular are important for FPGA debugging (DONE, CDONE, CLK_IN lines).
        </div>
        <div class="issue-fix">
          <div class="fix-label">Fix</div>
          LEDs: <code>LED_SMD:LED_0603_1608Metric</code>. Test points: <code>TestPoint:TestPoint_Pad_D1.0mm</code>.
        </div>
      </div>
    </div>
  </div>
</div>
<!-- ═══════════════════════════════════════ POWER ═══════════════ -->
<div class="section">
  <div class="section-header">
    <span class="section-tag">Sheet 3</span>
    <span class="section-title">Power Management</span>
  </div>
<div class="issue issue-major">
    <div class="issue-top">
      <span class="severity sev-major">Major</span>
      <div>
        <div class="issue-title">BT3 = "5V" assigned a CR2032 battery footprint — fundamentally wrong component type</div>
        <div class="issue-body">
          BT3 has value <code>5V</code> and footprint <code>Battery_Panasonic_CR2032-HFN_Horizontal_CircularHoles</code>. A Panasonic CR2032 is a 3V lithium coin cell — it cannot provide 5V and is not a rechargeable component. If this is intended as a supercapacitor for cold-start energy buffering, it needs a supercapacitor footprint and component. If it's backup power for RTC, it should be a proper 3V coin cell with correct voltage labeling.
        </div>
        <div class="issue-fix">
          <div class="fix-label">Fix</div>
          Clarify design intent. For RTC backup: use a 3V CR2032 and label it correctly. For energy buffer: use a supercapacitor (e.g., 1F/5.5V) and assign the CAP_PAS409 footprint already in BT4. Do not mix up battery and supercap symbols.
        </div>
      </div>
    </div>
  </div>
<div class="issue issue-major">
    <div class="issue-top">
      <span class="severity sev-major">Major</span>
      <div>
        <div class="issue-title">C33 = 100nF with a 10mm-diameter THT radial electrolytic footprint</div>
        <div class="issue-body">
          C33 is a 100nF decoupling capacitor but its footprint is <code>Capacitor_THT:CP_Radial_D10.0mm_P7.50mm</code> — a through-hole radial electrolytic capacitor footprint 10mm in diameter. A 100nF value at reasonable voltages is a ceramic SMD capacitor, not a 10mm electrolytic. This is clearly a wrong footprint assignment. It will waste enormous board area and the pads/hole size will be incompatible with a ceramic cap lead.
        </div>
        <div class="issue-fix">
          <div class="fix-label">Fix</div>
          Change C33 footprint to <code>Capacitor_SMD:C_0805_2012Metric</code> or <code>C_0402_1005Metric</code> as appropriate for the placement.
        </div>
      </div>
    </div>
  </div>
<div class="issue issue-major">
    <div class="issue-top">
      <span class="severity sev-major">Major</span>
      <div>
        <div class="issue-title">TPS62932 feedback divider — verify output voltage is correct</div>
        <div class="issue-body">
          U12 TPS62932 has <code>Rfbt4 = 30.9kΩ</code> and <code>Rfbb4 = 10kΩ</code>. TPS62932 has Vref = 0.6V. Vout = 0.6 × (1 + 30.9/10) = <strong>2.454V</strong>. This is neither 3.3V nor 5V. If this regulator is intended to supply the MCU at 3.3V, the output is 0.85V short. If it supplies sensors at 5V, the output is 2.55V short. For 3.3V output with Rfbb=10kΩ: Rfbt = (3.3/0.6 − 1) × 10k = <strong>45kΩ</strong>. For 5V: Rfbt = <strong>73.3kΩ</strong>.
        </div>
        <div class="issue-fix">
          <div class="fix-label">Fix</div>
          Determine the intended output voltage for U12, then recalculate and update Rfbt4. For 3.3V: change Rfbt4 from 30.9kΩ to 45.3kΩ (nearest E96: 45.3kΩ). Verify U9 TPS563300 separately with its own feedback network.
        </div>
      </div>
    </div>
  </div>
<div class="issue issue-moderate">
    <div class="issue-top">
      <span class="severity sev-moderate">Moderate</span>
      <div>
        <div class="issue-title">Many power components missing footprints — blocks PCB layout</div>
        <div class="issue-body">
          The following have no footprint: <code>L5</code> (22µH), <code>L6</code> (6.8µH), <code>L7</code> (4.7µH), <code>R4</code> (511kΩ), <code>R5</code> (88.7kΩ), <code>R6</code> (0Ω), <code>R7</code> (0Ω), <code>R8</code> (53.6kΩ), <code>R9</code> (49.9Ω), <code>R20</code> (10.2kΩ), <code>R25</code> (20kΩ), <code>J5</code>, <code>J6</code>, <code>C46–C49</code>. Power inductors and precision resistors are critical for DC-DC converter stability.
        </div>
        <div class="issue-fix">
          <div class="fix-label">Fix</div>
          Power inductors (µH range, high current): <code>Inductor_SMD:L_5020_1210Metric</code> or similar rated part. Precision resistors: 0402 or 0603. Connectors: assign appropriate screw terminal footprints. Cross-reference each component against its IC's reference design for recommended package size.
        </div>
      </div>
    </div>
  </div>
</div>
<!-- ═══════════════════════════════════════ SENSORS ═══════════════ -->
<div class="section">
  <div class="section-header">
    <span class="section-tag">Sheet 6</span>
    <span class="section-title">Sensors</span>
  </div>
<div class="issue issue-minor">
    <div class="issue-top">
      <span class="severity sev-minor">Minor</span>
      <div>
        <div class="issue-title">I2C_MIC_* label names on an I2S interface — misleading naming</div>
        <div class="issue-body">
          The MP45DT02 microphone is an I2S device (CLK/WS/DOUT). The sensor sheet exports hierarchical labels named <code>I2C_MIC_SCK</code>, <code>I2C_MIC_WS</code>, <code>I2C_MIC_SD</code>. The FPGA sheet correctly names these <code>I2S_MIC_*</code>. Functionally the connections are correct (root sheet wires them together), but the naming mismatch will cause confusion during firmware development and debugging.
        </div>
        <div class="issue-fix">
          <div class="fix-label">Fix</div>
          Rename sensor sheet hierarchical labels from <code>I2C_MIC_*</code> to <code>I2S_MIC_*</code> to match the FPGA sheet. Update root sheet connections accordingly.
        </div>
      </div>
    </div>
  </div>
<div class="issue issue-minor">
    <div class="issue-top">
      <span class="severity sev-minor">Minor</span>
      <div>
        <div class="issue-title">C2 = 100nF (BME680 decoupling) has no footprint</div>
        <div class="issue-body">
          C2 in the sensor sheet has no footprint. It appears to be the BME680 VDD decoupling cap. The BME680 is sensitive to supply noise — missing decoupling affects humidity and pressure measurement accuracy.
        </div>
        <div class="issue-fix">
          <div class="fix-label">Fix</div>
          Assign <code>Capacitor_SMD:C_0402_1005Metric</code> to C2.
        </div>
      </div>
    </div>
  </div>
</div>
<!-- ═══════════════════════════════════════ SYSTEM LEVEL ═══════════════ -->
<div class="section">
  <div class="section-header">
    <span class="section-tag">System</span>
    <span class="section-title">Cross-Sheet / System Architecture</span>
  </div>
<div class="issue issue-moderate">
    <div class="issue-top">
      <span class="severity sev-moderate">Moderate</span>
      <div>
        <div class="issue-title">Shared SPI bus between MCU, iCE40, and W25Q128 requires boot sequence discipline</div>
        <div class="issue-body">
          The W25Q128 bitstream flash is accessed by the iCE40 during configuration (before DONE asserts high) and by the STM32 after boot. Both share the same SPI bus. If the STM32 starts driving SPI before the iCE40's DONE pin goes high, it will corrupt the FPGA configuration read. The FPGA_DONE signal is connected back to the MCU which provides the gate, but firmware must respect this: do not drive SPI_FPGA_* lines until FPGA_DONE = HIGH.
        </div>
        <div class="issue-fix">
          <div class="fix-label">Fix</div>
          In STM32 firmware: configure SPI_FPGA_MOSI, SCK, CS as hi-Z (input) until FPGA_DONE goes high. After DONE asserts, STM32 takes over SPI master. Document this in the root schematic as a note. Consider adding a weak pull-up on FLASH_CS so it defaults deasserted during MCU boot.
        </div>
      </div>
    </div>
  </div>
<div class="issue issue-minor">
    <div class="issue-top">
      <span class="severity sev-minor">Minor</span>
      <div>
        <div class="issue-title">No reset supervisor / power-on reset circuit for the STM32</div>
        <div class="issue-body">
          The STM32H743 has an internal POR circuit, but on a battery-powered solar system with potentially slow supply ramp-up, a dedicated external supervisor (e.g., <code>MCP100</code> or <code>TPS3839</code>) improves reliability by holding NRST low until the supply is stable. This is not a hard requirement but is best practice for field-deployed hardware.
        </div>
        <div class="issue-fix">
          <div class="fix-label">Fix</div>
          Optional but recommended: add a voltage supervisor with threshold at ~2.9V (below 3.3V rail) driving NRST. Use a low-power part like <code>TPS3839G33DBVR</code> (SOT-23-5, 1µA quiescent).
        </div>
      </div>
    </div>
  </div>
</div>
<!-- ═══════════════════════════════════════ RF CHAIN AUDIT ══════════════ -->
<div class="section">
  <div class="section-header">
    <span class="section-tag">RF Audit</span>
    <span class="section-title">LoRa RF Chain Component Values vs Reference</span>
  </div>
  <table class="comp-table">
    <tr>
      <th>Ref</th><th>Function</th><th>Your Value</th><th>Semtech Reference</th><th>Status</th>
    </tr>
    <tr><td>L3</td><td>PA bias inductor (VR_PA→RFO)</td><td>470nH</td><td>470nH</td><td class="ok">✓ Correct</td></tr>
    <tr><td>C26</td><td>VR_PA bulk decoupling</td><td>4.7µF</td><td>4.7µF</td><td class="ok">✓ Correct</td></tr>
    <tr><td>C27</td><td>VR_PA HF decoupling</td><td>100nF</td><td>100nF</td><td class="ok">✓ Correct</td></tr>
    <tr><td>C25</td><td>DCC_SW converter cap</td><td>470nF</td><td>330nF</td><td class="error">✗ Wrong — change to 330nF</td></tr>
    <tr><td>C21/22/23</td><td>VDD decoupling (3×)</td><td>100nF each</td><td>100nF each</td><td class="ok">✓ Correct</td></tr>
    <tr><td>L9</td><td>TX matching series (L3 in AN)</td><td>3.9nH</td><td>3.9nH</td><td class="ok">✓ Correct</td></tr>
    <tr><td>C37</td><td>2nd harmonic notch (C4 in AN)</td><td>2.2pF</td><td>2.2pF</td><td class="ok">✓ Correct</td></tr>
    <tr><td>C40</td><td>Impedance match shunt (C5/C7)</td><td>3.3pF</td><td>3.3–5.6pF</td><td class="ok">✓ Acceptable</td></tr>
    <tr><td>L10</td><td>Higher order filter (L4 in AN)</td><td>5.6nH</td><td>5.6nH</td><td class="ok">✓ Correct</td></tr>
    <tr><td>C31</td><td>DC block (C6 in AN)</td><td>100pF</td><td>100pF</td><td class="ok">✓ Correct</td></tr>
    <tr><td>C29/C39</td><td>RX balun caps (C11/C12)</td><td>5.6pF each</td><td>5.6pF each</td><td class="ok">✓ Correct</td></tr>
    <tr><td>L8</td><td>RX balun inductor (L6 in AN)</td><td>8.2nH</td><td>8.2nH</td><td class="ok">✓ Correct</td></tr>
    <tr><td>L11</td><td>Antenna series link</td><td>0 [DNP]</td><td>DNP initially</td><td class="ok">✓ Correct practice</td></tr>
    <tr><td>C42/C43</td><td>Antenna shunt (DNP)</td><td>0 [DNP]</td><td>DNP initially</td><td class="ok">✓ Correct practice</td></tr>
    <tr><td>R2/R3</td><td>DIO series resistors (ESD)</td><td>100Ω each</td><td>100Ω recommended</td><td class="ok">✓ Correct</td></tr>
    <tr><td>R1</td><td>TX series element</td><td>No value</td><td>0Ω link</td><td class="error">✗ Missing value</td></tr>
    <tr><td>Y1</td><td>32MHz TCXO reference</td><td>ASTXH footprint</td><td>SMD TCXO</td><td class="warn">⚠ Symbol mismatch — see issue #1</td></tr>
    <tr><td>U5</td><td>RF switch SPDT</td><td>MASWSS0178</td><td>PE4259 / equivalent</td><td class="warn">⚠ Wrong footprint — see issue #2</td></tr>
  </table>
</div>
<!-- ═══════════════════════════════════════ PRIORITY ══════════════ -->
<div class="section">
  <div class="section-header">
    <span class="section-tag">Action List</span>
    <span class="section-title">Fix Priority Order</span>
  </div>
<div class="footnote">
    <strong>Do these before any PCB layout work:</strong><br><br>
    1. Fix TCXO Y1 symbol/footprint mismatch — create correct 4-pin SMD TCXO symbol<br>
    2. Fix RF switch U5 footprint — change to MSOP-8<br>
    3. Add 1.2V LDO to power sheet for iCE40 core<br>
    4. Add all STM32H743 decoupling capacitors (17× VDD + VDDA + VCAP)<br>
    5. Change C25 from 470nF to 330nF<br>
    6. Set R1 value (0Ω) and assign footprint<br>
    7. Assign footprints to all RF passives (L8, L9, L10, R2, R3, etc.)<br>
    8. Verify TPS62932 output voltage — change Rfbt4 to 45.3kΩ if output is 3.3V<br>
    9. Add I2C pull-ups (4.7kΩ × 2 to 3.3V)<br>
    10. Fix C33 footprint (100nF should not be THT 10mm radial)<br><br>
    <strong>Then run full ERC before pushing to PCB layout.</strong>
  </div>
</div>
</div><!-- /content -->
</body>
</html>
The 480 MHz ARM Cortex-M7 handles global system orchestration and communication stacks. The Lattice FPGA operates as a dedicated parallel hardware accelerator on a split-rail architecture (1.2V Core Logic / 3.3V I/O), executing FFT-based feature extraction and lightweight modulation classification on raw sensor streams with sub-100 ms latency — a computational envelope that software-driven embedded processors cannot match at this micro-power scale.
Multi-Seasonal Dynamic Reconfigurability
The FPGA is fully field-reconfigurable. Multiple AI bitstreams are stored non-volatilely on an onboard 128 Mb Winbond W25Q128JVS Serial NOR Flash chip. The host MCU reloads bitstreams on-demand via the dedicated SPI configuration path, switching the sentinel's operational profile across agricultural seasons without a single hardware modification.
Pipeline	Sensor Modality	Function
Acoustic Pest Pipeline	Digital MEMS Microphone	FPGA-accelerated hardware FFT; isolates insect chewing frequency signatures before infestations become visible
Spectral Crop Pipeline	Multi-channel NIR Sensor	Fuses NIR telemetry to compute vegetation indices; detects crop water-stress and cellular decay before leaf symptoms manifest
Soil Intelligence Pipeline	RS485 Modbus Probe + BME680	Interfaces with five-parameter soil probes and microclimate sensors; compiles localised fertility and irrigation profiles
---
2. Sub-GHz Mesh Networking & 6-Layer RF Layout
To bypass mountain topography barriers, AgriGuard-RES transmits only actionable edge decisions — not raw sensor data — over a sub-GHz Semtech SX1262 LoRa mesh network. The 433 MHz band was selected specifically for its superior diffraction and knife-edge bending characteristics across rugged highland terrain, achieving 5 to 15 kilometres per hop.
To guarantee that high-speed digital transitions from the FPGA configuration bus do not desensitise the radio receiver front-end, the physical board uses a 6-layer impedance-controlled stack-up.
PCB Layer Stack-Up
Layer	Functional Domain	Copper Profile
Layer 1 — Top	High-Speed RF / Signal	Straight 50-Ω Coplanar Waveguide, TX Switch, Matching Networks
Layer 2 — Int 1	Shield Plane	Solid, unbroken ground return path (GND)
Layer 3 — Int 2	Inner Signal	MCU breakout, dense SPI configuration buses
Layer 4 — Int 3	Power Domain	Split copper rails: `+3V3`, `+1V2` FPGA core, always-on lines
Layer 5 — Int 4	Auxiliary Plane	Secondary solid reference ground plane (GND)
Layer 6 — Bottom	Low-Speed Signal	Sensor tracking, FPGA backside decoupling capacitor networks
RF Layout Constraints
Straight-Line RF Mandate
The antenna trace travels in a perfectly straight, horizontal line on Layer 1 from the SX1262 output, through the passive Pi-network filter, across the RF switch pins, into the U.FL antenna port. Vias, corners, and trace bends are strictly prohibited on the RF line to prevent impedance discontinuities.
Faraday Shield Wall
The top-layer coplanar ground plane running parallel to the RF trace is stitched directly into the Layer 2 ground plane via shielding vias spaced at 1.5 mm to 2.0 mm increments, forming a continuous electromagnetic barrier that traps radio emissions and prevents coupling into adjacent signal layers.
USB 2.0 Differential Pair
The `USB_D+` and `USB_D−` debug lines are routed as a uniform parallel track (`0.15 mm width / 0.15 mm gap`) over the solid Layer 2 GND mirror, enforcing a rigid 90-Ω differential impedance profile.
---
3. Ultra-Low-Power Solar Harvesting & Power Tree
Perpetual, unattended field operation through harsh highland winters and extended overcast periods requires an ultra-low-power standby configuration targeting under 50 µA in deep sleep.
Subsystem Breakdown
Harvester Core — ADP5090
Dynamically tracks maximum power via an optimised 70% MPPT ratio using a multi-mega-ohm network (`R23 = 6.34 MΩ`, `R24 = 14.7 MΩ`) to eliminate passive divider leakage current. Charge termination (`TERM`) is locked at 5.0 V to saturate the primary supercapacitor reservoir.
Dual-Priority Backup Rail
A dual CR2032 coin-cell pack in series (6.0 V nominal), protected inline via a low-drop 1N4148 Schottky diode, steps the backup entry line down to 5.3 V to respect the harvester's maximum silicon input limit.
Always-On Sleep Rail — LMZM23600V3
Draws a mere 28 µA under zero-load conditions. Pin 4 (`EN`) is tied to `VIN_MAIN`; Pin 2 (`MODE/SYNC`) is grounded to enforce Auto PFM burst-switching mode, maintaining the persistent STM32 memory domains during deep standby.
Active 3.3 V Main Rail — TPS563300
Supervised by a rigid resistor network (`R1 = 590 kΩ`, `R2 = 169 kΩ`) targeting a 5.0 V startup threshold and a 4.0 V safety shutdown barrier, preventing brownout crashes. Power-gates the peripheral sensors, transceiver, and FPGA I/O banks.
Active 1.2 V FPGA Core Rail — TPS62932
Uses the chip's native internal 0.8 V reference with an optimised E96 1% divider (`R_fbt = 5.1 kΩ`, `R_fbb = 10.2 kΩ`), producing a mathematically exact 1.200 V core rail. Pin 1 (`RT`) is grounded, forcing a compact 1.2 MHz switching loop paired with a 2.2 µH low-profile inductor to suppress control-loop ripple.
Power Rail Summary
Rail	Regulator	Target Output	Condition
Always-On Sleep	LMZM23600V3	3.30 V	28 µA quiescent; Auto PFM
Active Main	TPS563300	3.30 V	Startup at 5.0 V; shutdown at 4.0 V
FPGA Core	TPS62932	1.200 V	1.2 MHz switching; `RT` = GND
Backup Entry	CR2032 × 2 + 1N4148	~5.3 V	Series pair; Schottky-clamped
---
4. Hardware Bring-Up & Quality Verification Procedures
Execute these diagnostic procedures on unpopulated or newly assembled hardware before connecting any active power source or debug probe.
Phase A — Isolation Checks (Continuity Diagnostic)
Set a digital multimeter to Continuity / Resistance Mode and verify power rail isolation across the 6-layer plane splits.
Measure between `+3V3` and `GND`. Confirm the resistance climbs into the kilo-ohm range as the 0603 decoupling arrays charge.
Measure between `+1V2` and `GND`. Verify the internal FPGA core logic plane is completely isolated.
Confirm that the centre grounding pad tap of the high-speed clock crystal load capacitors (`C50` / `C51`) maintains direct, low-impedance continuity to the primary system ground layer.
Phase B — Power-Up Verification (Voltage & Signal Loop Diagnostics)
Apply 5.0 V from a current-limited bench supply (50 mA ceiling) to the solar terminal block `J3`.
Confirm the LMZM23600V3 outputs a clean, stable 3.30 V always-on rail to the MCU standby domains.
Verify the TPS62932 output sits within the FPGA core operating tolerance window — 1.14 V to 1.26 V.
Connect a PC via USB-C to debug port `J7`. Confirm that the Configuration Channel pins (`CC1` / `CC2`) register a clean voltage drop across their independent 5.1 kΩ resistors, signalling the host machine to deliver a standard 5 V VBUS rail to sensing pin `PA9`.
---
<div align="center">
Agrionics Systems — Engineering Division
Precision at the edge. Intelligence in the field.
</div>
