# SPDX-License-Identifier: Apache-2.0
"""autoname.py: names the functions of the main image from the identifier-like strings they load (log tags and function names the firmware prints), and prints a map by subsystem prefix.

    python3 -I src/re/autoname.py local/re/main-stock.bin local/re/autonames.csv      (the stock main image, linked at 0x2c080000; the output is derived from Samsung's firmware: keep it in local/)

A function is a Thumb prologue (`push {..., lr}`) to the next one; a string is "loaded" when a pc-relative `ldr` of the function reads a literal that points at a printable identifier (`[a-z][a-z0-9_]{7,}` with an underscore). Where a function loads exactly one such string it is
named by it (confidence "one"); with several, they are all listed (confidence "many") and the first one is the guess. Needs capstone (the emulator's venv has it)."""
import csv, collections, re, struct, sys
import capstone
BASE = 0x2c080000
ID = re.compile(rb"^[a-z][a-z0-9_]{7,60}$")

def main(img, out):
    raw = open(img, "rb").read(); md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB); md.skipdata = True
    def cstr(addr):
        o = addr - BASE
        if not 0 <= o < len(raw): return None
        e = raw.find(b"\0", o, o + 80)
        return raw[o:e] if e > 0 else None
    funcs = []; refs = collections.defaultdict(list); cur = None
    for i in md.disasm(raw[0x1000:0x2bd000], BASE + 0x1000):
        if i.mnemonic in ("push", "push.w") and "lr" in i.op_str: cur = i.address; funcs.append(cur)
        elif cur and i.mnemonic in ("ldr", "ldr.w") and "[pc" in i.op_str:
            try: imm = int(i.op_str.split("#")[1].rstrip("]"), 16)
            except Exception: continue
            t = ((i.address + 4) & ~3) + imm
            if 0 <= t - BASE < len(raw) - 4:
                v = struct.unpack_from("<I", raw, t - BASE)[0]
                if BASE <= v < BASE + len(raw):
                    s = cstr(v)
                    if s and ID.match(s) and b"_" in s: refs[cur].append(s.decode())
    rows = []
    for f in funcs:
        names = list(dict.fromkeys(refs.get(f, [])))
        if names: rows.append((hex(f), names[0], "one" if len(names) == 1 else "many", ";".join(names)))
    with open(out, "w", newline="") as fh:
        w = csv.writer(fh); w.writerow(["address", "name", "confidence", "all_candidates"]); w.writerows(rows)
    pref = collections.Counter(r[1].split("_")[0] for r in rows if r[2] == "one")
    print("functions found: %d; named: %d (one string: %d)" % (len(funcs), len(rows), sum(1 for r in rows if r[2] == "one")))
    print("subsystems by prefix (functions named by exactly one string):"); [print("  %-14s %4d" % (k, v)) for k, v in pref.most_common(40)]

if __name__ == "__main__":
    if len(sys.argv) != 3: sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
