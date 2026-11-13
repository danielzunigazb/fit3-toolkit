# SPDX-License-Identifier: Apache-2.0
"""test_fondo.py: the Fondo app (src/watch/apps/fondo.c) on the real firmware's LVGL, with a small in-memory file system: the copy of the picture, the backups, exactly which bytes of the two
configuration files change, and that Quitar puts them back. The real wf_manager.cfg and wf_special.cfg of a watch (local/wf/, never in git) are used if they are there.

    python3 -I src/f3x/build_f3x.py src/watch/apps/fondo.c local/f3x/fondo.f3x --icon-kind fan   (and the images of test_launcher.py)
    local/venv/bin/python -I src/emu/test_fondo.py"""
import os, struct, sys
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(os.path.dirname(HERE)); sys.path.insert(0, HERE)
from screenshot import children
from screenshot_f3x import texts
from test_launcher import Home
SRC, DST, MAN, SPE = "/user/amber.bmp", "/user/wf/album/amber.bmp", "/user/wf/wf_manager.cfg", "/user/wf/wf_special.cfg"


class W(Home):
    def __init__(self, fs):
        super().__init__("local/re/main-launcherc.bin", {"/user/hub/a00.f3x": fs["/user/hub/a00.f3x"]}); e = self.e
        self.fs = {k: bytearray(v) for k, v in fs.items()}; self.h = {}; self.nfd = 10; self.writes = []
        e.hook(0x2c0c575c, self._fopen); e.hook(0x2c142dcc, self._fread); e.hook(0x2c142ee0, self._fwrite); e.hook(0x2c142c24, lambda em: self.h.pop(em.arg(0), None) and 0)
        e.hook(0x2c143144, lambda em: len(self.fs[self.h[em.arg(0)][0]])); e.hook(0x2c1430c4, self._fseek)
    def _fopen(self, em):
        path, mode = self.e.cstr(em.arg(0)), self.e.cstr(em.arg(1))
        if mode == "r":
            if path not in self.fs: return 0xfffffffe
        elif mode == "w": self.fs[path] = bytearray()
        elif mode == "a": self.fs.setdefault(path, bytearray())
        self.nfd += 1; self.h[self.nfd] = [path, 0, mode]; return self.nfd
    def _fseek(self, em): self.h[em.arg(0)][1] = em.arg(2); return 0
    def _fread(self, em):
        h = self.h[em.arg(0)]; d = self.fs[h[0]]; n = min(em.arg(2), len(d) - h[1]); self.e.write(em.arg(1), bytes(d[h[1]:h[1] + n])); h[1] += n; return n
    def _fwrite(self, em):
        h = self.h[em.arg(0)]; data = self.e.read(em.arg(1), em.arg(2)); self.fs[h[0]] += data; self.writes.append((h[0], len(data))); return em.arg(2)
    def root(self): return children(self.e, self.scr)[-1]
    def t(self): return texts(self.e, self.root())
    def tap(self, text):
        b = next(b for b in self.all(self.root()) if texts(self.e, b) and texts(self.e, b)[0] == text and self.e.u32(b) != 0x2c388a94); self.click(b); self.settle(1)
    def all(self, o): out = []; [out.extend([c] + self.all(c)) for c in children(self.e, o)]; return out


