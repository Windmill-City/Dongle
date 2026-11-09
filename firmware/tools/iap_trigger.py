#!/usr/bin/env python3
"""Request IAP mode through a USB vendor control request.

The dongle application listens on vendor request 0xB1:

    bmRequestType = 0x40 (vendor, host-to-device, device)
    bRequest      = 0xB1
    wValue        = 0x5A01  jump to the IAP bootloader
                    0x5A02  enter bootloader-region update mode
    wIndex        = 0x4941  token
    wLength       = 0

The request travels on EP0 and never touches the transparent RF data stream, so
payload bytes can no longer trigger an upgrade by accident.

On Windows the CH341 driver owns VID:PID 1A86:7523. pyusb can only send this
request while a user-mode-access driver is available for the device (for
example UsbDk, or WinUSB/libusbK bound with Zadig). If opening the device fails,
use SWD/wchisp to recover instead of the runtime trigger.

Import request() from other tools to keep the wire format in one place.
"""

import argparse
import sys

VID = 0x1A86
PID = 0x7523

REQUEST = 0xB1
MARKER = 0x5A
TOKEN = 0x4941
CMD_JUMP = 0x01
CMD_BOOT = 0x02


def request(command, timeout_ms=1000):
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
            "IAP trigger device not found (VID:PID 1A86:7523). Is the "
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


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Request IAP mode through the dongle's USB vendor request")
    parser.add_argument(
        "--boot",
        action="store_true",
        help="enter bootloader-region update mode instead of jumping to IAP")
    args = parser.parse_args()

    request(CMD_BOOT if args.boot else CMD_JUMP)
    print("IAP request sent" if not args.boot else "bootloader update request sent")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
