/* SPDX-License-Identifier: Apache-2.0 */
/* ds.h: the launcher's design system in code. Tokens first (colour, type, space, radius, size, haptics), then the components made of them. The description, the rules and the reasons are
 * in DESIGN.md at the project root; a value that is not a token here does not belong in launcher.c. Stock values (docs/details.md, section 6) are kept wherever the watch has one, so a
 * page of ours sits next to a stock page without a seam. Freestanding like ui.h: everything is `H` (static, unused ones are dropped). */
#ifndef F3_DS_H
#define F3_DS_H
#include "ui.h"
#include "dfont.h"                                           /* FT_DISPLAY: the digits of the hero numbers (URW Bookman Light, 48 px; only 0-9 : . - and space) */

/* ---- colour (RGB565, as the display shows it). Warm: amber and cream on the AMOLED's black; every surface is a brown, never a grey. Contrast (checked): TEXT on BG 19:1, on SURFACE 15:1;
 * TEXT_2 on BG 10:1, on SURFACE 7.9:1; ON_ACCENT on ACCENT 9.2:1, on ACCENT_P 5.9:1, on DANGER 5.4:1. White on amber would be 2:1: text on an accent is always ON_ACCENT. ---- */
#define DS_BG         0x0000                                     /* #000000 the page: black is off */
#define DS_SURFACE    0x28e2                                     /* #291c10 a tile, a quiet button: dark roasted brown */
#define DS_SURFACE_P  0x4183                                     /* #413018 the same while it is pressed */
#define DS_TEXT       0xff9b                                     /* #fff2de cream */
#define DS_TEXT_2     0xcd70                                     /* #cdae83 secondary text: warm sand */
#define DS_ACCENT     0xf524                                     /* #f6a520 amber: the one primary action of a view */
#define DS_ACCENT_P   0xcbe1                                     /* #cd7d08 the same while it is pressed */
#define DS_DANGER     0xeaa7                                     /* #ee5539 ember: a destructive action once it is armed; a battery at or under DS_BATTERY_LOW */
#define DS_OK         0x9e2d                                     /* #9cc66a sage: a state that is fine (always next to the words that say so) */
#define DS_ON_ACCENT  0x1881                                     /* #181008 the text on amber and on ember: dark brown */

/* ---- type: Roboto Medium, the sizes the watch has loaded. Only the characters 0x20 to 0x7a exist: no accented letters, and none of | { } ~ DEL. A character outside that range jumps into a null
 * fallback and hangs the interface: every text that comes from outside (the phone) is filtered first (bridge.h br_clean). ---- */
#define DS_T_DISPLAY  FT_CLOCK                                   /* 48 px: the time */
#define DS_T_HERO     FT_DISPLAY                                 /* 48 px digits: the time, the steps, the heart rate. ONLY the characters 0-9 : . - and space */
#define DS_T_TITLE    FT_NAME                                    /* 32 px: the title of a sheet */
#define DS_T_BODY     FT_BTN                                     /* 28 px: a button */
#define DS_T_LABEL    FT_SMALL                                   /* 24 px: a value, a line of detail */
#define DS_T_CAPTION  FT_TINY                                    /* 20 px: a tile's name, a section, a hint */

/* ---- space (a 4 px step; DS_EDGE is the one exception, it centres the grid on 256 px) ---- */
#define DS_EDGE       3                                          /* from the screen's edge to the grid */
#define DS_S1         4
#define DS_S2         8                                          /* between tiles, between buttons */
#define DS_S3         12                                         /* text inset, between a block and the next section */
#define DS_S4         16

