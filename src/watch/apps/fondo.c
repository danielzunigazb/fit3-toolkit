/* SPDX-License-Identifier: Apache-2.0 */
/* apps/fondo.c: puts a picture of our own on the Album watch face, from inside the watch (docs/watchfaces.md). The serial file service silently drops writes under /user/wf/, the firmware's own file calls do not.
 *   Instalar   copies /user/amber.bmp (raw RGB565, 256 x 402, no header: 205,824 bytes; put there from the PC) to /user/wf/album/amber.bmp, 512 bytes every 40 ms so the interface never stalls, then registers it:
 *              wf_special.cfg gets count 1, the name "amber.bmp" at offset 12 and 1 at offsets 7 and 0x3c4 (the album face uses its own pictures only when count != 0 and one of those two bytes is set,
 *              what the phone's protocol does through 0x2c101458; the watch clears +7 when it saves, +0x3c4 is kept: that is the one that survives a restart); wf_manager.cfg gets 0 at offset 8 (the picture in use; 0xfe means none). The two files are first copied to /user/wf_manager.bak and /user/wf_special.bak.
 *   Quitar     puts count 0 and 0xfe back. The picture stays where it is. Either way the watch reads the two files again when it restarts.
 * Layouts read from the firmware (ALBUM_ADD 0x2c101520): the list is the u8 count, then entries of 0x44 bytes from offset 12, the name first. */
#include "ds.h"
#include "icons.h"

#define SRC "/user/amber.bmp"
#define DST "/user/wf/album/amber.bmp"
#define MAN "/user/wf/wf_manager.cfg"
#define SPE "/user/wf/wf_special.cfg"
#define IMG_SIZE 205824u
#define MAN_SIZE 1232u
#define SPE_SIZE 1444u
#define CHUNK 512
typedef struct { lv_obj_t *root, *l_msg, *l_sub, *b_in, *b_out; lv_timer_t *tm; u32 off; u8 busy; char t_msg[TXT_MAX], t_sub[TXT_MAX]; } Fondo;

