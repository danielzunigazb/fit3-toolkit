# SPDX-License-Identifier: Apache-2.0
"""Render an installed .f3x app through the hub: screenshot_f3x.py APP.f3x OUTDIR (the first page of the app and, for notes, the editor; the apps that talk to the phone are rendered by test_bridge.py against a model of the phone)."""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(os.path.dirname(HERE)); sys.path.insert(0, HERE)
from screenshot import Screen, FONT_DIR, children
from probe_lvgl import A

class Shot(Screen):
    def __init__(self, image, fonts):
        super().__init__(image, fonts); self.user_files = {}; self.buf = b""; self.pos = 0; self.path = ""; self.sent = []
        e = self.e; e.hook(0x2c0c575c, self._open); e.hook(0x2c142dcc, self._read); e.hook(0x2c142ee0, self._write); e.hook(0x2c142c24, lambda em: 0); e.hook(0x2c1435e0, lambda em: 0)
        e.hook(0x2c112f58, lambda em: self.sent.append((em.arg(0), self.e.cstr(em.arg(1), em.arg(2)))) or 1)
    def _open(self, em):
        self.path = self.e.cstr(em.arg(0)); mode = self.e.cstr(em.arg(1)); self.pos = 0
        if mode == "r":
            if self.path not in self.user_files: return 0xfffffffe
            self.buf = self.user_files[self.path]; return 7
        self.buf = b""; return 7
    def _read(self, em):
        n = min(em.arg(2), len(self.buf) - self.pos); self.e.write(em.arg(1), self.buf[self.pos:self.pos + n]); self.pos += n; return n
    def _write(self, em): self.buf += self.e.read(em.arg(1), em.arg(2)); self.user_files[self.path] = self.buf; return em.arg(2)

def texts(e, o):
    out = []
    if e.u32(o) == 0x2c388a94: out.append(e.cstr(e.u32(o + 0x2c), 256))
    for c in children(e, o): out += texts(e, c)
    return out

def main(argv):
    app = argv[0]; out = argv[1] if len(argv) > 1 else os.path.join(ROOT, "local/shots/f3x"); os.makedirs(out, exist_ok=True)
    stem = os.path.splitext(os.path.basename(app))[0]
    s = Shot(os.path.join(ROOT, "local/re/main-hubtest.bin"), FONT_DIR); e = s.e; s.user_files["/user/hub/a00.f3x"] = open(app, "rb").read()
    s.seed_txt(); s.load_fonts(); scr = e.call(0x2c1ecc64, 0, 0); s.show(scr)
    click = lambda o: e.call(A["event_send"], o, 7, 0, max_insns=500_000_000)
    def shot(name): e.call(0x2c2782a8, scr); s.settle(2); s.png(os.path.join(out, name + ".png")); print("wrote", name)
    rows = children(e, children(e, scr)[1]); click(rows[1]); root = children(e, scr)[2]; s.settle(3)
    shot(stem + "-main")
    if stem == "notes":
        row = children(e, root)[2]; click(row); s.settle(2); shot("notes-editor-empty")
        buttons = children(e, root)[2:]
        key4 = next(b for b in buttons if "4 ghi" in texts(e, b)); click(key4); click(key4); s.tick += 1000; click(key4); click(key4); click(key4)
        key0 = next(b for b in buttons if "0 _" in texts(e, b)); click(key0)
        key6 = next(b for b in buttons if "6 mno" in texts(e, b)); click(key6); s.tick += 1000
        key8 = next(b for b in buttons if "8 tuv" in texts(e, b)); click(key8); click(key8); click(key8)
        shot("notes-editor-text")
    return 0
if __name__ == "__main__": sys.exit(main(sys.argv[1:]))
