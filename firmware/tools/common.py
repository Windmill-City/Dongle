#!/usr/bin/env python3
"""Shared helpers for the dongle upgrade tool."""

import zlib

VID = 0x1A86
PID = 0x7523

REQUEST = 0xB1
MARKER = 0x5A
TOKEN = 0x4941
CMD_APP_UPDATE = 0x01
CMD_BOOT_UPDATE = 0x02


def request(command, timeout_ms=1000):
    """Send the vendor control request that enters an upgrade mode."""
    try:
        import usb.core
    except ImportError:
        raise SystemExit("pyusb is required: pip install pyusb")

    try:
        device = usb.core.find(idVendor=VID, idProduct=PID)
    except usb.core.NoBackendError:
        raise SystemExit(
            "pyusb has no libusb backend; install libusb-1.0 (and UsbDk on "
            "Windows if the CH341 driver owns the device)")
    if device is None:
        raise SystemExit(
            "upgrade device not found (VID:PID 1A86:7523). Is the "
            "application running and enumerated?")

    try:
        device.ctrl_transfer(
            0x40,
            REQUEST,
            (MARKER << 8) | (command & 0xFF),
            TOKEN,
            0,
            timeout=timeout_ms,
        )
    except usb.core.USBError as error:
        raise SystemExit(
            f"vendor control request failed: {error}\n"
            "On Windows the CH341 driver must permit user-mode control "
            "transfers (UsbDk, or WinUSB/libusbK via Zadig).")
    return True


def find_port():
    """Return the first CH341 serial port, or None."""
    try:
        from serial.tools import list_ports
    except ImportError:
        raise SystemExit("pyserial is required: pip install pyserial")

    for port in list_ports.comports():
        if (port.vid, port.pid) == (VID, PID):
            return port.device
        hwid = (port.hwid or "").upper()
        if f"{VID:04X}:{PID:04X}" in hwid:
            return port.device
    return None


def read_hex(path):
    """Read an Intel HEX file into a sparse {address: byte} mapping."""
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


def crc32(data):
    return zlib.crc32(data) & 0xFFFFFFFF
