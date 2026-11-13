# SPDX-License-Identifier: Apache-2.0
"""kvmap.py: prints the map of the settings store (the "KV" records of /user/kvdata.bin) from a stock main image: key offset, data size and count of each record.

    python3 -I src/re/kvmap.py local/re/main-stock.bin

The firmware keeps a table of records in RAM (`0x20108a90`; a 0x14-byte header, then the data: u16 crc at +2, u16 loaded index at +4, u16 count at +6, u32 size at +8, a write hook at +0xc, a loader at +0x10) that it fills at start-up from `/user/kvdata.bin`
(storage "Dev-NV"). The initial image of the table is in the flash image (0x2c2d53e0, 0x1008 bytes, first u16 = the version, 2) and the list of the records is `(offset, total length)` pairs at 0x2c2d3d28.
Only sizes and counts are printed; the default values are Samsung's data."""
import struct, sys
BASE = 0x2c080000; IMG = 0x2c2d53e0 - BASE; LST = 0x2c2d3d28 - BASE

def main(path):
    raw = open(path, "rb").read(); seen = {}
    print("table version: %d" % struct.unpack_from("<H", raw, IMG)[0])
    for i in range(60):
        off, total = struct.unpack_from("<II", raw, LST + 8 * i)
        if total == 0 or off in seen: continue
        size, = struct.unpack_from("<I", raw, IMG + off + 8); count, = struct.unpack_from("<H", raw, IMG + off + 6)
        seen[off] = (size, count)
    print("%d records:" % len(seen)); print("  key     data   count")
    for off, (size, count) in sorted(seen.items()): print("  %#05x  %5d   %s" % (off, size, count or "-"))

if __name__ == "__main__":
    if len(sys.argv) != 2: sys.exit(__doc__)
    main(sys.argv[1])
