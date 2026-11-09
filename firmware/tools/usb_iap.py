#!/usr/bin/env python3
"""Flash the CH570Q dongle through its USB IAP bootloader.

The bootloader enumerates as VID:PID 0x4348:0x55E0. On Windows the WCH
CH375 driver claims that device, so this tool talks to it through WCH's
CH375DLL (no extra driver install). Commands (bulk EP2 OUT):

    0x80 program   [0x80, count, data...]   count == 0 flushes the tail
    0x81 erase     [0x81, 0, addr0..addr3]  only addr == 0x2000 is accepted
    0x82 verify    [0x82, count, addr0..3, data...]
    0x83 end       [0x83, 0]                 reply, then jump to the app

Every command is answered with two bytes [status, 0]; status 0 means success.
"""

import argparse
import ctypes
import os
import sys
import time

APP_START = 0x00002000
APP_END = 0x0003C000
PACKET = 64
PROGRAM_DATA = PACKET - 2   # opcode + count
VERIFY_DATA = PACKET - 6    # opcode + count + 4-byte address
STATUS_OK = 0

DLL_CANDIDATES = [
    r"C:\MounRiver\MounRiver_Studio3\resources\app\resources\win32\components\WCH\Others\WCHISPTool\default\CH375DLL64.dll",
    r"C:\MounRiver\MounRiver_Studio3\resources\app\resources\win32\others\LinkDrv\ISP\CH375DLL64.dll",
    r"C:\MounRiver\MounRiver_Studio3\resources\app\resources\win32\components\WCH\Others\CommunicationLib\default\CH375DLL64.dll",
]


def find_dll(explicit):
    if explicit:
        return explicit
    for path in DLL_CANDIDATES:
        if os.path.isfile(path):
            return path
    raise SystemExit("CH375DLL64.dll not found; pass --dll")


class Ch375:
    def __init__(self, dll_path):
        self.dll = ctypes.WinDLL(dll_path)
        self.dll.CH375OpenDevice.argtypes = [ctypes.c_ulong]
        self.dll.CH375OpenDevice.restype = ctypes.c_ulong
        self.dll.CH375CloseDevice.argtypes = [ctypes.c_ulong]
        self.dll.CH375CloseDevice.restype = ctypes.c_ulong
        self.dll.CH375SetTimeout.argtypes = [ctypes.c_ulong, ctypes.c_ulong, ctypes.c_ulong]
        self.dll.CH375SetTimeout.restype = ctypes.c_ulong
        self.dll.CH375WriteData.argtypes = [ctypes.c_ulong, ctypes.c_void_p,
                                            ctypes.POINTER(ctypes.c_ulong)]
        self.dll.CH375WriteData.restype = ctypes.c_ulong
        self.dll.CH375ReadData.argtypes = [ctypes.c_ulong, ctypes.c_void_p,
                                           ctypes.POINTER(ctypes.c_ulong)]
        self.dll.CH375ReadData.restype = ctypes.c_ulong
        self.handle = None

    def open(self, timeout_ms=10000):
        for index in range(4):
            handle = self.dll.CH375OpenDevice(index)
            if handle not in (0, 0xFFFFFFFF):
                self.handle = handle
                self.dll.CH375SetTimeout(handle, timeout_ms, timeout_ms)
                return
        raise SystemExit("IAP device not found (VID:PID 4348:55E0)")

    def close(self):
        if self.handle is not None:
            self.dll.CH375CloseDevice(self.handle)
            self.handle = None

    def write(self, payload):
        buffer = (ctypes.c_ubyte * len(payload))(*payload)
        length = ctypes.c_ulong(len(payload))
        self.dll.CH375WriteData(self.handle, buffer, ctypes.byref(length))
        if length.value != len(payload):
            raise SystemExit(f"short write: {length.value}/{len(payload)}")

    def read(self, size):
        buffer = (ctypes.c_ubyte * size)()
        length = ctypes.c_ulong(size)
        self.dll.CH375ReadData(self.handle, buffer, ctypes.byref(length))
        return bytes(buffer[:length.value])

    def command(self, payload, retries=0):
        # A command issued while the bootloader is busy with a flash write can
        # be dropped; retry idempotent commands a few times.
        for _ in range(retries + 1):
            self.write(payload)
            reply = self.read(2)
            if len(reply) >= 1:
                return reply[0]
            time.sleep(0.005)
        raise SystemExit("no status reply from bootloader")


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
    if path.lower().endswith(".hex"):
        data = read_hex(path)
        if not data:
            raise SystemExit("empty hex file")
        start = min(data)
        if start != APP_START:
            raise SystemExit(f"unexpected image start 0x{start:08X}, expected 0x{APP_START:08X}")
        image = bytearray(max(data) + 1 - start)
        for address, value in data.items():
            image[address - start] = value
    else:
        with open(path, "rb") as handle:
            image = bytearray(handle.read())
    if not image:
        raise SystemExit("empty image")
    while len(image) % 4:
        image.append(0x00)
    if APP_START + len(image) > APP_END:
        raise SystemExit("image does not fit in the application region")
    return image


