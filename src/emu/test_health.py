# SPDX-License-Identifier: Apache-2.0
"""test_health.py: the Salud app on the real firmware's LVGL, fed with a heart-rate file (the one pulled from the watch, local/health/hr-point.rec, never in git) and a chosen clock.

    (the images of test_launcher.py, and:)  python3 -I src/f3x/build_f3x.py src/watch/apps/salud.c local/f3x/salud.f3x --icon-kind health
    local/venv/bin/python -I src/emu/test_health.py [outdir]"""
import os, struct, sys, time
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(os.path.dirname(HERE)); sys.path.insert(0, HERE)
from screenshot import children
from screenshot_f3x import texts
from test_launcher import Home
HR = "/nand/user/health/hr-point.rec"


def synth(n, end_ts, tz=-300, step=60):
    """a file in the watch's format: 20-byte header, then n records of 12 bytes ending at end_ts"""
    hdr = bytearray(20); hdr[0:4] = struct.pack("<I", 20 + 12 * n); out = bytes(hdr)
    for i in range(n):
        v = 55 + (i * 7) % 60
        out += struct.pack("<IhBBBBBB", end_ts - (n - 1 - i) * step, tz, v, 1, 0, v, 70, 0)
    return out


class Watch(Home):
    def __init__(self, files):
        super().__init__("local/re/main-launcherc.bin", files); e = self.e
        e.hook(0x2c143144, lambda em: len(self.user_files[HR]))                                  # fs_file_size
        def seek(em): self.buf = self.user_files[HR]; self.pos = em.arg(2); return 0                # fs_seek(fd, off lo, off hi, whence): the next read starts there
        e.hook(0x2c1430c4, seek)
    def open(self, name): self.click(next(t for t in self.named() if texts(self.e, t) == [name])); self.settle(2); return children(self.e, self.scr)[-1]
    def t(self): return texts(self.e, children(self.e, self.scr)[-1])


