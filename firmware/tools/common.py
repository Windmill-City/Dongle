#!/usr/bin/env python3
"""Shared helpers for the dongle upgrade tool."""

import zlib

VID = 0x1A86
PID = 0x5720

HID_USAGE_PAGE = 0xFF00
HID_REPORT_ID = 1
HID_STATUS_REPORT_ID = 2
HID_MARKER = 0x5A
HID_TOKEN = b"IAP"
HID_STATUS_OK = 0x00

CMD_APP_UPDATE = 0x01
CMD_BOOT_UPDATE = 0x02


def request(command, timeout_ms=500):
    """Ask the running firmware for an upgrade through HID feature report 1."""
    try:
        import hid
    except ImportError:
        raise SystemExit("hidapi is required: pip install hidapi")

    devices = hid.enumerate(VID, PID)
    if not devices:
        raise SystemExit(
            "upgrade device not found (VID:PID 1A86:5720). Is the "
            "application running and enumerated?")

    candidates = [info for info in devices
                  if info.get("usage_page") == HID_USAGE_PAGE] or devices
    report = bytes([HID_REPORT_ID, command & 0xFF, HID_MARKER]) + HID_TOKEN
    report += bytes(8 - len(HID_TOKEN) - 2)

    last_error = None
    for info in candidates:
        device = hid.device()
        try:
            device.open_path(info["path"])
            if device.send_feature_report(report) < 0:
                raise OSError("HID feature report was not accepted")
            try:
                response = device.read(9, timeout_ms)
            except (OSError, IOError):
                response = []
            if response:
                status = response[1] if response[0] in (
                    HID_REPORT_ID, HID_STATUS_REPORT_ID) else response[0]
                if status != HID_STATUS_OK:
                    raise SystemExit(
                        f"upgrade request rejected with HID status 0x{status:02X}")
            return True
        except (OSError, IOError) as error:
            last_error = error
        finally:
            device.close()

    raise SystemExit(f"HID upgrade request failed: {last_error}")


def find_port():
    """Return the first CDC ACM port exposed by the dongle, or None."""
    try:
        from serial.tools import list_ports
    except ImportError:
        raise SystemExit("pyserial is required: pip install pyserial")

    for port in list_ports.comports():
        if (port.vid, port.pid) == (VID, PID):
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
