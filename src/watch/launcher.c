/* SPDX-License-Identifier: Apache-2.0 */
/* launcher.c: the launcher, the page of hub.c when it is built with --launcher. Included by hub.c (one translation unit), drawn only with ds.h (the design system; DESIGN.md).
 *
 * One scrolling page, top to bottom:
 *   the status header     the time (24 h, from the RTC), the battery, the date; refreshed once a second
 *   the favourites        up to six apps the user fixed there, three per row, icons only (a dock)
 *   "Mis apps"            our apps: the built-in ones, then the installed .f3x files, two per row with their names
 *   "Del reloj"           the watch's own apps, read from the firmware's app-list table (its icons, its windows), three per row
 * A tap opens the app: ours as hub.c does, a stock one with the call the stock list makes. A long press opens a sheet: fix it to the favourites or release it, and for an installed
 * app remove it (two taps). The favourites are 16 bytes in L_CFG, written only when they change.
 * An item is a u16: 0.. a built-in app (index in APPS), ID_INST + slot an installed one, ID_STOCK + row a row of the stock table. */
#include "health.h"
#define L_CFG "/user/launcher.cfg"
#define L_MAGIC 0x3148434cu                                      /* "LCH1" */
#define ID_INST 0x100
#define ID_STOCK 0x200
#define STOCK_ROWS 12
typedef struct { u16 kind, window; const char *icon; u16 text, flag; } StockRow;       /* a row of the stock app list (docs/details.md, section 2): the window it opens, its icon's image ID, its name's text ID */
#define STOCK(r) ((const StockRow *)(0x2c30021cu + 12u * (r)))
typedef struct { u32 magic; u16 pin[L_PINS]; } LCfg;

/* ---- the status header ---- */
static void home_tick(Win *w) {
    static const char DAYS[] = "do\0lu\0ma\0mi\0ju\0vi\0sa", MONTHS[] = "ene\0feb\0mar\0abr\0may\0jun\0jul\0ago\0sep\0oct\0nov\0dic";
    static const u8 KEY[12] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
    i32 t[9]; u32 pct = 0; char b[TXT_MAX], *p;
    fill8(t, 0, sizeof t); rtc_get_time(t);
    p = putn(b, (u32)t[3] % 24u, 2); p = put(p, ":"); putn(p, (u32)t[2] % 60u, 2); set_txt(w->l_time, w->t_time, b);
    b[0] = 0;
    if (t[5] >= 1 && t[5] <= 12 && t[4] >= 1 && t[4] <= 31 && t[6] > 0) {              /* the day of the week is computed from the date (Sakamoto), 0 = Sunday */
        u32 y = (u32)t[6] - (t[5] < 3), wd = (y + y / 4u - y / 100u + y / 400u + KEY[t[5] - 1] + (u32)t[4]) % 7u;
        p = put(b, DAYS + wd * 3u); p = put(p, " "); p = putn(p, (u32)t[4], 1); p = put(p, " "); put(p, MONTHS + (u32)(t[5] - 1) * 4u);
    }
    set_txt(w->l_date, w->t_date, b);
    b[0] = 0; power_ctrl(1, &pct, 1);
    if (pct) { p = putn(b, pct > 100 ? 100 : pct, 1); put(p, "%"); }                   /* 0 = not known: nothing is shown rather than a wrong number */
    if (b[0] != w->t_bat[0] || b[1] != w->t_bat[1] || b[2] != w->t_bat[2]) set_text_color(w->l_bat, pct <= DS_BATTERY_LOW ? DS_DANGER : DS_TEXT_2);
    set_txt(w->l_bat, w->t_bat, b);
    /* under the time: the steps of today (a read of the firmware's RAM, every second) and the last heart rate (the file, every 30 s, and only if it is not older than 30 min) */
    if (!(w->hr_tick++ % 30)) { u32 age = 0, hr = hr_tail(&age); w->hr = (u8)(hr && age < 1800u ? hr : 0); }
    u32 steps = ACT(0x204); p = b; b[0] = 0;
    if (steps && steps < 1000000u) { p = putn(p, steps, 1); p = put(p, " pasos"); }
    if (w->hr) { if (b[0]) p = put(p, "  "); p = putn(p, w->hr, 1); put(p, " lpm"); }
    set_txt(w->l_act, w->t_act, b);
}
static void on_home_timer(lv_timer_t *t) { Win *w = (Win *)timer_user_data(t); if (w->l_time) home_tick(w); }
static void add_header(Win *w) {
    w->l_time = mk_text(w->list, DS_S3, DS_S2, 176, DS_T_HERO, DS_TEXT, ALIGN_LEFT, "");
    w->l_bat = mk_text(w->list, 146, DS_S3 + 2, SCREEN_W - 146 - DS_S3, DS_T_LABEL, DS_TEXT_2, ALIGN_RIGHT, "");
    w->l_date = mk_text(w->list, 146, 44, SCREEN_W - 146 - DS_S3, DS_T_CAPTION, DS_TEXT_2, ALIGN_RIGHT, "");
    w->l_act = mk_text(w->list, DS_S3, 62, SCREEN_W - 2 * DS_S3, DS_T_CAPTION, DS_TEXT_2, ALIGN_LEFT, "");
    w->t_act[0] = 0; w->hr_tick = 0;
    w->t_time[0] = w->t_date[0] = w->t_bat[0] = 0; home_tick(w);
}

