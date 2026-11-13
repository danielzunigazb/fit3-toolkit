# The settings store (`/user/kvdata.bin`)

Read from the stock main image (R390XXU0AZA3) and from one watch; `src/re/kvmap.py` prints the map. Sizes and counts are listed here; the default values are Samsung's data and are not.

## How it works

The firmware keeps its settings as **records** in a RAM table at `0x20108a90` (0x1008 bytes). A record has a 0x14-byte header (`u16` crc at +2, `u16` loaded index at +4, `u16` count at +6, `u32` size at +8, a write hook at +0xc, a loader function at +0x10) and then its data. A record is read with `kv_read(key, buf, len, index)` (`0x2c0ea3c8`) and written with `kv_write` (`0x2c0ea438`, 177 callers); **`len` must equal the record's size, or the firmware asserts and restarts the watch.** The key is the record's offset in the table. At start-up the table is cleared and loaded from `/user/kvdata.bin` ("Dev-NV"); if its version (the first `u16`, 2) differs from the one in the image the defaults are written back (`Version mismatch...`, `Config Data invalid in NV`).

The initial image of the table is in flash at `0x2c2d53e0`, and `0x2c2d3d28` lists the records as `(offset, total length)`: 56 distinct records, 51 plain and 5 with a count (41 x 68 B at `0x7c`, 10 x 169 B at `0x2a8`, 200 x 88 B at `0x45c`, 200 x 26 B at `0x4f0`, and one more with a loader at `0xc0e9...`); the counted ones keep their elements outside the table.

## Seen in the live file

`/user/kvdata.bin` is 30,242 bytes: the table first (the headers; the version `02 00` at the start), then the data. The packing of the data after the table is **not decoded**: the records do not follow the table's offsets (a read of the key `0x110` at its table offset gives zeros).

## Records identified (from the code that reads them)

| Key | Size | What | How we know |
|---|---|---|---|
| `0x110` | 28 | display: `+4` screen timeout, `+8` brightness, `+0x14` idle return | `kv_display_*` getters (docs/symbols.csv) |
| `0x248` | 12 | vibration: the first word is the on/off flag that gates the motor | `kv_vibration_enabled` |
| `0x424` | 36 | daily goals: the first `u16` is the step goal (default 6,000, clamped to 1,000..49,000 by `0x2c0e5610`); the other fields are listed by the log "dailyactivity step, duration, calorie, floor, achour, extime..." but their order is a hypothesis | `0x2c0e5610`, and Salud reads the goal from here |
| `0x9d4` | 136 | `+8` bit 0 enables the World Clock row of the app list | `kv_worldclock_enabled` |
| `0xf8` | 4 | the app the double press of the side key opens (an index into the 14-window table) | docs/details.md |

The other 46 records are listed by `kvmap.py` without a meaning yet.

## What it is good for

Writing a record from an app (`kv_write`) changes the watch's real settings, so it would let an app set the brightness, the vibration or the goal. It is also a way to break the watch: the length must match, the version must stay, and the crc16 is kept by the write function. Reads are safe; **writes are untested on a watch.**
