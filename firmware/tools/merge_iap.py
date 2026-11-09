#!/usr/bin/env python3
import sys


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
            payload = record[4 : 4 + count]
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


def record(kind, address, payload):
    body = bytes([len(payload), (address >> 8) & 0xFF, address & 0xFF, kind]) + payload
    checksum = (-sum(body)) & 0xFF
    return ":" + body.hex().upper() + f"{checksum:02X}"


def write_hex(path, data):
    addresses = sorted(data)
    lines = []
    upper = None
    index = 0

    while index < len(addresses):
        start = addresses[index]
        current_upper = (start >> 16) & 0xFFFF
        if current_upper != upper:
            lines.append(record(0x04, 0, bytes([(current_upper >> 8) & 0xFF, current_upper & 0xFF])))
            upper = current_upper

        payload = bytearray()
        cursor = start
        while (
            index < len(addresses)
            and addresses[index] == cursor
            and len(payload) < 16
        ):
            payload.append(data[addresses[index]])
            cursor += 1
            index += 1
        lines.append(record(0x00, start & 0xFFFF, bytes(payload)))

    lines.append(":00000001FF")
    with open(path, "w", encoding="ascii", newline="\n") as handle:
        handle.write("\n".join(lines) + "\n")


def main() -> int:
    if len(sys.argv) != 4:
        print(f"usage: {sys.argv[0]} iap.hex app.hex combined.hex")
        return 1

    data = read_hex(sys.argv[1])
    app = read_hex(sys.argv[2])
    overlap = set(data) & set(app)
    if overlap:
        print(f"overlap at 0x{min(overlap):08X}")
        return 2
    data.update(app)
    write_hex(sys.argv[3], data)
    print(f"wrote {sys.argv[3]} ({len(data)} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
