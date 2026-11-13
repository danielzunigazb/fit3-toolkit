# BLE receive path (AZA3) — what was read in the disassembly

Status: **read from the firmware only**. Nothing here was tested on a watch.
Addresses are for `R390XXU0AZA3` main image (base `0x2c080000`). Flash is
aliased: a pointer `0x0cXXXXXX` in a literal pool is the same code as
`0x2cXXXXXX`.

## Chain, from the radio to the protocol parser

1. GATT service registration: `app_samsung_add_samsungps` (`0x2c22ba70`) sends
   a RivieraWaves `GAPM` add-profile message for profile task id `0x62`. The
   attribute database (UUIDs) is **not in the main image**; it is most likely
   in the BLE stack in ROM. The 128-bit service UUID is therefore still
   unknown from this image alone. The same pattern exists for
   `app_encryption_add_samsungps` (`0x2c22b664`) and
   `app_smartthing_find_add_samsungps` (`0x2c22b8bc`).
2. `app_samsung_server_rx_data_received_handler` (`0x2c22b964`) logs
   `"%s length %d"`, hex-dumps the bytes, and calls a registered callback
   stored in RAM `0x2011a294`.
3. That callback is installed by the setter at `0x2c22baf0`, called from the
   driver init `0x2c228438`. The callback is `0x2c2283e0`: unless
   `0x2c1cbfc4()` returns 9, it forwards `(src=0xe, kind=0xb, data, len)` to
   `thread_bt_msg_driver_msg_send` (`0x2c14e80c`).
4. `0x2c14e80c` allocates a pool block (`0x4b1` bytes) and posts it to the
   `thread_bt_msg` queue. Block layout:

   | offset | size | meaning |
   |-------:|-----:|---------|
   | 0 | u16 | message type (`0xb` = received data) |
   | 2 | u8  | channel (index into the handler table) |
   | 3 | u8  | 0 |
   | 4 | u16 | length |
   | 6 | ... | payload |

5. The thread loop (`0x2c14e6fc`) waits on an event, pops the block, and calls
   `handler_table[channel]` (table at `0x2c2ef5c8`, 17 entries):

   | channel | handler |
   |--------:|---------|
   | 1  | `0x2c229104` (SPP factory, connect/disconnect/data) |
   | 10 | `0x2c229dfc` |
   | 11 | `0x2c229044` |
   | 13 | `0x2c229f1e` |
   | 14 | `0x2c2285e0` (**BLE**, log prefix `[DRIVER_BLE]`) |
   | 15 | `0x2c2291b0` |
   | 16 | `0x2c22977c` |

6. The BLE handler (`0x2c2285e0`), for type `0xb` (also `0x2803`): clamps the
   length to `< 0x3e8` (1000), restarts a 5 s timer (`0x2c228c3c(0x1388)`) and
   passes `(payload, length)` to **`0x2c10a114`**, the protocol entry point.
7. `0x2c10a114` checks two state functions (`0x2c10c418`, `0x2c10c05c`) and
   queues `{u8 0, u16 length, ptr}` to `0x2c10a800`. The framing code further
   down (`0x2c10a1c0`) uses header sizes 6/8 for frames shorter than 256 bytes
   and 8/10 for longer ones, so frames carry a 6–10 byte header.

## Not done yet

- Service/characteristic UUIDs (probably in ROM, not in this image).
- Header field meanings and the command ids of each `PROTO_*` module.
- The empty command table scanned at `0x2c38e118` (start == end in this image)
  was the SPP-side lookup `0x2c22bcf8`; it dispatches nothing here.

## Notes

- `0x2c22901c` is the **send** side (`bt_spp_factory_data_send`), not the
  parser. It was mistaken for the parser first.
- Nothing in this path touches the emergency SOS trigger.

## Frame header (read from `0x2c10ae8c`, the post-handshake parser)

"RX task handler" (`0x2c10a5d8`) pops `{u8 session, u16 len, ptr}` from a
queue and calls `0x2c10b3fc` (handshake) while the link is not negotiated, or
`0x2c10ae8c` afterwards. Log tag `PROTO_BLE`, module id `0x13`. The marker
strings `SAMSUNG_ACCESSARY__` / `SAMSUNG` and the names `ProPktM`, `ProPktC`,
`AckPktRec`, `FragErr` point to the Samsung Accessory Protocol framing.

The size `n` is the negotiated frame size (`0x2c10c00c`).

| `n` | bytes 0-1 | bytes 2-3 | next bytes | payload |
|-----|-----------|-----------|------------|---------|
| `<= 0xff` | equal to each other (`CrcErr` otherwise) | flags A, B | bytes 4,5 (counter, other) | at 6 (at 4 if A bit 2 set) |
| `> 0xff`  | u16 BE `L` | u16 BE `CRC16(L)` | bytes 4,5 = flags A, B; bytes 6,7 (counter, other) | at 8 (at 6 if A bit 2 set) |

`CRC16` is **CRC-16/ARC** (reflected, poly `0xA001`, init 0): table at
`0x2c2e04d8`, routine `0x2c10c430`; the table reproduces the check value
`0xBB3D` for `"123456789"`. `0x2c10c454` computes it over the two bytes of `L`
(high byte first).

Flag A (`bits 7..0`): `[7:6]` class (0 = `ProPktM`, 2 = `ProPktC`, others are
errors), `[5:3]` kind, `[2]` short-header flag, `[2:1]` fragment code (0 whole,
1 first, 2 middle, 3 last), `[0]` selects one of two handler groups
(`ErrWFRDt` strings vs `ErrWFURDt` strings). Flag B: `[7:5]`, `[4]`, `[3:0]`
stored as three fields, meaning not decoded. Byte 6 of a long frame is compared
with an expected counter that is incremented `& 0xf`, so it is a 4-bit
sequence number.

Class 2 (`ProPktC`): kind 1 = acknowledgement (`AckPktRec`), kind 2 = reset
(`0x2c10c070`), anything else is `ErrMsgTyp`.
Class 0 (`ProPktM`): kind must be 0 (data), anything else is `ErrMsgTyp`.

### Handshake (while not negotiated, `0x2c10b3fc` / `0x2c10b13c`)

The watch builds a `0x69`-byte request: byte 1 = `0x14`, bytes 2-3 and 4-5 =
`0x0401` (little endian), byte 6 = 3, then the string `SAMSUNG_ACCESSARY__`,
11 bytes of `xx:xx:xx:xx:xx:xx`-style data, a flag byte `1`, then the frame
sizes (`min(x, 0x1f4)` and `0x3d4`). The reply must have byte 1 = `0x15`
(`PdMsgErr` otherwise) and carries the phone's sizes (`errMTU` if <= 14,
`errAPDU` if <= 29), a manufacturer string (`MF:%s`) and more fields at
offsets `0x2c..0x68`. Sizes are clamped to 500 (frame) and 980 (APDU).

Not decoded: the exact reply layout, flag B, and the module/command ids
inside the APDU. `src/re/sap_frame.py` only implements what is listed above.