/* ---- items ---- */
#ifdef F3X_LOADER
static const Inst *l_inst(Win *w, u16 id) { for (int k = 0; k < w->ninst; k++) if (w->inst[k]->slot == id - ID_INST) return w->inst[k]; return 0; }
#endif
static int l_valid(Win *w, u16 id) {
    (void)w;
    if (id < ID_INST) return id < (u16)NAPPS;
#ifdef F3X_LOADER
    if (id < ID_STOCK) return l_inst(w, id) != 0;
#endif
    return id >= ID_STOCK && id < ID_STOCK + STOCK_ROWS && STOCK(id - ID_STOCK)->window != WINDOW_ID;      /* our own row is this page */
}
static const void *l_icon(Win *w, u16 id) {
    (void)w;
    if (id >= ID_STOCK) return STOCK(id - ID_STOCK)->icon;
#ifdef F3X_LOADER
    if (id >= ID_INST) return &l_inst(w, id)->icon;
#endif
    return APPS[id]->icon;
}
static const char *l_name(Win *w, u16 id) {                      /* of our apps; a stock app's name is a text ID */
    (void)w;
#ifdef F3X_LOADER
    if (id >= ID_INST) return l_inst(w, id)->name;
#endif
    return APPS[id]->name;
}
static int l_pin_at(Win *w, u16 id) { for (int k = 0; k < L_PINS; k++) if (w->pin[k] == id + 1) return k; return -1; }
static void l_unpin(Win *w, int k) { for (; k < L_PINS - 1; k++) w->pin[k] = w->pin[k + 1]; w->pin[L_PINS - 1] = 0; }
static void l_save(Win *w) { LCfg c; c.magic = L_MAGIC; mcopy(c.pin, w->pin, sizeof c.pin); blob_save(L_CFG, &c, sizeof c); }
static void page_load(Win *w) { LCfg c; if (blob_load(L_CFG, &c, sizeof c) && c.magic == L_MAGIC) mcopy(w->pin, c.pin, sizeof c.pin); }

/* ---- opening ---- */
static void l_launch(Win *w, u16 id) {
    motor_once(DS_HAPTIC_TAP, 1);
    if (id >= ID_STOCK) { w->busy = 1; wm_open(2, STOCK(id - ID_STOCK)->window, 0, 0xf); return; }      /* exactly what the stock list does on a tap */
#if HOST_MODE
    w->busy = 1; wm_open(2, HOST_ID, id, 0xf);                   /* host_create takes the same numbers: an index, or 0x100 + slot */
#else
    if (id < ID_INST) { open_app(w, APPS[id]); return; }
#ifdef F3X_LOADER
    const App *a; void *blk = inst_load(id - ID_INST, &a);
    if (!blk) { motor_once(DS_HAPTIC_ALERT, 1); return; }        /* a damaged or incompatible file: refused */
    open_app(w, a);
    if (w->cur) w->blk = blk; else app_free(blk);
#endif
#endif
}

