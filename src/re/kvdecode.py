# SPDX-License-Identifier: Apache-2.0
"""kvdecode.py: reads a copy of /user/kvdata.bin and prints its records, checking each crc.

    python3 -I src/re/kvdecode.py kvdata.bin [main-stock.bin]

Layout (seen in one live file, matched against the 56 records of the image): the file is a sequence of 0x200-byte slots; a slot is one record: a 0x14-byte header, then the data.
Header: u16 version (the record's "loaded index"), u16 crc, u16 (unknown), u16 count, u32 size, then two words that are zero or code pointers (not data).
The crc is CRC-16/CCITT-FALSE (poly 0x1021, init 0xffff) over the `size` bytes of data. Slots are in the same order as the record list of the image (`kvmap.py`), with holes (an all-zero slot) in between; a record's key (its offset in the RAM table) is therefore found by matching the sizes in order, which needs the image.
Records with a count keep their elements outside the file. Values are the user's settings: the tool prints them, so do not paste its output in an issue."""
import struct, sys
BASE = 0x2c080000; IMG = 0x2c2d53e0 - BASE; LST = 0x2c2d3d28 - BASE
SLOT = 0x200
# names from docs/settings.md (log strings near the readers); the rest stay unnamed
NAMES = {0xf8: "double-press app", 0x110: "display", 0x248: "vibration", 0x424: "daily goals", 0x9d4: "world clock",
         0xd4: "sleep mode", 0xe58: "theater mode", 0x174: "user info", 0x380: "heart-rate alert", 0x3f4: "sleep goal",
         0x5e0: "phone info", 0x964: "reminders", 0xb0c: "account", 0xe78: "timers?", 0x45c: "sport.cfg", 0x4f0: "sport_screen.bin",
         0x2a8: "bluetooth.cfg", 0x520: "app order"}

def crc16(data, c=0xFFFF):
    for b in data:
        c ^= b << 8
        for _ in range(8): c = ((c << 1) ^ 0x1021) & 0xFFFF if c & 0x8000 else (c << 1) & 0xFFFF
    return c

def keys(image):
    out, seen = [], set()
    for i in range(60):
        off, total = struct.unpack_from("<II", image, LST + 8 * i)
        if total and off not in seen: seen.add(off); out.append((off, struct.unpack_from("<I", image, IMG + off + 8)[0]))
    return sorted(out)

def slots(kv):
    for k in range(0, len(kv), SLOT):
        h = kv[k:k + 0x14]
        if len(h) < 0x14: break
        ver, crc, _x, cnt, size = struct.unpack_from("<HHHHI", h, 0)
        yield k, ver, crc, cnt, size, kv[k + 0x14:k + 0x14 + size]

def main(argv):
    kv = open(argv[0], "rb").read(); image = open(argv[1], "rb").read() if len(argv) > 1 else None
    recs = keys(image) if image else []; ri = 0; bad = 0; n = 0
    for k, ver, crc, cnt, size, data in slots(kv):
        if size == 0 and ver == 0: continue
        key = None
        if recs:
            j = ri
            while j < len(recs) and recs[j][1] != size: j += 1
            if j < len(recs): key = recs[j][0]; ri = j + 1
        ok = crc16(data) == crc; n += 1; bad += not ok
        print("slot %#06x  key %-6s size %4d count %-3s crc %s  %-17s %s" % (k, "%#x" % key if key is not None else "?", size, cnt or "-", "ok " if ok else "BAD", NAMES.get(key, ""), data[:24].hex(" ")))
    print("%d records, %d with a bad crc" % (n, bad))

if __name__ == "__main__":
    if len(sys.argv) not in (2, 3): sys.exit(__doc__)
    main(sys.argv[1:])
