# The settings store (`/user/kvdata.bin`)

Read from the stock main image (R390XXU0AZA3) and from one watch; `src/re/kvmap.py` prints the map. Sizes and counts are listed here; the default values are Samsung's data and are not.

## How it works

The firmware keeps its settings as **records** in a RAM table at `0x20108a90` (0x1008 bytes). A record has a 0x14-byte header (`u16` crc at +2, `u16` loaded index at +4, `u16` count at +6, `u32` size at +8, a write hook at +0xc, a loader function at +0x10) and then its data. A record is read with `kv_read(key, buf, len, index)` (`0x2c0ea3c8`) and written with `kv_write` (`0x2c0ea438`, 177 callers); **`len` must equal the record's size, or the firmware asserts and restarts the watch.** The key is the record's offset in the table. At start-up the table is cleared and loaded from `/user/kvdata.bin` ("Dev-NV"); if its version (the first `u16`, 2) differs from the one in the image the defaults are written back (`Version mismatch...`, `Config Data invalid in NV`).

The initial image of the table is in flash at `0x2c2d53e0`, and `0x2c2d3d28` lists the records as `(offset, total length)`: 56 distinct records, 51 plain and 5 with a count (41 x 68 B at `0x7c`, 10 x 169 B at `0x2a8`, 200 x 88 B at `0x45c`, 200 x 26 B at `0x4f0`, and one more with a loader at `0xc0e9...`); the counted ones keep their elements outside the table.

## Seen in the live file

`/user/kvdata.bin` is 30,242 bytes: a sequence of **0x200-byte slots, one record per slot**. A slot is a 0x14-byte header and then the data (the rest of the slot is zero):

| Offset | Size | Field |
|---|---|---|
| 0 | u16 | version / loaded index (the first slot's `2` is the table version) |
| 2 | u16 | crc: **CRC-16/CCITT-FALSE** (poly 0x1021, init 0xffff, check value `0x29b1`) over the data bytes |
| 4 | u16 | unknown (0 in all slots seen) |
| 6 | u16 | element count (0 for plain records) |
| 8 | u32 | data size, equal to the record's size in the image |
| 0xc, 0x10 | 2 x u32 | zero or a code pointer, not data |

The slots are in the order of the image's record list (`kvmap.py`, sorted by key), with all-zero slots in between (slots 6, 19, 26 and 27 of the copy we have). Matching the sizes in order gives every one of the 56 records a slot, and **all 56 crcs are valid**, which is how the layout was checked. The key (the offset in the RAM table) is therefore not stored in the file: it comes from the order, so decoding needs the image. `src/re/kvdecode.py` does it and prints each record; it prints the user's settings, so do not paste its output in an issue.

Records with a count have only their first element in the slot; the others live in separate files in `/user`, and the sizes match exactly:

| Key | Elements | File | Check |
|---|---|---|---|
| `0x45c` | 200 x 88 B | `/user/sport.cfg` (17,600 B) | the loader at `0x2c0ea4a0` writes the file in a loop of `0xc8` elements of `0x58` bytes |
| `0x4f0` | 200 x 26 B | `/user/sport_screen.bin` (5,200 B) | size, and the string is next to the `[KVSCREEN]` code |
| `0x2a8` | 10 x 169 B | `/user/bluetooth.cfg` (1,690 B) | size, and the file is opened next to the kv code (`0x2c0e9d10`) |
| `0x7c` | 41 x 68 B (2,788 B) | not on the watch we have | no file of that size in `/user`; `badge.cfg` and `coach_proto.bin` exist in the image but are not on this watch |

Checks against the code: the step goal of key `0x424` (`6000` here) and the layout of the display record read as the code expects. The remaining fields of `0x424` look like `90, 500, 1000, 12, 13, 10, 8, 30`, which fits the log line about duration, calories and floors, but that order is still a hypothesis.

## Records identified (from the code that reads them)

| Key | Size | What | How we know |
|---|---|---|---|
| `0x110` | 28 | display: `+4` screen timeout, `+8` brightness, `+0x14` idle return | `kv_display_*` getters (docs/symbols.csv) |
| `0x248` | 12 | vibration: the first word is the on/off flag that gates the motor | `kv_vibration_enabled` |
| `0x424` | 36 | daily goals: the first `u16` is the step goal (default 6,000, clamped to 1,000..49,000 by `0x2c0e5610`); the other fields are listed by the log "dailyactivity step, duration, calorie, floor, achour, extime..." but their order is a hypothesis | `0x2c0e5610`, and Salud reads the goal from here |
| `0x9d4` | 136 | `+8` bit 0 enables the World Clock row of the app list | `kv_worldclock_enabled` |
| `0xf8` | 4 | the app the double press of the side key opens (an index into the 14-window table) | docs/details.md |

Named from the log strings printed next to the code that reads or writes them (the key is the constant loaded into `r0` before the call to `kv_read`/`kv_write`; the strings are those within 160 bytes of the call):

| Key | Size | What | Evidence |
|---|---|---|---|
| `0xd4` | 16 | sleep mode: scheduled on/off, temporary-close switch, repeat, status | `[sleepmode kv] set ... onoff=%d` |
| `0xe58` | 12 | theater mode: duration, supported, on/off, status | `[theatermode kv] set ...` |
| `0x174` | 148 | user info (the heart-rate maximum is read from it) | `[D] userinfo hr max:%d` |
| `0x380` | 36 | heart-rate alert: switch and the high/low limits | `srv heartrate remind set sw,hw:%d,lw:%d,hv:%d,lv:%d` |
| `0x3f4` | 28 | sleep goal: set time, bed time, wake-up time | `sleepg set_time:%d,bed_time:%d,wakeup_time:%d` |
| `0x5e0` | 88 | phone / set-up info: connection, OOBE status, screen-off time, brightness | `phoneinfo conn:%d`, `oobeScreenOfftime` |
| `0x964` | 92 | reminders: master switch | `tmp.remind_switch %d` |
| `0xb0c` | 38 | account: child-account flag, "together" support code | `child account`, `together support code:%s` |
| `0xe78` | 48 | list of timers (hypothesis) | `add num:%d, timer:%u` |

Keys seen with no string nearby (so no name): `0x1c`, `0x2a8` apart from its file, `0x520` (written by `applist_store_order`, so the app order), `0x8ec` (read by `applist_get_order`), `0x580`, `0xb48`, `0xbd0`, `0xdbc`, `0xebc`, `0xf2c`, `0xfac`. Some keys are built at run time and were not found by this scan, so the list is not complete.

## What it is good for

Writing a record from an app (`kv_write`) changes the watch's real settings, so it would let an app set the brightness, the vibration or the goal. It is also a way to break the watch: the length must match, the version must stay, and the crc16 is kept by the write function. Reads are safe; **writes are untested on a watch.** Editing the file directly (a slot's data plus its crc) is possible now that the crc is known, but the firmware also checks the table version and the watch rewrites the file itself, so this is untested too.