/* ---- the sheet of a long press ---- */
static void l_close_sheet(Win *w) { if (w->sheet) { lv_obj_del(w->sheet); w->sheet = 0; } w->armed = 0; w->b_pin = w->b_rm = w->b_cancel = 0; }
static void on_sheet(lv_event_t *e) {
    Win *w = (Win *)lv_event_get_user_data(e); lv_obj_t *t = lv_event_get_target(e);
    if (lv_event_get_code(e) != EV_CLICKED || !w->sheet) return;
    int k = l_pin_at(w, w->sel);
    if (t == w->b_pin) {
        if (k >= 0) l_unpin(w, k);
        else if (!w->pin[L_PINS - 1]) { for (k = 0; w->pin[k]; k++) { } w->pin[k] = w->sel + 1; }
        else { motor_once(DS_HAPTIC_ALERT, 1); return; }         /* the favourites are full (the button says so) */
        l_save(w); motor_once(DS_HAPTIC_TAP, 1); l_close_sheet(w); build_list(w);
    } else if (t == w->b_rm) {
#ifdef F3X_LOADER
        if (!w->armed) { w->armed = 1; ds_pill_arm(w->b_rm, w->l_rm, "Confirmar"); motor_once(DS_HAPTIC_TAP, 1); return; }
        char path[32]; slot_path(path, w->sel - ID_INST); blob_save(path, "F3X-GONE", 8);
        if (k >= 0) { l_unpin(w, k); l_save(w); }
        motor_once(DS_HAPTIC_ALERT, 1); l_close_sheet(w); build_list(w);
#endif
    } else if (t == w->b_cancel) { motor_once(DS_HAPTIC_TAP, 1); l_close_sheet(w); }
}
static void l_sheet(Win *w, u16 id) {
    char b[TXT_MAX], *p; i32 y = SCREEN_H - DS_S2 - DS_PILL_H + 2; int inst = id >= ID_INST && id < ID_STOCK, pinned = l_pin_at(w, id) >= 0, full = w->pin[L_PINS - 1] != 0;
    lv_obj_t *s = w->sheet = ds_sheet(w->scr); w->sel = id; w->armed = 0; w->b_rm = 0;
    if (id >= ID_STOCK) { ds_text_native(s, DS_S3, 36, SCREEN_W - 2 * DS_S3, DS_TEXT, ALIGN_CENTER, STOCK(id - ID_STOCK)->text); put(b, "App del reloj"); }
    else {
        mk_text(s, DS_S3, 36, SCREEN_W - 2 * DS_S3, DS_T_TITLE, DS_TEXT, ALIGN_CENTER, l_name(w, id)); p = put(b, inst ? "Instalada" : "Integrada");
#ifdef F3X_LOADER
        if (inst) { u32 t = (l_inst(w, id)->size * 10u + 512u) / 1024u; p = put(p, ", "); p = putn(p, t / 10u, 1); p = put(p, "."); p = putn(p, t % 10u, 1); put(p, " kB"); }
#endif
        (void)p;
    }
    mk_text(s, 0, 132, SCREEN_W, DS_T_CAPTION, DS_TEXT_2, ALIGN_CENTER, b);
    w->b_cancel = ds_pill(s, y, "Cancelar", DS_QUIET, 0, on_sheet, w); y -= DS_PILL_H + DS_S2;         /* bottom up: the way out is always in the same place */
    if (inst) { w->b_rm = ds_pill(s, y, "Quitar", DS_DANGER_QUIET, &w->l_rm, on_sheet, w); y -= DS_PILL_H + DS_S2; }
    w->b_pin = ds_pill(s, y, pinned ? "Soltar" : full ? "Favoritos llenos" : "Fijar arriba", !pinned && full ? DS_OFF : DS_PRIMARY, 0, on_sheet, w);
    motor_once(DS_HAPTIC_TAP, 1);
}

