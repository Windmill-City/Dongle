#!/usr/bin/env python3
import sys

try:
    import serial
except ImportError:
    print("pip install pyserial")
    raise

MAGIC = bytes([0x55, 0xAA, 0x49, 0x41, 0x50])


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} COMx")
        return 1

    with serial.Serial(sys.argv[1], 115200, timeout=1) as port:
        port.write(MAGIC)
        port.flush()

    print("IAP magic sent")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
