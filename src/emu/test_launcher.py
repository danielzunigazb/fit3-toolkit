# SPDX-License-Identifier: Apache-2.0
"""test_launcher.py: the launcher (src/watch/launcher.c, hub.c built with --launcher) checked and rendered with the firmware's own LVGL code.

    for n in "launcher:" "launcherc:--no-native-transitions"; do
      python3 -I src/build_firmware.py local/firmware/stock-aza3.bin local/work/${n%%:*}.bin --hub tennis,counter,dice,mars --launcher ${n#*:}
      python3 -I src/firmware/fwx.py local/work/${n%%:*}.bin main local/re/main-${n%%:*}.bin; done
    python3 -I src/build_firmware.py local/firmware/stock-aza3.bin local/work/launcher0.bin --hub none --launcher --no-native-transitions
    python3 -I src/firmware/fwx.py local/work/launcher0.bin main local/re/main-launcher0.bin
    local/venv/bin/python -I src/emu/test_launcher.py [outdir] [app.f3x ...]

What is Python here, because the emulator has no .data and no files of the watch: the RTC, the battery, the texts of the language pack (the names of the stock apps) and the stock
app-list icons: the firmware's table of them is pointed at stand-in icons drawn by mkicon.py (the real ones are images on the watch). Everything else is the code that runs on the watch.
Pictures go to outdir (default local/shots/launcher). The .f3x files given are "installed" in the slots. Limits as in screenshot.py."""
import os, struct, sys
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(os.path.dirname(HERE)); sys.path.insert(0, HERE); sys.path.insert(0, os.path.join(ROOT, "src", "assets"))
from screenshot import FONT_DIR, children
from screenshot_f3x import Shot, texts
from probe_lvgl import A
import mkicon
LABEL = 0x2c388a94; TABLE = 0x2c30021c; CFG = "/user/launcher.cfg"
NAMES = {172: "Salud", 173: "Multimedia", 174: "Buscar mi teléfono", 175: "Tiempo", 176: "Calendario", 177: "Ajustes", 4315: "Temporizador", 179: "Alarma", 180: "Cronómetro", 181: "Reloj mundial", 182: "Cámara", 183: "Calculadora"}
CREATE = int(os.environ.get("F3_CREATE", "0x2c1ecc64"), 0); EVENT = int(os.environ.get("F3_EVENT", "0x2c1ecd68"), 0); WIN = int(os.environ.get("F3_WINDOW", "0x2b"), 0)


