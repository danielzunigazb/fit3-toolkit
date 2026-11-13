/* SPDX-License-Identifier: Apache-2.0 */
/* t9.h: a small text entry for an app: twelve keys, multi-tap like a phone's keypad (2 = a, b, c, 2), drawn with ds.h. The text is ASCII, lower case.
 * The app gives the buffer and the label that shows it, and is told when DONE is tapped. The reply keyboard (kbd.c) is another thing: it belongs to the notification page. */
#ifndef F3_T9_H
#define F3_T9_H
#include "ds.h"
#define T9_KEY_W 78
#define T9_KEY_H 52
#define T9_TOP 154                                               /* the keys take the screen from here down: four rows of three */
typedef struct {
    char *buf; u32 max;                                          /* the text and its size (with the terminator) */
    lv_obj_t *field, *key[12]; const char *hint;                 /* the label that shows it; what it shows while the text is empty */
    void (*done)(void *ud); void *ud;
    u32 last_tap; u8 last_key, cycle, dirty, pad;
} T9;
/* the field is three lines tall: a longer text shows its end, behind two dots, so the user always sees what they are typing */
H void t9_show(T9 *t) {
    char tmp[52]; u32 n = 0; while (t->buf[n]) n++;
    if (n > 48) { tmp[0] = '.'; tmp[1] = '.'; mcopy(tmp + 2, t->buf + n - 46, 47); lv_label_set_text(t->field, tmp); }
    else lv_label_set_text(t->field, t->buf[0] ? t->buf : t->hint);
    set_text_color(t->field, t->buf[0] ? DS_TEXT : DS_TEXT_2);
}
H void t9_key(lv_event_t *e) {
    static const char *const KEYS[10] = { " ", ".,?!1", "abc2", "def3", "ghi4", "jkl5", "mno6", "pqrs7", "tuv8", "wxyz9" };
    if (lv_event_get_code(e) != EV_CLICKED) return;
    T9 *t = (T9 *)lv_event_get_user_data(e); lv_obj_t *o = lv_event_get_target(e); int k = -1;
    for (int i = 0; i < 12; i++) if (t->key[i] == o) k = i;
    if (k < 0) return;
    u32 n = 0, now = lv_tick_get(); while (t->buf[n]) n++;
    if (k == 11) { motor_once(DS_HAPTIC_TAP, 1); t->done(t->ud); return; }
    if (k == 9) { if (n) { t->buf[n - 1] = 0; t->dirty = 1; } t->last_key = 0xff; }
    else {
        int digit = k == 10 ? 0 : k + 1; const char *g = KEYS[digit]; u32 gn = 0; while (g[gn]) gn++;
        if (digit && n && t->last_key == (u8)digit && now - t->last_tap < 900) { t->cycle = (u8)((t->cycle + 1) % gn); t->buf[n - 1] = g[t->cycle]; }
        else if (n + 1 < t->max) { t->cycle = 0; t->buf[n++] = g[0]; t->buf[n] = 0; }
        else { motor_once(DS_HAPTIC_ALERT, 1); return; }         /* full */
        t->last_key = (u8)digit; t->last_tap = now; t->dirty = 1;
    }
    motor_once(DS_HAPTIC_TAP, 1); t9_show(t);
}
/* builds the field (a surface from y to the keys) and the keys on `root`; `done_text` is the caption of the last key */
H void t9_build(T9 *t, lv_obj_t *root, i32 y, char *buf, u32 max, const char *hint, const char *done_text, void (*done)(void *), void *ud) {
    static const char *const CAP[11] = { "1 .?", "2 abc", "3 def", "4 ghi", "5 jkl", "6 mno", "7 pqrs", "8 tuv", "9 wxyz", "<-", "0 _" };
    t->buf = buf; t->max = max; t->hint = hint; t->done = done; t->ud = ud; t->last_key = 0xff; t->dirty = 0;
    lv_obj_t *box = mk_box(root, DS_EDGE, y, SCREEN_W - 2 * DS_EDGE, T9_TOP - DS_S2 - y, DS_R_COMPACT, DS_SURFACE);
    t->field = mk_text(box, DS_S3, DS_S2, SCREEN_W - 2 * DS_EDGE - 2 * DS_S3, DS_T_LABEL, DS_TEXT, ALIGN_LEFT, ""); t9_show(t);
    for (int i = 0; i < 12; i++) {
        t->key[i] = mk_box(root, DS_EDGE + (i % 3) * (T9_KEY_W + DS_S2), T9_TOP + (i / 3) * (T9_KEY_H + DS_S2), T9_KEY_W, T9_KEY_H, DS_R_COMPACT, i == 11 ? DS_ACCENT : DS_SURFACE);
        ds_bg_pressed(t->key[i], i == 11 ? DS_ACCENT_P : DS_SURFACE_P);
        mk_text(t->key[i], 0, 14, T9_KEY_W, i == 11 ? DS_T_CAPTION : DS_T_LABEL, i == 11 ? DS_ON_ACCENT : DS_TEXT, ALIGN_CENTER, i == 11 ? done_text : CAP[i]);
        lv_obj_add_event_cb(t->key[i], t9_key, 0, t);
    }
}
#endif