/* ---- radius and size ---- */
#define DS_R_TILE     26                                         /* the stock card */
#define DS_R_COMPACT  22
#define DS_R_PILL     100
#define DS_ICON       64                                         /* every icon: the stock app-list size */
#define DS_TILE_W     121                                        /* a tile with a name: two per row */
#define DS_TILE_H     132
#define DS_COMPACT    78                                         /* a tile without a name: three per row */
#define DS_PILL_H     52
#define DS_PILL_W     244
#define DS_HEADER_H   96                                         /* the status header: the time, the battery and the date, and under them the steps and the heart rate */
#define DS_SECTION_H  32                                         /* a section title and the air under it */
#define DS_PAGE_TOP   48                                         /* where a page's content starts under its title */
#define DS_ROW_H      64                                         /* a list row */
#define DS_BATTERY_LOW 15

/* ---- haptics (the firmware's patterns; both respect the user's vibration setting) ---- */
#define DS_HAPTIC_TAP   1                                        /* something was accepted */
#define DS_HAPTIC_ALERT 4                                        /* something was removed, or refused */

#define DS_PRESSED 0x0020                                        /* LV_STATE_PRESSED as a style selector */
#define ds_bg_pressed(o, c)         F(0x2c27d53c, void, (lv_obj_t *, u16, u32))(o, c, DS_PRESSED)

/* ---- components ---- */
/* Tile: an app as a rounded surface with its icon. `name` gives the variant with a caption (two per row); without it the compact one (three per row). `src` is what lv_img_set_src takes:
 * an Icon, or the image-ID string of a stock icon. */
H lv_obj_t *ds_tile(lv_obj_t *parent, i32 x, i32 y, const void *src, const char *name) {
    i32 w = name ? DS_TILE_W : DS_COMPACT, h = name ? DS_TILE_H : DS_COMPACT;
    lv_obj_t *r = mk_box(parent, x, y, w, h, name ? DS_R_TILE : DS_R_COMPACT, DS_SURFACE);
    ds_bg_pressed(r, DS_SURFACE_P);
    lv_obj_t *im = lv_img_create(r); lv_img_set_src(im, src); lv_obj_set_pos(im, (w - DS_ICON) / 2, name ? DS_S3 - 2 : (h - DS_ICON) / 2);
    if (name) mk_text(r, DS_S1, DS_S3 + DS_ICON + DS_S1, w - 2 * DS_S1, DS_T_CAPTION, DS_TEXT, ALIGN_CENTER, name);     /* a long name wraps onto a second line */
    return r;
}
/* Section: the title of a group of tiles */
H lv_obj_t *ds_section(lv_obj_t *parent, i32 y, const char *text) { return mk_text(parent, DS_S3, y, SCREEN_W - 2 * DS_S3, DS_T_CAPTION, DS_TEXT_2, ALIGN_LEFT, text); }
/* Note: a quiet centred line (an empty state, a hint) */
H lv_obj_t *ds_note(lv_obj_t *parent, i32 y, const char *text) { return mk_text(parent, DS_S4, y, SCREEN_W - 2 * DS_S4, DS_T_CAPTION, DS_TEXT_2, ALIGN_CENTER, text); }
/* Pill: a full-width button. One DS_PRIMARY per view at most; DS_QUIET for the way out; DS_DANGER_QUIET for a destructive action before it is armed (ds_pill_arm turns it red and
 * renames it: destructive actions always take two taps); DS_OFF cannot be used now and says why in its caption. */
