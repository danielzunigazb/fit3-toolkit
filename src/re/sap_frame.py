#!/usr/bin/env python3
"""Parse the BLE frame header documented in docs/ble-rx-path.md.

Read-only helper. It encodes what the AZA3 disassembly shows; no frame was
captured from a real watch, so treat the output as a hypothesis.
"""
import struct
import sys


def crc16(data: bytes, init: int = 0) -> int:
    """CRC-16/ARC, same table-driven routine as 0x2c10c430."""
    crc = init
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc


def parse(frame: bytes, n: int):
    """n = negotiated frame size. Returns a dict, raises ValueError."""
    long = n > 0xFF
    if long:
        if len(frame) < 6:
            raise ValueError("short frame")
        (length,) = struct.unpack_from(">H", frame, 0)
        (crc,) = struct.unpack_from(">H", frame, 2)
        if crc != crc16(frame[0:2]):
            raise ValueError("CrcErr")
        a, b = frame[4], frame[5]
        base = 6
    else:
        if len(frame) < 4 or frame[0] != frame[1]:
            raise ValueError("CrcErr")
        length = frame[0]
        a, b = frame[2], frame[3]
        base = 4
    cls = a >> 6
    if cls not in (0, 2):
        raise ValueError("ErrMsgTyp class %d" % cls)
    out = {
        "length": length, "class": cls, "kind": (a >> 3) & 7,
        "fragment": (a >> 1) & 3, "flag0": a & 1,
        "b_hi": b >> 5, "b_4": (b >> 4) & 1, "b_lo": b & 0xF,
    }
    if a & 4:
        hdr = base
    else:
        hdr = base + 2
        if len(frame) < hdr:
            raise ValueError("short frame")
        out["byte6"], out["byte7"] = frame[base], frame[base + 1]
    out["payload"] = frame[hdr:]
    return out


def selftest():
    assert crc16(b"123456789") == 0xBB3D
    f = struct.pack(">HH", 5, crc16(b"\x00\x05")) + bytes([0x00, 0x00, 0x01, 0x00]) + b"hello"
    r = parse(f, 0x401)
    assert r["class"] == 0 and r["kind"] == 0 and r["payload"] == b"hello", r
    try:
        parse(f[:2] + b"\x00\x00" + f[4:], 0x401)
    except ValueError as e:
        assert str(e) == "CrcErr"
    else:
        raise AssertionError("bad CRC accepted")
    print("sap_frame selftest ok")


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--hex":
        print(parse(bytes.fromhex(sys.argv[3]), int(sys.argv[2], 0)))
    else:
        selftest()
