#!/usr/bin/env python3
"""Update the IAP bootloader region (0x0000-0x1FFF) of the dongle while the
application is running.

The application enumerates as the CH341 USB serial (VID:PID 1A86:7523). Send it
the bootloader-update vendor control request (see iap_trigger.py), wait for the
ready byte, then stream framed commands:

    vendor: 0xB1 / wValue 0x5A02 / wIndex 0x4941 on EP0
    ready:  5A                            (one byte, device -> host)
    frame:  A5 | CMD | LEN_LO LEN_HI | PAYLOAD | XOR

Commands:
    0x01 erase   (empty payload)          erase 0x0000-0x1FFF
    0x02 data    addr(4) + data           program data at addr
    0x03 verify  addr(4) + data           compare flash against data
    0x04 end     crc32(4)                 verify whole region, then reset
    0x05 abort   (empty payload)          leave update mode (only before erase)

Every frame is answered with one status byte; 0 means success. The image is
padded with 0xFF to the full 8 KiB region, so the end CRC covers exactly what
the flash holds after the update.
"""

import argparse
import struct
import sys
import time

try:
    import serial
except ImportError:
    print("pip install pyserial")
    raise SystemExit(1)

import iap_trigger

READY = 0x5A
SOF = 0xA5

CMD_ERASE = 0x01
CMD_DATA = 0x02
CMD_VERIFY = 0x03
CMD_END = 0x04
CMD_ABORT = 0x05

STATUS_OK = 0
STATUS_NAMES = {
    0x01: "flash write/erase failed",
    0x02: "bad frame or checksum",
    0x03: "address/length outside the IAP region",
    0x04: "image CRC mismatch",
    0x05: "region not erased yet",
}

IAP_START = 0x00000000
IAP_END = 0x00002000
# Multiple of 4 (word programming) and small enough that a whole frame stays
# inside one 64-byte USB packet.
CHUNK = 52

try:
    import zlib

    def crc32(data):
        return zlib.crc32(data) & 0xFFFFFFFF
except ImportError:  # pragma: no cover
    def crc32(data):
        crc = 0xFFFFFFFF
        for value in data:
            crc ^= value
            for _ in range(8):
                crc = (crc >> 1) ^ (0xEDB88320 & (0 - (crc & 1)))
        return (~crc) & 0xFFFFFFFF


def frame(command, payload=b""):
    body = bytes([command, len(payload) & 0xFF, (len(payload) >> 8) & 0xFF]) + payload
    checksum = 0
    for value in body:
        checksum ^= value
    return bytes([SOF]) + body + bytes([checksum])


def read_hex(path):
    data = {}
    base = 0
    with open(path, "r", encoding="ascii") as handle:
        for line in handle:
            line = line.strip()
            if not line.startswith(":"):
                continue
            record = bytes.fromhex(line[1:])
            if len(record) < 5:
                continue
            count = record[0]
            address = (record[1] << 8) | record[2]
            kind = record[3]
            payload = record[4:4 + count]
            if kind == 0:
                for offset, value in enumerate(payload):
                    data[base + address + offset] = value
            elif kind == 1:
                break
            elif kind == 2:
                base = ((payload[0] << 8) | payload[1]) << 4
            elif kind == 4:
                base = ((payload[0] << 8) | payload[1]) << 16
    return data


def load_image(path):
    region = bytearray(b"\xFF" * (IAP_END - IAP_START))
    if path.lower().endswith(".hex"):
        data = read_hex(path)
        if not data:
            raise SystemExit("empty hex file")
        if min(data) < IAP_START or max(data) >= IAP_END:
            raise SystemExit(
                f"image out of range: 0x{min(data):08X}-0x{max(data):08X}")
        for address, value in data.items():
            region[address - IAP_START] = value
    else:
        with open(path, "rb") as handle:
            image = handle.read()
        if not image:
            raise SystemExit("empty image")
        if len(image) > IAP_END - IAP_START:
            raise SystemExit("image does not fit in the 8 KiB IAP region")
        region[:len(image)] = image
    return bytes(region)


class Link:
    def __init__(self, port, timeout):
        self.port = serial.Serial(port, 115200, timeout=timeout, write_timeout=timeout)

    def close(self):
        self.port.close()

    def read_exact(self, size):
        data = self.port.read(size)
        if len(data) != size:
            raise SystemExit(
                f"timeout: expected {size} byte(s), got {len(data)}")
        return data

    def start(self):
        self.port.reset_input_buffer()
        iap_trigger.request(iap_trigger.CMD_BOOT)
        deadline = time.time() + 3.0
        while time.time() < deadline:
            value = self.port.read(1)
            if value and value[0] == READY:
                return
        raise SystemExit("device did not acknowledge the bootloader update request")

    def command(self, command, payload=b""):
        self.port.write(frame(command, payload))
        self.port.flush()
        status = self.read_exact(1)[0]
        if status != STATUS_OK:
            name = STATUS_NAMES.get(status, "unknown status")
            raise SystemExit(f"command failed: 0x{status:02X} ({name})")

    def abort(self):
        self.port.write(frame(CMD_ABORT))
        self.port.flush()
        self.read_exact(1)


def update(link, region):
    print("erasing IAP region...")
    link.command(CMD_ERASE)

    total = len(region)
    offset = 0
    while offset < total:
        chunk = region[offset:offset + CHUNK]
        payload = struct.pack("<I", IAP_START + offset) + chunk
        link.command(CMD_DATA, payload)
        offset += len(chunk)
        print(f"\rprogrammed {offset}/{total}", end="", flush=True)
    print()

    print("verifying region CRC...")
    link.command(CMD_END, struct.pack("<I", crc32(region)))
    print("IAP region updated; device resetting")


def main():
    parser = argparse.ArgumentParser(
        description="Update the dongle IAP bootloader region over USB serial")
    parser.add_argument("image", help="bootloader .bin or .hex (8 KiB max)")
    parser.add_argument("port", help="serial port of the running application (e.g. COM5)")
    parser.add_argument("--timeout", type=float, default=2.0,
                        help="serial timeout in seconds (default 2)")
    parser.add_argument("--abort", action="store_true",
                        help="leave update mode without writing (only before erase)")
    args = parser.parse_args()

    link = Link(args.port, args.timeout)
    try:
        link.start()
        if args.abort:
            link.abort()
            print("aborted")
            return 0
        update(link, load_image(args.image))
    finally:
        link.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