class Home(Shot):
    def __init__(self, image, files=None):
        super().__init__(os.path.join(ROOT, image), FONT_DIR); e = self.e; self.user_files = dict(files or {})
        self.now = [0, 5, 42, 18, 12, 11, 2026, 0, 0]; self.pct = 87; self.wm = []; self.freed = []; self.timers = {}; self.deleted = []; self.motor = []
        e.hook(0x2c0cb8f4, lambda em: e.write(em.arg(0), struct.pack("<9i", *self.now)) or 0)
        e.hook(0x2c0a9e24, lambda em: e.w32(em.arg(1), self.pct) or 0)
        e.hook(0x2c225ddc, lambda em: self.wm.append((em.arg(0), em.arg(1) & 0xffffffff, em.arg(2), em.arg(3))) or 0)
        e.hook(0x2c0eb018, lambda em: self.freed.append(em.arg(0)) or 0); e.hook(0x2c1423f8, lambda em: self.motor.append(em.arg(0)) or 1)
        e.hook(0x2c28a3f0, self._timer); e.hook(0x2c28a448, lambda em: self.deleted.append(em.arg(0)) or 0)
        self.seed_txt(); self.load_fonts()
        self.strs = {}                                       # the language pack: text id -> a string in memory; the font of every text: Roboto Medium 28
        for tid, s in NAMES.items(): a = self.mem(len(s.encode()) + 1); e.write(a, s.encode() + b"\0"); self.strs[tid] = a
        e.hook(0x2c2138cc, lambda em: self.strs[em.arg(0)]); e.hook(0x2c2134fc, lambda em: e.u32(0x2010b0e0 + 4 * 7))
        for r, kind in enumerate(("mars", "tennis", "hub", "counter", "dice", "hub", "counter", "tennis", "dice", "mars", "hub", "counter")):       # stand-ins for the stock icons
            data = mkicon.encode_indexed4(*mkicon.draw(kind, 64)); d = self.mem(len(data)); e.write(d, data); dsc = self.mem(16)
            e.write(dsc, struct.pack("<IIII", 9 | 64 << 10 | 64 << 21, len(data), d, 0))
            if e.u16(TABLE + 12 * r + 2) != WIN:
                for base in (TABLE, TABLE - 0x20000000): e.write(base + 12 * r + 4, struct.pack("<I", dsc))
        self.scr = e.call(CREATE, 0, 0); self.show(self.scr); self.win = e.u32(self.scr + 0x10); self.list = children(e, self.scr)[0]
        e.w32(0x200fb4e4 + WIN * 0x30 + 0x10, self.scr)      # the window record's screen, as the window manager keeps it
    def mem(self, n): a = self.heap; self.heap += (n + 15) & ~15; self.e.write(a, bytes(n)); return a
    def _timer(self, em): t = self.mem(32); self.e.w32(t + 8, em.arg(0)); self.e.w32(t + 12, em.arg(2)); self.timers[t] = em.arg(0); return t
    def run_timers(self):
        for t, cb in list(self.timers.items()):
            if t not in self.deleted: self.e.call(cb, t)
    def kids(self): return children(self.e, self.list)
    def tiles(self): return [k for k in self.kids() if self.e.u32(k) != LABEL]
    def named(self): return [k for k in self.tiles() if texts(self.e, k)]
    def compact(self): return [k for k in self.tiles() if not texts(self.e, k)]
    def head(self): return [self.e.cstr(self.e.u32(k + 0x2c), 64) for k in self.kids() if self.e.u32(k) == LABEL]
    def box(self, o): self.e.call(A["update_layout"], self.scr, max_insns=500_000_000); return [self.e.i32(o + 0x14 + 4 * k) for k in range(4)]
    def click(self, o, code=7): self.e.call(A["event_send"], o, code, 0, max_insns=500_000_000)
    def hold(self, o): self.click(o, 1); self.click(o, 5); self.click(o); return children(self.e, self.scr)[-1]       # press, long press, the release: the sheet
    def sheet(self): k = children(self.e, self.scr); return k[-1] if len(k) > 1 else None
    def pill(self, text): return next(b for b in children(self.e, self.sheet()) if texts(self.e, b) == [text])
    def pins(self): c = self.user_files.get(CFG); return [x - 1 for x in struct.unpack("<I6H", c)[1:] if x] if c else None
    def shot(self, out, name): self.e.call(0x2c2782a8, self.scr); self.settle(2); self.png(os.path.join(out, name + ".png")); print("wrote", os.path.join(out, name + ".png"))
    def scroll(self, y): self.e.call(0x2c27be64, self.list, y, 0, max_insns=500_000_000)


