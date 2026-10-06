# AgriGuard-RES Bring-Up Test Bench

A script-driven way to bring up and validate the AgriGuard-RES board without any firmware on the MCU. The host PC pokes the STM32H743's registers over SWD (through OpenOCD), which lets Python drive the MCU's GPIO pins directly and act as the bus master for the chips wired to them.

> **Status:** early starter. The structure and the first two tests are in place; the pin map still has to be filled in from the KiCad schematic, and the script has not yet been run against a physical board. Update this section once it has.

## How it works

```
Python script  --Tcl RPC (TCP :6666)-->  OpenOCD  --SWD-->  STM32H743
                                                              |
                                          GPIO registers (MODER / BSRR / IDR)
                                                              |
                                      bit-banged SPI to W25Q128, SX1262, iCE40 ...
```

1. OpenOCD talks to the STM32 through a debug probe and exposes a Tcl server on port 6666.
2. The script reads and writes memory-mapped registers (`read_memory` / `write_memory`) to set pin modes, drive pins and read pin levels.
3. A bit-banged SPI (mode 0, MSB first) is built on top of those pins, so the script can query the peripherals on the board.

Because it works at register level, it needs no flashed firmware. It also means a failing test points at hardware (power, soldering, traces, the part itself), not at software.

## Requirements

- A debug probe wired to the board's SWD/JTAG header (ST-Link, J-Link or an FTDI-based probe)
- [OpenOCD](https://openocd.org/) with `target/stm32h7x.cfg` available
- Python 3.8 or newer (standard library only, no pip packages needed)
- A bench supply set to 5.0 V with a 50 mA current limit for first power-up (see the bring-up procedure in the main README)

## Usage

1. Power the board (current-limited) and connect the probe.
2. Start OpenOCD in one terminal:

   ```
   openocd -f interface/stlink.cfg -f target/stm32h7x.cfg
   ```

   Swap `interface/stlink.cfg` for the config matching your probe.
3. Fill in the pin names for your board in the `FLASH_SPI` dictionary near the top of `agriguard_bringup.py`.
4. Run the bench in a second terminal:

   ```
   python agriguard_bringup.py
   ```

Example of the expected output on a good board:

```
=== AgriGuard-RES bring-up ===
[PASS] test_mcu_id: DBGMCU DEV_ID=0x450 (expect 0x450)
[PASS] test_flash_jedec: JEDEC ID=ef4018 (expect ef4018)
VERDICT: ALL PASSED
```

The script exits with code 0 when everything passes and 1 otherwise, so it can be called from other tooling later. A test that raises an exception is reported as a failure, never a pass.

## Current tests

| Test | What it checks | Expected |
|------|----------------|----------|
| `test_mcu_id` | Reads the DBGMCU ID code over SWD, proving the core is powered and the debug header is wired correctly | `DEV_ID = 0x450` |
| `test_flash_jedec` | Bit-bangs a JEDEC ID read (`0x9F`) to the W25Q128 | `ef 40 18` |

## Planned tests

- [ ] **SX1262:** `GetStatus` (opcode `0xC0`) over SPI, plus NRESET and BUSY line behaviour
- [ ] **iCE40HX8K:** bit-bang the SPI configuration (CRESET_B, bitstream, CDONE), then read the STATUS register to confirm the loaded design responds
- [ ] **BME680:** bit-banged I2C read of the chip ID at register `0xD0` (expect `0x61`)
- [ ] **Rails:** 3.3 V, 1.2 V and current draw via a SCPI-controlled bench supply or DMM
- [ ] **RS-485:** loopback check of the Modbus transceiver
- [ ] Per-board logging (serial number, date, pass/fail) to `logs/`

## Known limits

- **Slow by design.** Every clock edge is a TCP round trip, so this suits ID and status reads, not streaming data.
- **Not for use alongside running firmware.** The script overrides pin modes, so it assumes a fresh board or a held-in-reset application.
- **The MCU must be alive.** If the first test fails, check power rails, the crystal and the SWD wiring before anything else.
- **Register addresses** are for the STM32H743 (RM0433). Verify them if you port this to another part.

## Repository layout

```
Bringup_Test_Bench/
├── agriguard_bringup.py   # OpenOCD client, GPIO + bit-banged SPI helpers, tests
└── README.md
```

As the bench grows, the plan is to move the pin names into a separate `pinmap.py` (so a board respin only touches one file) and to add a `logs/` folder for per-board results.

## Related folders

- `Hardware/`: KiCad schematics, the source of truth for the pin map
- `Firmware_MCU/`: STM32 firmware
- `HDL_FPGA/`: SystemVerilog cores and the bitstreams the iCE40 tests will load
- `Software_In_A_Loop(SIL)/`: Verilator simulation harness (simulation only, no hardware)
