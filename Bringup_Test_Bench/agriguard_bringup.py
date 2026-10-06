"""
AgriGuard-RES bring-up bench
----------------------------
Pokes the STM32H743 over SWD through OpenOCD's Tcl RPC port (6666).
No firmware needed: the host writes GPIO registers directly, so the PC acts
as the bus master for the chips wired to the MCU.

Run OpenOCD first (any ST-Link / J-Link / FTDI probe over SWD):
    openocd -f interface/stlink.cfg -f target/stm32h7x.cfg
Then:
    python agriguard_bringup.py
"""
import socket
import sys

TERM = b"\x1a"  # OpenOCD Tcl RPC message terminator

# STM32H743 register map (verify against RM0433)
RCC_AHB4ENR = 0x580244E0
GPIO_BASE = {p: 0x58020000 + 0x400 * i for i, p in enumerate("ABCDEFGHIJK")}
MODER, IDR, BSRR = 0x00, 0x10, 0x18
DBGMCU_IDC = 0x5C001000

# ---- TODO: fill these from your KiCad schematic -------------------------
FLASH_SPI = dict(sck="PA5", mosi="PA7", miso="PA6", cs="PA4")  # W25Q128 bus
# -------------------------------------------------------------------------


class OpenOCD:
    def __init__(self, host="127.0.0.1", port=6666):
        self.s = socket.create_connection((host, port), timeout=5)

    def cmd(self, c):
        self.s.sendall(c.encode() + TERM)
        buf = b""
        while not buf.endswith(TERM):
            chunk = self.s.recv(4096)
            if not chunk:
                raise ConnectionError("OpenOCD closed the connection")
            buf += chunk
        return buf[:-1].decode().strip()

    def rd32(self, addr):
        return int(self.cmd(f"read_memory 0x{addr:08x} 32 1").split()[0], 16)

    def wr32(self, addr, val):
        self.cmd(f"write_memory 0x{addr:08x} 32 {{0x{val:08x}}}")


class Pin:
    def __init__(self, ocd, name):
        self.ocd, self.port, self.n = ocd, name[1], int(name[2:])
        self.base = GPIO_BASE[self.port]
        en = ocd.rd32(RCC_AHB4ENR)
        ocd.wr32(RCC_AHB4ENR, en | (1 << "ABCDEFGHIJK".index(self.port)))

    def _mode(self, bits):
        m = self.ocd.rd32(self.base + MODER)
        m &= ~(3 << (2 * self.n))
        self.ocd.wr32(self.base + MODER, m | (bits << (2 * self.n)))

    def output(self):
        self._mode(0b01)

    def input(self):
        self._mode(0b00)

    def high(self):
        self.ocd.wr32(self.base + BSRR, 1 << self.n)

    def low(self):
        self.ocd.wr32(self.base + BSRR, 1 << (self.n + 16))

    def read(self):
        return (self.ocd.rd32(self.base + IDR) >> self.n) & 1


class BitBangSPI:
    """SPI mode 0, MSB first. Slow (one TCP round trip per edge) but fine for ID/status reads."""

    def __init__(self, ocd, sck, mosi, miso, cs):
        self.sck, self.mosi = Pin(ocd, sck), Pin(ocd, mosi)
        self.miso, self.cs = Pin(ocd, miso), Pin(ocd, cs)
        for p in (self.sck, self.mosi, self.cs):
            p.output()
        self.miso.input()
        self.sck.low()
        self.cs.high()

    def xfer(self, data):
        out = bytearray()
        self.cs.low()
        for byte in data:
            rx = 0
            for bit in range(7, -1, -1):
                (self.mosi.high if (byte >> bit) & 1 else self.mosi.low)()
                self.sck.high()
                rx = (rx << 1) | self.miso.read()
                self.sck.low()
            out.append(rx)
        self.cs.high()
        return bytes(out)


# ------------------------------- tests -----------------------------------
def test_mcu_id(ocd):
    dev_id = ocd.rd32(DBGMCU_IDC) & 0xFFF
    return dev_id == 0x450, f"DBGMCU DEV_ID=0x{dev_id:03X} (expect 0x450)"


def test_flash_jedec(ocd):
    spi = BitBangSPI(ocd, **FLASH_SPI)
    rx = spi.xfer(bytes([0x9F, 0, 0, 0]))[1:]
    return rx == bytes([0xEF, 0x40, 0x18]), f"JEDEC ID={rx.hex()} (expect ef4018)"


TESTS = [test_mcu_id, test_flash_jedec]


def main():
    ocd = OpenOCD()
    print("=== AgriGuard-RES bring-up ===")
    failed = 0
    for t in TESTS:
        try:
            ok, msg = t(ocd)
        except Exception as e:  # a crash is a fail, never a pass
            ok, msg = False, f"exception: {e}"
        print(f"[{'PASS' if ok else 'FAIL'}] {t.__name__}: {msg}")
        failed += not ok
    print("VERDICT:", "ALL PASSED" if not failed else f"{failed} FAILED")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
