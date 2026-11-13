# Health data on the Fit3 (what the firmware keeps and where)

Read on one watch (SM-R390, R390XXU0AZA3), by reading its files over the serial file service (`/user/health/...` is `/nand/user/health/...`, read only, docs/protocol.md) and the RAM block of the activity page. [watch]

## Heart rate: `/nand/user/health/hr-point.rec`

20-byte header (the first u32 is the file size), then records of 12 bytes, oldest first:

| Offset | Size | Field |
|---|---|---|
| 0 | u32 | unix time, seconds |
| 4 | i16 | time zone, minutes (-300 for UTC-5): local = unix + zone x 60 |
| 6 | u8 | beats per minute (40 to 127 seen) |
| 7 | u8 | flag (5 on the first reading of a session, 1 after) |
| 9, 10 | u8 | a second value (the same, or another reading of the same minute) and a quality figure; not used |

About one record a minute while the watch is worn: 2,628 in two days. The file keeps growing; read the tail with a seek (`fs_file_size` `0x2c143144`, `fs_seek` `0x2c1430c4`). `hr-last.rec` (12 bytes: unix time, zone, bpm, flag) is the last one, but it was not updated as often as the point file.

## Activity: the firmware's RAM block at `0x3ce65d98` (0x238 bytes)

Identified against the stock Health page (5,758 steps, 209 kcal at the same time):

| Offset | Type | Meaning |
|---|---|---|
| `+0x204` | u32 | steps today |
| `+0x210` | float | kilocalories (209.98) |
| `+0x218` | float | distance, metres (4262.0) |
| `+0x208` | float | active minutes (63) |
| `+0x21c`, `+0x220`, `+0x228` | float | 201.8, 4096.2, 1525.0: not identified (other energy and distance figures) |

The daily goal of steps is a u16 at the start of the 36-byte KV record `0x424` (clamped 1000 to 49,000 by `0x2c0e5610`). Reading the block needs no call: it is a plain read of RAM (the app does it every 3 s). Floats are read as bits and converted by hand: the watch code has no libgcc.

## Other files (not decoded)

| File | Size | What it looks like |
|---|---|---|
| `dailysnap_mr4.rec` | 45,192 | 20-byte header, then records of 92 bytes, one a minute: unix time, zone, and floats of motion intensity. Not the step counter. |
| `stress-point.rec` | 50,212 | 20-byte header, then records of 16 bytes: unix time, zone, a value byte, flags |
| `spo2-last.rec`, `hr-resting.rec`, `health-time.rec`, `health-date.rec` | 12 to 596 | the SpO2 file was all zeros (no reading); `hr-resting` has 10-byte records |
| `sleep-*.slp` | missing | not created yet on this watch |

`src/watch/apps/salud.c` shows the activity and the last 36 heart-rate readings; `src/emu/test_health.py` tests it. These files hold personal data: pulled copies go in `local/health/` (git-ignored) and are never published.
