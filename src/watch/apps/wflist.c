/* SPDX-License-Identifier: Apache-2.0 */
/* apps/wflist.c: a diagnostic, read-only: lists the folders where the watch keeps the user's faces and pictures (and what /user and /nand/user look like to the firmware) with the sizes of their files, into /user/wf_list.txt,
 * which can then be read from the PC (docs/watchfaces.md). It only opens, reads and closes directories and asks for sizes; the one thing it writes is that text file, once, in chunks. */
#include "ds.h"
#include "icons.h"

typedef struct { char *out; u32 len, cap, n; lv_obj_t *root; } Wl;
static u32 wl_len(const char *s) { u32 n = 0; while (s[n]) n++; return n; }
static void wl_put(Wl *w, const char *s) { while (*s && w->len + 1 < w->cap) w->out[w->len++] = *s++; }
static void wl_dir(Wl *w, const char *path, int depth, int maxd) {
    i32 h = fs_opendir(path); u8 *ent;
    if (h < 0) { wl_put(w, path); wl_put(w, "  (cannot open)\n"); return; }
    for (int guard = 0; guard < 160 && w->n < 600; guard++) {
        char child[120], num[12];
        ent = 0;
        if ((fs_readdir(h, &ent) & 0xff) || !ent) break;
        const char *name = (const char *)(ent + 0xb);
        if (name[0] == '.' || wl_len(path) + wl_len(name) + 2 >= sizeof child) continue;
        { char *p = child; const char *q = path; while (*q) *p++ = *q++; if (p > child && p[-1] != '/') *p++ = '/'; q = name; while (*q) *p++ = *q++; *p = 0; }
        w->n++;
        i32 h2 = fs_opendir(child);
        if (h2 >= 0) {                                           /* a directory */
            fs_closedir(h2); wl_put(w, child); wl_put(w, "/\n");
            if (depth < maxd) wl_dir(w, child, depth + 1, maxd);
        } else {
            u8 st[0x60]; fill8(st, 0, sizeof st); fs_stat(child, st);
            wl_put(w, child); wl_put(w, "  "); putn(num, *(u32 *)(st + 0x20), 1); wl_put(w, num); wl_put(w, "\n");
        }
    }
    fs_closedir(h);
}
static void *wl_create(lv_obj_t *root) {
    Wl *w = (Wl *)app_malloc(sizeof(Wl)); char msg[TXT_MAX], *p;
    if (!w) return 0;
    fill8(w, 0, sizeof(Wl)); w->root = root; w->cap = 6000; w->out = (char *)app_malloc(w->cap);
    ds_title(root, "Esferas");
    if (!w->out) { ds_message(root, "Sin memoria", 0); return w; }
    wl_put(w, "== /nand/user/album\n"); wl_dir(w, "/nand/user/album", 0, 1);
    wl_put(w, "== /nand/user/wf\n"); wl_dir(w, "/nand/user/wf", 0, 2);
    wl_put(w, "== /user\n"); wl_dir(w, "/user", 0, 1);
    wl_put(w, "== /user/wf\n"); wl_dir(w, "/user/wf", 0, 2);
    wl_put(w, "== /user/album\n"); wl_dir(w, "/user/album", 0, 1);
    wl_put(w, "== /nand/user/daily\n"); wl_dir(w, "/nand/user/daily", 0, 1);
    wl_put(w, "== /nand/user/health\n"); wl_dir(w, "/nand/user/health", 0, 1);
    { i32 fd = f_open("/user/wf_list.txt", "w"); if (fd >= 0) { for (u32 o = 0; o < w->len; o += 512) f_write(fd, w->out + o, w->len - o < 512 ? w->len - o : 512); f_close(fd); } }
    p = putn(msg, w->n, 1); put(p, " entradas");
    ds_message(root, "Listo", msg);
    return w;
}
static int wl_back(void *st) { (void)st; return 0; }
static void wl_destroy(void *st) { Wl *w = (Wl *)st; if (w->out) app_free(w->out); app_free(w); }
APP_EXPORT(wflist, "Esferas (dx)", &ICON_COUNTER, wl_create, wl_back, wl_destroy)
