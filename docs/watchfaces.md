# Watch faces on the Fit3 (what the firmware keeps, and what is decoded)

Read on one watch (SM-R390, R390XXU0AZA3): the folder listing was made by `src/watch/apps/wflist.c` (an installed app that walks the folders with the firmware's own `opendir`/`readdir`/`stat`), the files by the serial file service (read only, docs/protocol.md). These are Samsung's assets: keep copies in `local/wf/` (git-ignored), never redistribute.

## Layout on the watch

`/nand/system/wf/` holds six faces (`wf1` to `wf6`), the photo face `album/` and `gif/`. Each face is a folder:

| File | Size seen | What it is |
|---|---|---|
| `setting.bin` | 256 B | the manifest (below) |
| `styleN.bin` | 222 KB to 969 KB | one per colour/layout variant (wf5: 4, wf4 and wf2: 6, wf1: 3) |
| `aod.bin` | 49 KB to 931 KB | the always-on version |
| `preview.bin` | 299 KB to 598 KB | the pictures of the face picker: one per style |
| `font_*.bin` | 92 to 325 B | small descriptors of the number and text sprites per language (`font_en`, `font_ko`, `font_cn0`, `font_cn2`, `font_pt_rPT`) and of the counters (`font_0.bin`: label `WF_COUNT`) |

User faces would go in `/nand/user/wf/` with `wf_manager.cfg` (list: version, unique id, style) and `wf_special.cfg`; neither can be opened by the watch's directory call on this watch (`/nand/user` itself does not open), so they do not exist yet or live under another name. The firmware has the commands `AT^WF_ADD`, `WF_ADD_SPECIAL`, `WF_SET`, `WF_DELETE`, `WF_DATA_START/END/TIME/DATE/WEATHER/HEALTH/OTHERS` and `WF_IND_STATUS` (factory test family, framing not known, docs/protocol.md) and the phone protocol `PROTO_WATCHFACE`.

## Decoded

**Images** (`preview.bin`, the album): a 16-byte header, then raw RGB565 little endian: `u16 width, u16 height, u32 0x82 (format), u32 data size + 4, u32 0`. The album backgrounds are 256 x 402 (205,840 B); a preview is 178 x 280 and `preview.bin` repeats it once per style. Decoded and checked by eye (a dog and a cat on the album, the four colourways of wf5).

**`setting.bin`**: the ASCII `LQ_WF`, `0x12345678`, the version string (`00106` for wf5, `00108` for wf4), then at offset 48 a u16 (unique id: `0x2712` = 10002 for wf5, `0x2713` for wf4), a u16 style count (4 for wf5, 6 for wf4), `ff ff`, and `SM-R390_<version>_256x402` twice (the model, the face and the resolution).

**What a face is made of** (from the pictures): numeric and text fields at fixed places: time, date, weather, steps, heart rate, kilocalories, floors. Each style is one colourway of that arrangement. The digits are pre-rendered sprites, not a font file: `font_en.bin` lists the characters the face may draw (`0123456789`, degree, minus, `a.m.`, `p.m.`, `steps`, `bpm`, `kcal`, `floors`) with a width each.

## Not decoded

`styleN.bin` and `aod.bin` are a container: magic `0x12345678`, then u32 fields (wf5 `aod`: 5, 0x12c, 0x17558, 0x400, 0x144, 3, 2; wf5 `style0`: 0x13, 0x4cc, 0x54114, 0x400, 0x4e4, 1, 0): probably a count of entries and offsets to a table of widgets and to the images. That table (what each widget is, its place, its sprites) is the next thing to read; the six faces share the format, so comparing them should show it.

## Our own picture on the Album face (works, seen on a watch)

The pieces, read from the firmware (`ALBUM_ADD` `0x2c101520`, the face's creator `0x2c10612c`, the registry loader `0x2c0ffff4`, the setter `0x2c101458`):

- **A user picture** is `/user/wf/album/<name>.bmp`: raw RGB565 little endian, **no header**, width x height x 2 bytes, at most 256 x 402 (the firmware's own converter writes exactly that from the PNG the phone sends). Up to 13.
- **The registry** is `/user/wf/wf_special.cfg`, 1,444 bytes (`0x5a4`): `u8 count` at 0, `u8` selected index at 1, then entries of 0x44 bytes from offset 12 (the name, `<name>.bmp`, first). Offset 8 is `0xffffffff`.
- **When the face uses them:** `count != 0` **and** the byte at `+7` or at `+0x3c4` is non-zero. The phone's protocol sets both through `0x2c101458(flag, value)` when the album face is chosen. The watch clears `+7` itself when it saves (runtime state); **`+0x3c4` is kept**, so it is the one that makes the choice survive a restart. With both at zero the face shows the system default (`/nand/system/wf/album/album_bg2.bin`, the dog; `album_bg1.bin` is the cat).
- **`wf_manager.cfg`** (1,232 bytes): `u32` count of faces (7), `u32` id of the current face (`0xfe` = album), `u32` at offset 8 (`0xfe` = no picture chosen; the watch rewrites it at start-up).
- **The serial file service silently drops writes under `/user/wf/`** (it says "accepted" and the file never exists), though it writes under `/user/`. The firmware's own file calls do not have that restriction: `src/watch/apps/fondo.c` copies the picture from `/user/amber.bmp` (put there from the PC) and edits the registry from inside the watch, in chunks of 512 bytes, and keeps the first copy of the two files as `/user/wf_*.bak`. `Quitar` undoes it. Tested in the emulator by `src/emu/test_fondo.py` against a model of the file system.
- The watch needs a restart to read the registry again.

## What this opens, in order of cost

1. **More pictures** (up to 13) and a picker: easy now, the mechanism is known.
2. **A face of our own** (digits, ring, data fields, colourways): the manifest and the previews are easy; the style container (`styleN.bin`) is the work.
3. **A face as our own window**: full freedom, but the face is the home screen, so a mistake shows at every wake-up. Last, with a copy ready.