def main(argv):
    out = argv[0] if argv else os.path.join(ROOT, "local/shots/apps"); os.makedirs(out, exist_ok=True)
    f = os.path.join(ROOT, "local/f3x/salud.f3x")
    if not os.path.exists(f) or not os.path.exists(os.path.join(ROOT, "local/re/main-launcherc.bin")): print("SKIP: need local/f3x/salud.f3x and the launcher images"); return 0
    res = []; ok = lambda name, cond, obs="": (res.append(bool(cond)), print("%s %s %s" % ("PASS" if cond else "FAIL", name, obs)))
    import calendar
    local_epoch = calendar.timegm((2026, 11, 12, 18, 42, 5, 0, 0, 0))                          # the RTC of test_launcher says 18:42:05; the zone is -300 min: unix = local + 5 h
    end_ts = local_epoch + 5 * 3600 - 120                                                      # the newest record: 2 min before now
    files = {"/user/hub/a00.f3x": open(f, "rb").read(), HR: synth(100, end_ts)}
    w = Watch(files); e = w.e
    w.user_files[HR] = synth(100, end_ts)
    ACT = 0x3ce65d98; f = lambda x: struct.unpack("<I", struct.pack("<f", x))[0]                  # the firmware's activity block, as the stock page showed it: 5758 steps, 209.98 kcal, 4262 m
    try: e.uc.mem_map(0x3ce65000, 0x1000)
    except Exception: pass
    e.w32(ACT + 0x204, 5758); e.w32(ACT + 0x210, f(209.977)); e.w32(ACT + 0x218, f(4262.016)); e.w32(ACT + 0x208, f(63.0))
    e.hook(0x2c0ea3c8, lambda em: e.write(em.arg(1), struct.pack("<H", 6000)) or 0)                # the daily goal: KV record 0x424
    root = w.open("Salud")
    t = w.t(); ok("the page: steps in the ring, the goal, the last reading and its age, kcal and km", t == ["Salud", "5758", "pasos", "meta 6000", str(55 + (99 * 7) % 60), "lpm", "hace 2 min", "209 kcal", "4.3 km"], t)
    boxes = lambda wd: [k for k in children(e, root) if e.u32(k) != 0x2c388a94 and not (e.u32(k + 0x24) & 1) and w.box(k)[2] - w.box(k)[0] + 1 == wd]
    dots = boxes(6); ok("the ring: 48 dots of 6 px, the first at the top and the 13th (a quarter) at the right", len(dots) == 48 and w.box(dots[0])[1] < 70 and abs((w.box(dots[0])[0] + 3) - 128) <= 1 and w.box(dots[12])[0] > 200, [w.box(d)[:2] for d in (dots[0], dots[12])] if len(dots) == 48 else len(dots))
    e.w32(ACT + 0x204, 5761); e.w32(ACT + 0x210, f(210.4)); e.w32(ACT + 0x218, f(4299.9)); w.run_timers(); w.settle(2)
    ok("it follows the firmware's block (3 s timer): 5761 steps, 210 kcal, 4.3 km", [w.t()[1], w.t()[7], w.t()[8]] == ["5761", "210 kcal", "4.3 km"], w.t())
    e.w32(ACT + 0x204, 5758); e.w32(ACT + 0x210, f(209.977)); e.w32(ACT + 0x218, f(4262.016))
    vals = [55 + (i * 7) % 60 for i in range(100 - 36, 100)]
    bars = boxes(2); ok("36 bars, the newest on the right, 2 px wide", len(bars) == 36 and w.box(bars[-1])[0] > w.box(bars[0])[0], len(bars))
    h = [w.box(b)[3] - w.box(b)[1] + 1 for b in bars]; ok("a bar is as tall as its value: (bpm - 40) x 0.60", h[-1] == (vals[-1] - 40) * 60 // 100 and h[0] == (vals[0] - 40) * 60 // 100, (h[0], h[-1]))
    ok("the app writes nothing to the watch's storage", [k for k in w.user_files if k not in files and k != HR] == [], list(w.user_files))
    w.shot(out, "salud")
    w.user_files[HR] = synth(100, end_ts + 600, step=60); w.now[2] = 52; w.run_timers(); w.settle(2); ok("the timer re-reads the file every 10 s: ten minutes later, 'hace 2 min' again as new records came in", w.t()[6] == "hace 2 min", w.t()[:7])
    w.now[3] = 22; w.run_timers(); w.settle(2); ok("with no new reading for hours the age says so", w.t()[6] == "hace 4 h", w.t()[6]); w.shot(out, "salud-stale")
    w.user_files[HR] = synth(0, 0); w.run_timers(); w.settle(2); ok("an empty file: no data, said in words, and no bars", w.t()[4:7] == ["--", "lpm", "sin medidas"] and not boxes(2), w.t()[4:7])
    w.user_files.pop(HR); w.run_timers(); w.settle(2); ok("a missing file: the same", w.t()[4:7] == ["--", "lpm", "sin medidas"], w.t()[4:7])
    e.w32(ACT + 0x204, 6500); w.run_timers(); w.settle(2); ok("past the goal the ring is full (and the number keeps counting)", w.t()[1] == "6500", w.t()[1]); w.shot(out, "salud-full")
    real = os.path.join(ROOT, "local/health/hr-point.rec")
    if os.path.exists(real):                                                                       # the file as the watch wrote it: the last reading must be the one decoded by hand (see docs/health.md)
        d = open(real, "rb").read(); n = (len(d) - 20) // 12; last = struct.unpack_from("<IhB", d, 20 + 12 * (n - 1)); w.user_files[HR] = d
        w.now[:] = [0, 5, 0, 0, 0, 0, 0, 0, 0]; lt = last[0] + last[1] * 60 + 120; y = time.gmtime(lt); w.now[:] = [0, y.tm_sec, y.tm_min, y.tm_hour, y.tm_mday, y.tm_mon, y.tm_year, 0, 0]
        w.run_timers(); w.settle(2); ok("the real file of the watch: its last reading, 2 min before the clock", [w.t()[4], w.t()[6]] == [str(last[2]), "hace 2 min"], (w.t()[4:7], last))
    print("%d of %d passed" % (sum(res), len(res))); return 0 if all(res) else 1


if __name__ == "__main__": sys.exit(main(sys.argv[1:]))
