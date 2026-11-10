#!/usr/bin/env python3
"""Upgrade the dongle application or IAP bootloader.

Both targets use the same path:

    1. the host sends HID feature report 1
       (command 0x01 = application, 0x02 = boot region)
    2. the application sets the upgrade flag and resets into the bootloader
    3. the host opens the CDC ACM serial port and waits for the ready byte 0x5A
    4. the host streams A5 | CMD | LEN_LO LEN_HI | PAYLOAD | XOR frames
    5. for an application upgrade the bootloader writes 0x2000; for a boot
       upgrade it writes the staging area 0x3A000, verifies the CRC, then
       copies the image to 0x0000 from RAM and resets

Commands: 0x01 erase(length), 0x02 data(addr + data), 0x04 end(crc32).
Every frame is answered with one status byte; 0 means success.
"""

import argparse
import struct
import time

try:
    import serial
except ImportError:
    print("pip install pyserial")
    raise SystemExit(1)

import common

READY = 0x5A
SOF = 0xA5

CMD_ERASE = 0x01
CMD_DATA = 0x02
CMD_END = 0x04

STATUS_OK = 0
STATUS_NAMES = {
    0x01: "flash write/erase failed",
    0x02: "bad frame or checksum",
    0x03: "address/length outside the target region",
    0x04: "image CRC mismatch",
    0x05: "region not erased yet",
}

IAP_START = 0x00000000
IAP_END = 0x00002000
APP_START = 0x00002000
APP_END = 0x0003A000
STAGING_START = 0x0003A000
STAGING_END = 0x0003C000

# Multiple of 4 (word programming) and small enough that a whole frame stays
# inside one 64-byte USB packet.
CHUNK = 52


def frame(command, payload=b""):
    body = bytes([command, len(payload) & 0xFF, (len(payload) >> 8) & 0xFF]) + payload
    checksum = 0
    for value in body:
        checksum ^= value
    return bytes([SOF]) + body + bytes([checksum])


def load_image(path, start, end):
    if path.lower().endswith(".hex"):
        data = common.read_hex(path)
        if not data:
            raise SystemExit("empty hex file")
        first = min(data)
        last = max(data)
        if first != start:
            raise SystemExit(
                f"unexpected image start 0x{first:08X}, expected 0x{start:08X}")
        if last >= end:
            raise SystemExit(
                f"image end 0x{last:08X} exceeds 0x{end:08X}")
        image = bytearray(b"\xFF" * (last + 1 - start))
        for address, value in data.items():
            image[address - start] = value
    else:
        with open(path, "rb") as handle:
            image = bytearray(handle.read())
        if not image:
            raise SystemExit("empty image")
        if len(image) > end - start:
            raise SystemExit("image does not fit in the target region")

    while len(image) % 4:
        image.append(0xFF)
    return image


class Link:
    def __init__(self, port):
        self.port = serial.Serial(
            port, 115200, timeout=0.2, write_timeout=10.0)

    def close(self):
        self.port.close()

    def wait_ready(self, deadline):
        while time.time() < deadline:
            value = self.port.read(1)
            if value and value[0] == READY:
                return True
        return False

    def read_exact(self, size, timeout=15.0):
        previous = self.port.timeout
        self.port.timeout = timeout
        try:
            data = self.port.read(size)
        finally:
            self.port.timeout = previous
        if len(data) != size:
            raise SystemExit(
                f"timeout: expected {size} byte(s), got {len(data)}")
        return data

    def command(self, command, payload=b""):
        self.port.write(frame(command, payload))
        self.port.flush()
        status = self.read_exact(1)[0]
        if status != STATUS_OK:
            name = STATUS_NAMES.get(status, "unknown status")
            raise SystemExit(f"command failed: 0x{status:02X} ({name})")


def open_link(port, timeout):
    deadline = time.time() + timeout
    last_error = None

    while time.time() < deadline:
        candidates = []
        if port:
            candidates.append(port)
        detected = common.find_port()
        if detected and detected not in candidates:
            candidates.append(detected)

        for name in candidates:
            link = None
            try:
                link = Link(name)
                if link.wait_ready(min(deadline, time.time() + 2.0)):
                    return link
            except (serial.SerialException, OSError) as error:
                last_error = error
            if link is not None:
                link.close()
        time.sleep(0.2)

    detail = f": {last_error}" if last_error else ""
    raise SystemExit(f"upgrade port did not become ready{detail}")


def update(link, image, write_start):
    print("erasing...")
    link.command(CMD_ERASE, struct.pack("<I", len(image)))

    total = len(image)
    offset = 0
    while offset < total:
        chunk = image[offset:offset + CHUNK]
        link.command(CMD_DATA, struct.pack("<I", write_start + offset) + chunk)
        offset += len(chunk)
        print(f"\rprogrammed {offset}/{total}", end="", flush=True)
    print()

    print("verifying region CRC...")
    link.command(CMD_END, struct.pack("<I", common.crc32(image)))
    print("upgrade complete; device resetting")


def main():
    parser = argparse.ArgumentParser(
        description="Upgrade the dongle application or IAP bootloader")
    parser.add_argument("target", choices=("app", "iap"),
                        help="app = application region, iap = bootloader region")
    parser.add_argument("image", help="target .bin or .hex")
    parser.add_argument("--port",
                        help="serial port (auto-detected when omitted)")
    parser.add_argument("--timeout", type=float, default=15.0,
                        help="seconds to wait for the upgrade port (default 15)")
    args = parser.parse_args()

    if args.target == "app":
        start, end = APP_START, APP_END
        write_start = start
        command = common.CMD_APP_UPDATE
    else:
        start, end = IAP_START, IAP_END
        write_start = STAGING_START
        command = common.CMD_BOOT_UPDATE

    image = load_image(args.image, start, end)
    if args.target == "app":
        print(f"{args.target} image {args.image}: {len(image)} bytes at 0x{start:08X}")
    else:
        print(f"{args.target} image {args.image}: {len(image)} bytes, "
              f"staged at 0x{write_start:08X}, copied to 0x{IAP_START:08X}")

    common.request(command)
    print("upgrade request sent; waiting for the serial port...")

    link = open_link(args.port, args.timeout)
    try:
        update(link, image, write_start)
    finally:
        link.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