enum { DS_PRIMARY, DS_QUIET, DS_DANGER_QUIET, DS_OFF };
H lv_obj_t *ds_pill(lv_obj_t *parent, i32 y, const char *text, int variant, lv_obj_t **lab, void (*cb)(lv_event_t *), void *ud) {
    lv_obj_t *b = mk_box(parent, (SCREEN_W - DS_PILL_W) / 2, y, DS_PILL_W, DS_PILL_H, DS_R_PILL, variant == DS_PRIMARY ? DS_ACCENT : DS_SURFACE);
    lv_obj_t *l = mk_text(b, 0, 13, DS_PILL_W, DS_T_BODY, variant == DS_OFF ? DS_TEXT_2 : variant == DS_PRIMARY ? DS_ON_ACCENT : DS_TEXT, ALIGN_CENTER, text);       /* y 13: see mk_pill */
    if (variant != DS_OFF) ds_bg_pressed(b, variant == DS_PRIMARY ? DS_ACCENT_P : DS_SURFACE_P);
    if (lab) *lab = l;
    lv_obj_add_event_cb(b, cb, 0, ud);
    return b;
}
H void ds_pill_arm(lv_obj_t *b, lv_obj_t *lab, const char *text) { set_bg_color(b, DS_DANGER); set_text_color(lab, DS_ON_ACCENT); lv_label_set_text(lab, text); }
/* Title: the name of a page, one line at the top */
H lv_obj_t *ds_title(lv_obj_t *parent, const char *text) {
    lv_obj_t *l = mk_text(parent, DS_S3, DS_S2, SCREEN_W - 2 * DS_S3, DS_T_BODY, DS_TEXT, ALIGN_CENTER, text); lv_obj_set_size(l, SCREEN_W - 2 * DS_S3, 34); return l;
}
/* Row: one item of a list, full width: a title and, optionally, a line of detail. Each is one line: a longer text is cut, not wrapped. With `cb` it is a button. */
H lv_obj_t *ds_row(lv_obj_t *parent, i32 y, const char *title, const char *sub, void (*cb)(lv_event_t *), void *ud) {
    i32 w = SCREEN_W - 2 * DS_EDGE;
    lv_obj_t *r = mk_box(parent, DS_EDGE, y, w, DS_ROW_H, DS_R_COMPACT, DS_SURFACE);
    lv_obj_set_size(mk_text(r, DS_S3, sub ? 5 : 18, w - 2 * DS_S3, DS_T_LABEL, DS_TEXT, ALIGN_LEFT, title), w - 2 * DS_S3, 29);
    if (sub) lv_obj_set_size(mk_text(r, DS_S3, 35, w - 2 * DS_S3, DS_T_CAPTION, DS_TEXT_2, ALIGN_LEFT, sub), w - 2 * DS_S3, 24);
    if (cb) { ds_bg_pressed(r, DS_SURFACE_P); lv_obj_add_event_cb(r, cb, 0, ud); }
    return r;
}
/* Message: what a page shows instead of content: nothing there, waiting, no link, an error. A short title and one sentence that says what to do. */
H void ds_message(lv_obj_t *parent, const char *title, const char *note) {
    mk_text(parent, DS_S3, 128, SCREEN_W - 2 * DS_S3, DS_T_TITLE, DS_TEXT, ALIGN_CENTER, title);
    if (note) mk_text(parent, DS_S4, 180, SCREEN_W - 2 * DS_S4, DS_T_CAPTION, DS_TEXT_2, ALIGN_CENTER, note);
}
/* ScrollText: a long text under a page's title that scrolls vertically (an answer from FAN) */
H lv_obj_t *ds_scroll_text(lv_obj_t *parent, const char *text) {
    lv_obj_t *box = mk_box(parent, 0, DS_PAGE_TOP, SCREEN_W, SCREEN_H - DS_PAGE_TOP, 0, DS_BG);
    lv_obj_add_flag(box, FL_SCROLLABLE); lv_obj_set_scrollbar_mode(box, 0);
    mk_text(box, DS_S3, 0, SCREEN_W - 2 * DS_S3, DS_T_CAPTION, DS_TEXT, ALIGN_LEFT, text);
    return box;
}
/* Sheet: a full-screen layer over the page for one decision; the back gesture closes it */
H lv_obj_t *ds_sheet(lv_obj_t *scr) { return mk_root(scr); }
/* a text of the watch's own language pack, in its own font (a stock app's name, with every accent the language needs) */
H lv_obj_t *ds_text_native(lv_obj_t *parent, i32 x, i32 y, i32 w, u16 color, u32 align, u32 text_id) {
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_pos(l, x, y); lv_obj_set_width(l, w); set_text_color(l, color); set_text_align(l, align);
    set_text_font(l, text_id_get_font(text_id)); lv_label_set_text(l, text_id_get_string(text_id));
    return l;
}
#endif