def main(argv):
    f = os.path.join(ROOT, "local/f3x/fondo.f3x")
    if not os.path.exists(f) or not os.path.exists(os.path.join(ROOT, "local/re/main-launcherc.bin")): print("SKIP: need local/f3x/fondo.f3x and the launcher images"); return 0
    res = []; ok = lambda name, cond, obs="": (res.append(bool(cond)), print("%s %s %s" % ("PASS" if cond else "FAIL", name, obs)))
    man = open(os.path.join(ROOT, "local/wf/wf_manager.cfg"), "rb").read() if os.path.exists(os.path.join(ROOT, "local/wf/wf_manager.cfg")) else bytes([7, 0, 0, 0, 0xfe, 0, 0, 0, 0xfe, 0, 0, 0]) + bytes(1220)
    spe = open(os.path.join(ROOT, "local/wf/wf_special.cfg"), "rb").read() if os.path.exists(os.path.join(ROOT, "local/wf/wf_special.cfg")) else bytes(8) + b"\xff\xff\xff\xff" + bytes(1432)
    img = bytes((i * 7 + (i >> 8)) & 255 for i in range(205824))
    w = W({"/user/hub/a00.f3x": open(f, "rb").read(), MAN: man, SPE: spe}); e = w.e
    w.click(next(t for t in w.named() if texts(e, t) == ["Fondo ambar"])); w.settle(2)
    ok("the page", w.t()[:5] == ["Fondo", "Esfera Album", "Pon el fondo ambar", "Instalar", "Quitar"], w.t())
    w.tap("Instalar"); ok("no picture in /user: it says so, writes nothing and touches no configuration", w.t()[1:3] == ["Falta el fondo", "amber.bmp no esta en /user"] and not w.writes and w.fs[MAN] == man and w.fs[SPE] == spe, w.t())
    w.fs[SRC] = bytearray(img); w.writes.clear()
    w.tap("Instalar"); ok("it starts copying", w.t()[1] == "Copiando...", w.t()[1:3])
    for _ in range(420):
        w.run_timers()
        if w.t()[1] != "Copiando...": break
    w.settle(2)
    ok("the picture is copied byte for byte (205,824 bytes)", bytes(w.fs[DST]) == img, len(w.fs.get(DST, b"")))
    ok("it was written in pieces of at most 512 bytes (the interface is never blocked for long)", max(n for p, n in w.writes if p == DST) <= 512 and len([1 for p, n in w.writes if p == DST]) == 402, max(n for p, n in w.writes if p == DST))
    ok("the two configuration files were backed up first, unchanged", bytes(w.fs["/user/wf_manager.bak"]) == man and bytes(w.fs["/user/wf_special.bak"]) == spe)
    ok("the message says it is done and to restart", w.t()[1:3] == ["Listo", "Reinicia el reloj"], w.t()[1:3])
    ns, nm = bytes(w.fs[SPE]), bytes(w.fs[MAN])
    ok("wf_special.cfg: count 1, the album-mode byte (+7) set and the name at offset 12; nothing else changes", ns[0] == 1 and ns[7] == 1 and ns[0x3c4] == 1 and ns[12:12 + 10] == b"amber.bmp\0" and ns[1:7] == spe[1:7] and ns[8:12] == spe[8:12] and ns[12 + 0x40:0x3c4] == spe[12 + 0x40:0x3c4] and ns[0x3c5:] == spe[0x3c5:] and len(ns) == 1444, ns[:24].hex(" "))
    diff = [i for i in range(len(man)) if man[i] != nm[i]]
    ok("wf_manager.cfg: only the bytes 8 to 11 change (the picture in use: 0xfe becomes 0)", diff == [8] and nm[8:12] == bytes(4) and len(nm) == 1232, diff)
    w.writes.clear(); w.tap("Instalar"); ok("again with the picture already there: no new copy, just the registration (and the first backup is not overwritten)", not any(p == DST for p, n in w.writes) and w.t()[1] == "Listo" and bytes(w.fs["/user/wf_special.bak"]) == spe and bytes(w.fs[SPE])[7] == 1 and bytes(w.fs[SPE])[0x3c4] == 1, w.t()[1:3])
    w.tap("Quitar"); ok("Quitar puts count 0 and the 0xfe back, and nothing else", bytes(w.fs[SPE]) == spe and bytes(w.fs[MAN]) == man and w.t()[1:3] == ["Quitado", "Reinicia el reloj"], w.t()[1:3])
    bad = W({"/user/hub/a00.f3x": open(f, "rb").read(), MAN: man[:100], SPE: spe}); bad.fs[SRC] = bytearray(img)
    bad.click(next(t for t in bad.named() if texts(bad.e, t) == ["Fondo ambar"])); bad.settle(2); bad.tap("Instalar")
    for _ in range(420):
        bad.run_timers()
        if bad.t()[1] != "Copiando...": break
    bad.settle(2); ok("a configuration file of an unexpected size is refused and left alone", bad.t()[1] == "Fallo" and bytes(bad.fs[SPE]) == spe, bad.t()[1:3])
    print("%d of %d passed" % (sum(res), len(res))); return 0 if all(res) else 1


if __name__ == "__main__": sys.exit(main(sys.argv[1:]))