def trigger_iap(port):
    import serial
    with serial.Serial(port, 115200, timeout=1) as handle:
        handle.write(bytes([0x55, 0xAA, 0x49, 0x41, 0x50]))
        handle.flush()
    print(f"sent IAP magic on {port}")


def flash(link, image, do_verify=True):
    status = link.command(bytes([0x81, 0x00, APP_START & 0xFF,
                                 (APP_START >> 8) & 0xFF,
                                 (APP_START >> 16) & 0xFF,
                                 (APP_START >> 24) & 0xFF]), retries=5)
    if status != STATUS_OK:
        raise SystemExit(f"erase failed: status 0x{status:02X}")
    print(f"erased 0x{APP_START:08X}-0x{APP_END:08X}")

    offset = 0
    while offset < len(image):
        chunk = image[offset:offset + PROGRAM_DATA]
        status = link.command(bytes([0x80, len(chunk)]) + bytes(chunk))
        if status != STATUS_OK:
            raise SystemExit(f"program failed at 0x{APP_START + offset:08X}: status 0x{status:02X}")
        offset += len(chunk)
        print(f"\rprogrammed {offset}/{len(image)}", end="", flush=True)
    status = link.command(bytes([0x80, 0x00]), retries=5)
    if status != STATUS_OK:
        raise SystemExit(f"program flush failed: status 0x{status:02X}")
    print(f"\nprogrammed {len(image)} bytes")

    if do_verify:
        offset = 0
        while offset < len(image):
            chunk = image[offset:offset + (VERIFY_DATA & ~3)]
            address = APP_START + offset
            payload = bytes([0x82, len(chunk),
                             address & 0xFF, (address >> 8) & 0xFF,
                             (address >> 16) & 0xFF, (address >> 24) & 0xFF]) + bytes(chunk)
            status = link.command(payload, retries=5)
            if status != STATUS_OK:
                raise SystemExit(f"verify failed at 0x{address:08X}: status 0x{status:02X}")
            offset += len(chunk)
        print(f"verified {len(image)} bytes")
    else:
        print("verify skipped")

    link.write(bytes([0x83, 0x00]))
    try:
        reply = link.read(2)
        if reply and reply[0] != STATUS_OK:
            raise SystemExit(f"end failed: status 0x{reply[0]:02X}")
    except SystemExit:
        raise
    print("end sent, application started")


def main():
    parser = argparse.ArgumentParser(description="USB IAP flasher for the CH570Q dongle")
    parser.add_argument("image", help="application .bin or .hex")
    parser.add_argument("--dll", help="path to CH375DLL64.dll")
    parser.add_argument("--trigger", metavar="COMx",
                        help="send the IAP magic to a running app before flashing")
    parser.add_argument("--verify", action="store_true",
                        help="read back and compare after programming")
    args = parser.parse_args()

    image = load_image(args.image)
    print(f"image {args.image}: {len(image)} bytes at 0x{APP_START:08X}")

    if args.trigger:
        trigger_iap(args.trigger)

    link = Ch375(find_dll(args.dll))
    link.open()
    try:
        flash(link, image, do_verify=args.verify)
    finally:
        link.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