static void say(Fondo *s, const char *msg, const char *sub) { set_txt(s->l_msg, s->t_msg, msg); set_txt(s->l_sub, s->t_sub, sub); }
static u32 size_of(const char *path) { i32 fd = f_open(path, "r"); u32 n; if (fd < 0) return 0xffffffffu; n = f_size(fd); f_close(fd); return n; }
/* the two configuration files, edited in place (install 1: register amber.bmp and select it; 0: back to none). 0 = done */
static int edit_cfg(int install) {
    u8 *man = (u8 *)app_malloc(MAN_SIZE), *spe = (u8 *)app_malloc(SPE_SIZE); int r = 1;
    if (!man || !spe) goto out;
    if (size_of(MAN) != MAN_SIZE || size_of(SPE) != SPE_SIZE || !blob_load(MAN, man, MAN_SIZE) || !blob_load(SPE, spe, SPE_SIZE)) { r = 2; goto out; }
    if (install && size_of("/user/wf_manager.bak") == 0xffffffffu) { blob_save("/user/wf_manager.bak", man, MAN_SIZE); blob_save("/user/wf_special.bak", spe, SPE_SIZE); }       /* the first backup is the original: never overwritten */
    if (install) {
        const char *n = "amber.bmp"; u32 i = 0;
        spe[0] = 1; spe[7] = 1; spe[0x3c4] = 1; fill8(spe + 12, 0, 0x40); while (n[i]) { spe[12 + i] = (u8)n[i]; i++; }
        man[8] = 0; man[9] = man[10] = man[11] = 0; man[13] = 0;
    } else {
        spe[0] = 0; spe[7] = 0; spe[0x3c4] = 0; fill8(spe + 12, 0, 0x40);
        man[8] = 0xfe; man[9] = man[10] = man[11] = 0;
    }
    blob_save(SPE, spe, SPE_SIZE); blob_save(MAN, man, MAN_SIZE); r = 0;
out:
    if (man) app_free(man);
    if (spe) app_free(spe);
    return r;
}
static void fail(Fondo *s, const char *why) { if (s->tm) { lv_timer_del(s->tm); s->tm = 0; } s->busy = 0; motor_once(DS_HAPTIC_ALERT, 1); say(s, "Fallo", why); }
static void tick(lv_timer_t *t) {
    Fondo *s = (Fondo *)timer_user_data(t); u8 buf[CHUNK]; u32 n = IMG_SIZE - s->off, got = 0; i32 fd; char b[TXT_MAX], *p;
    if (n > CHUNK) n = CHUNK;
    fd = f_open(SRC, "r"); if (fd < 0) { fail(s, "No leo amber.bmp"); return; }
    f_seek(fd, (long long)s->off, 0);
    while (got < n) { i32 r = f_read(fd, buf + got, n - got); if (r <= 0) break; got += (u32)r; }
    f_close(fd);
    if (got != n) { fail(s, "Lectura corta"); return; }
    fd = f_open(DST, "a"); if (fd < 0) { fail(s, "No escribo en wf/album"); return; }
    if ((u32)f_write(fd, buf, n) != n) { f_close(fd); fail(s, "Escritura corta"); return; }
    f_close(fd);
    s->off += n;
    if (s->off < IMG_SIZE) { p = putn(b, s->off * 100u / IMG_SIZE, 1); put(p, " %"); say(s, "Copiando...", b); return; }
    lv_timer_del(s->tm); s->tm = 0; s->busy = 0;
    if (size_of(DST) != IMG_SIZE) { fail(s, "El tamano no cuadra"); return; }
    if (edit_cfg(1)) { fail(s, "No pude registrarlo"); return; }
    motor_once(DS_HAPTIC_TAP, 1); say(s, "Listo", "Reinicia el reloj");
}
static void on_click(lv_event_t *e) {
    Fondo *s = (Fondo *)lv_event_get_user_data(e); lv_obj_t *t = lv_event_get_target(e);
    if (lv_event_get_code(e) != EV_CLICKED || s->busy) return;
    motor_once(DS_HAPTIC_TAP, 1);
    if (t == s->b_out) { if (edit_cfg(0)) say(s, "Fallo", "No pude quitarlo"); else say(s, "Quitado", "Reinicia el reloj"); return; }
    if (size_of(SRC) != IMG_SIZE) { say(s, "Falta el fondo", "amber.bmp no esta en /user"); motor_once(DS_HAPTIC_ALERT, 1); return; }
    if (size_of(DST) == IMG_SIZE) { if (edit_cfg(1)) say(s, "Fallo", "No pude registrarlo"); else say(s, "Listo", "Reinicia el reloj"); return; }       /* already copied: only register it */
    { i32 fd = f_open(DST, "w"); if (fd < 0) { say(s, "Fallo", "No creo el fichero"); return; } f_close(fd); }      /* empty it: the copy appends */
    s->off = 0; s->busy = 1; say(s, "Copiando...", "0 %"); s->tm = lv_timer_create(tick, 40, s);
}
static void *fondo_create(lv_obj_t *root) {
    Fondo *s = (Fondo *)app_malloc(sizeof(Fondo)); if (!s) return 0;
    fill8(s, 0, sizeof(Fondo)); s->root = root;
    ds_title(root, "Fondo");
    s->l_msg = mk_text(root, DS_S3, 70, SCREEN_W - 2 * DS_S3, DS_T_TITLE, DS_TEXT, ALIGN_CENTER, "Esfera Album");
    s->l_sub = mk_text(root, DS_S3, 118, SCREEN_W - 2 * DS_S3, DS_T_CAPTION, DS_TEXT_2, ALIGN_CENTER, "Pon el fondo ambar");
    s->b_in = ds_pill(root, SCREEN_H - 2 * (DS_S2 + DS_PILL_H) + 4, "Instalar", DS_PRIMARY, 0, on_click, s);
    s->b_out = ds_pill(root, SCREEN_H - DS_S2 - DS_PILL_H + 2, "Quitar", DS_QUIET, 0, on_click, s);
    return s;
}
static int fondo_back(void *st) { Fondo *s = (Fondo *)st; return s->busy; }       /* while it copies, back does nothing */
static void fondo_destroy(void *st) { Fondo *s = (Fondo *)st; if (s->tm) lv_timer_del(s->tm); app_free(s); }
APP_EXPORT(fondo, "Fondo ambar", &ICON_COUNTER, fondo_create, fondo_back, fondo_destroy)