/* ---- the page ---- */
static void on_tile(lv_event_t *e) {
    Win *w = (Win *)lv_event_get_user_data(e); lv_obj_t *t = lv_event_get_target(e); u32 code = lv_event_get_code(e); int k;
    if ((code != EV_CLICKED && code != EV_LONG_PRESSED) || w->cur || w->sheet || w->busy) return;
    for (k = 0; k < w->ntile && w->tile[k].o != t; k++) { }
    if (k == w->ntile) return;
    if (code == EV_LONG_PRESSED) { l_sheet(w, w->tile[k].id); return; }       /* the release that follows finds the sheet open and is not a tap */
    l_launch(w, w->tile[k].id);
}
/* a tile for `id` at cell `n` of a grid of `cols` columns that starts at `y`: with its name in the two-column grid, compact in the three-column one */
static void l_tile(Win *w, u16 id, int n, i32 y, int cols) {
    i32 cw = cols == 2 ? DS_TILE_W : DS_COMPACT, ch = cols == 2 ? DS_TILE_H : DS_COMPACT;
    if (w->ntile >= L_TILES) return;
    lv_obj_t *r = ds_tile(w->list, DS_EDGE + (n % cols) * (cw + DS_S2), y + (n / cols) * (ch + DS_S2), l_icon(w, id), cols == 2 ? l_name(w, id) : 0);
    lv_obj_add_event_cb(r, on_tile, 0, w);
    w->tile[w->ntile].o = r; w->tile[w->ntile++].id = id;
}
#define GRID_H(n, cols, ch) ((((n) + (cols) - 1) / (cols)) * ((ch) + DS_S2))
static void build_list(Win *w) {
    lv_obj_t *l = w->list; i32 y = DS_HEADER_H, keep = w->ntile ? lv_obj_get_scroll_y(l) : 0; int n = 0, k;       /* a rebuild after a pin, an unpin or a removal keeps the place the user was at */
    lv_obj_clean(l); w->ntile = 0;
#ifdef F3X_LOADER
    for (k = 0; k < w->ninst; k++) app_free(w->inst[k]);
    w->ninst = 0;
    for (int slot = 0; slot < F3X_SLOTS; slot++) { Inst *in = inst_scan(slot); if (in) w->inst[w->ninst++] = in; }
#endif
    add_header(w);
    for (k = 0; k < L_PINS;) if (w->pin[k] && !l_valid(w, w->pin[k] - 1)) l_unpin(w, k); else k++;    /* a favourite that is gone (a removed app) leaves the row */
    for (k = 0; k < L_PINS && w->pin[k]; k++) l_tile(w, w->pin[k] - 1, n++, y, 3);
    if (n) y += GRID_H(n, 3, DS_COMPACT) + DS_S1;
    ds_section(l, y, "Mis apps"); y += DS_SECTION_H; n = 0;
    for (k = 0; k < (int)NAPPS && k < 12; k++) l_tile(w, (u16)k, n++, y, 2);
#ifdef F3X_LOADER
    for (k = 0; k < w->ninst; k++) l_tile(w, (u16)(ID_INST + w->inst[k]->slot), n++, y, 2);
#endif
    if (n) y += GRID_H(n, 2, DS_TILE_H) + DS_S1; else { ds_note(l, y + DS_S2, "Instala apps desde el PC"); y += 72; }
    ds_section(l, y, "Del reloj"); y += DS_SECTION_H; n = 0;
    for (k = 0; k < STOCK_ROWS; k++) if (l_valid(w, (u16)(ID_STOCK + k))) l_tile(w, (u16)(ID_STOCK + k), n++, y, 3);
    y += GRID_H(n, 3, DS_COMPACT) + DS_S3;
    ds_note(l, y, "Deja pulsada una app para fijarla arriba");
    if (keep) { lv_obj_update_layout(l); lv_obj_scroll_to_y(l, keep, 0); }       /* the layout first: the page must be tall enough to scroll to where it was */
    else lv_obj_scroll_to_y(l, 0, 0);
}
static int page_back(Win *w) { if (!w->sheet) return 0; l_close_sheet(w); return 1; }
static void page_free(Win *w) { if (w->tm) lv_timer_del(w->tm); }