def main(argv):
    out = argv[0] if argv else os.path.join(ROOT, "local/shots/launcher"); f3x = argv[1:]; os.makedirs(out, exist_ok=True)
    for img in ("local/re/main-launcher.bin", "local/re/main-launcherc.bin"):
        if not os.path.exists(os.path.join(ROOT, img)): print("SKIP: need %s (see the docstring)" % img); return 0
    res = []; ok = lambda name, cond, obs="": (res.append(bool(cond)), print("%s %s %s" % ("PASS" if cond else "FAIL", name, obs)))
    size = lambda r: (r[2] - r[0] + 1, r[3] - r[1] + 1)
    # --- the default build: an app is a window of its own
    h = Home("local/re/main-launcher.bin"); e = h.e
    ok("the screen holds the page only", len(children(e, h.scr)) == 1, len(children(e, h.scr)))
    ok("status header, two sections and the hint", h.head() == ["18:42", "87%", "ju 12 nov", "", "Mis apps", "Del reloj", "Deja pulsada una app para fijarla arriba"], h.head())
    t = h.named(); ok("our apps have named tiles, in order", [texts(e, x) for x in t] == [["Tennis Score"], ["Counter"], ["Dice"], ["Mars Production"]], [texts(e, x) for x in t])
    c = h.compact(); ok("the watch's own apps are compact tiles: all the stock list but the entry this page replaces", len(c) == 11, len(c))
    b = [h.box(x) for x in t]; cb = [h.box(x) for x in c]
    ok("named tiles are 121 x 132, two per row, 8 px apart", all(size(r) == (121, 132) for r in b) and [r[0] for r in b] == [3, 132, 3, 132] and b[2][1] - b[0][3] - 1 == 8, b)
    ok("compact tiles are 78 x 78, three per row, on the same edges", all(size(r) == (78, 78) for r in cb) and [r[0] for r in cb[:4]] == [3, 89, 175, 3] and cb[2][2] == b[1][2], cb[:4])
    ok("nothing overlaps: the sections follow each other", b[0][1] >= 96 + 24 and cb[0][1] > b[3][3] + 24, (b[0], b[3], cb[0]))
    ok("without favourites there is no dock", h.pins() is None and h.tiles()[0] == t[0])
    h.shot(out, "home"); h.scroll(330); h.shot(out, "home-watch"); h.scroll(0)
    try: e.uc.mem_map(0x3ce65000, 0x1000)
    except Exception: pass
    e.w32(0x3ce65d98 + 0x204, 5758); h.run_timers(); ok("the header shows the steps of today under the time (a read of the firmware's activity block)", h.head()[3] == "5758 pasos", h.head()[:4])
    e.w32(0x3ce65d98 + 0x204, 0); h.run_timers(); ok("no steps (0): nothing is shown, not a made-up value", h.head()[3] == "", h.head()[:4])
    h.now[2] = 43; h.now[3] = 9; h.now[4] = 1; h.now[5] = 3; h.now[6] = 2027; h.pct = 12; h.run_timers()
    ok("the timer keeps the header current (1 March 2027 is a Monday)", h.head()[:3] == ["09:43", "12%", "lu 1 mar"], h.head()[:3])
    h.pct = 0; h.now[5] = 0; h.run_timers(); ok("an unknown battery or date shows nothing, not a wrong value", h.head()[:3] == ["09:43", "", ""], h.head()[:3])
    h.click(t[2]); ok("a tap asks the window manager for the app's window (id 0x10, the app's index, the stock slide)", h.wm == [(2, 0x10, 2, 0xf)] and h.motor == [1], h.wm)
    h.click(t[1]); h.click(c[0]); ok("more taps while it opens are ignored", len(h.wm) == 1, h.wm)
    e.call(EVENT, 0, 0, 0); h.click(c[0]); ok("back on the page, a tap on a stock tile opens its window as the stock list does (Health: 8)", h.wm[1:] == [(2, 8, 0, 0xf)], h.wm)
    e.call(0x2c27b164, h.scr); ok("leaving the page deletes its timer and frees its state", list(h.timers) == h.deleted and h.win in h.freed, (h.deleted, h.win in h.freed))
    # --- apps inside the page's window, installed apps, favourites, removal
    files = {"/user/hub/a%02d.f3x" % i: open(f, "rb").read() for i, f in enumerate(f3x)}
    h = Home("local/re/main-launcherc.bin", files); e = h.e; t = h.named(); c = h.compact()
    ok("installed apps follow the built-in ones", len(t) == 4 + len(f3x), [texts(e, x) for x in t])
    s = h.hold(c[2]); ok("a long press on a stock tile: its name from the language pack, what it is, fix or cancel", texts(e, s) == [NAMES[174].encode().decode("latin1"), "App del reloj", "Cancelar", "Fijar arriba"], texts(e, s))
    h.shot(out, "sheet-watch")
    h.scroll(330); y0 = e.call(0x2c27b6c6, h.list, max_insns=500_000_000)
    h.click(h.pill("Fijar arriba")); y1 = e.call(0x2c27b6c6, h.list, max_insns=500_000_000); ok("the rebuild after a pin keeps the place in the page (it does not jump to the top)", y0 > 100 and abs(y1 - y0) <= 2, (y0, y1)); h.scroll(0)
    ok("Fijar arriba saves it (16 bytes) and the page gets a dock", h.pins() == [0x202] and len(h.user_files[CFG]) == 16 and h.sheet() is None and h.tiles()[0] not in h.named() and h.box(h.tiles()[0])[:2] == [3, 96], (h.pins(), h.box(h.tiles()[0])))
    ok("the sections moved down by the dock", h.box(h.named()[0])[1] == 96 + 86 + 4 + 32, h.box(h.named()[0]))
    h.click(h.tiles()[0]); ok("a tap on the favourite opens it", h.wm[-1] == (2, 0x22, 0, 0xf), h.wm)
    e.call(EVENT, 0, 0, 0); s = h.hold(h.tiles()[0]); ok("a favourite can be released", texts(e, s)[-1] == "Soltar", texts(e, s)); h.click(h.pill("Cancelar"))
    ok("Cancelar closes the sheet and changes nothing", h.sheet() is None and h.pins() == [0x202])
    for k in (1, 0, 3, 4, 5): h.hold(h.named()[1] if k == 1 else h.compact()[-11:][k]); h.click(h.pill("Fijar arriba"))
    ok("six favourites, in the order they were fixed", h.pins() == [0x202, 1, 0x200, 0x203, 0x204, 0x205] and len(h.tiles()) - len(h.named()) - 11 == 6, h.pins())
    h.shot(out, "home-dock")
    s = h.hold(h.named()[0]); ok("with six the button is off and says why", texts(e, s)[-1] == "Favoritos llenos", texts(e, s)); h.shot(out, "sheet-full")
    n = len(h.user_files[CFG]); h.motor.clear(); h.click(children(e, s)[-1]); ok("... and a tap on it is refused with the alert pattern", h.sheet() is not None and h.pins()[-1] == 0x205 and h.motor == [4], h.motor)
    e.call(A["event_send"], h.scr, 12, 0); ok("the back gesture closes a sheet, not the page", h.sheet() is None and not any(w[0] == 3 for w in h.wm), h.wm)
    h.hold(h.tiles()[1]); h.click(h.pill("Soltar")); ok("Soltar frees a place and keeps the order", h.pins() == [0x202, 0x200, 0x203, 0x204, 0x205], h.pins())
    h2 = Home("local/re/main-launcherc.bin", h.user_files); ok("the favourites are there when the page is built again", len(h2.tiles()) - len(h2.named()) - 11 == 5 and h2.pins() == h.pins(), h2.pins())
    if f3x:
        h = h2; e = h.e; first = h.named()[4]; name = texts(e, first)[0]
        h.hold(first); h.click(h.pill("Fijar arriba")); first = h.named()[4]
        s = h.hold(first); ok("an installed app: its name, its size, release, remove, cancel", texts(e, s)[0] == name and texts(e, s)[1].startswith("Instalada, ") and texts(e, s)[2:] == ["Cancelar", "Quitar", "Soltar"], texts(e, s))
        h.shot(out, "sheet-installed")
        h.click(h.pill("Quitar")); ok("Quitar asks once more and removes nothing yet", texts(e, s)[2:] == ["Cancelar", "Confirmar", "Soltar"] and h.user_files["/user/hub/a00.f3x"] != b"F3X-GONE", texts(e, s)); h.shot(out, "sheet-confirm")
        h.click(h.pill("Confirmar")); ok("Confirmar removes the app, its favourite and rebuilds the page", h.user_files["/user/hub/a00.f3x"] == b"F3X-GONE" and len(h.named()) == 3 + len(f3x) and 0x100 not in h.pins() and len(h.pins()) == 5 and h.head()[0] == "18:42", (len(h.named()), h.pins()))
        h.run_timers(); ok("the timer drives the rebuilt header", h.head()[0] == "18:42", h.head()[:3])
        h.scroll(250); h.shot(out, "home-apps"); h.scroll(0)
    h.click(h.named()[1]); root = children(e, h.scr)[-1]
    ok("a tap opens the app (here inside the page's window)", "Counter" in texts(e, root) and e.u32(h.win + 16) != 0, texts(e, root)); h.settle(3)
    e.call(A["event_send"], h.scr, 12, 0); ok("the back gesture returns to the page", len(children(e, h.scr)) == 1 and e.u32(h.win + 16) == 0, len(children(e, h.scr)))
    # --- a favourite whose app is gone, and no apps of ours at all
    if os.path.exists(os.path.join(ROOT, "local/re/main-launcher0.bin")) and CREATE == 0x2c1ecc64:
        h = Home("local/re/main-launcher0.bin", {CFG: struct.pack("<I6H", 0x3148434c, 0x101, 0x208, 0, 0, 0, 0)})
        ok("with no app of ours the section says how to get one; the watch's apps are still there", h.head()[4:] == ["Mis apps", "Instala apps desde el PC", "Del reloj", "Deja pulsada una app para fijarla arriba"] and len(h.compact()) == 12, h.head())
        ok("a favourite whose app is gone is dropped, the next one moves up", len(h.tiles()) == 12 and h.box(h.tiles()[0])[:2] == [3, 96], len(h.tiles())); h.shot(out, "home-empty")
    print("%d of %d passed" % (sum(res), len(res))); return 0 if all(res) else 1


if __name__ == "__main__": sys.exit(main(sys.argv[1:]))
