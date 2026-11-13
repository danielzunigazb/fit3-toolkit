/* SPDX-License-Identifier: Apache-2.0 */
/* apps/salud.c: the watch's own health data, read where the firmware keeps it (nothing is sent anywhere; docs/health.md), drawn as one calm page:
 *   the ring       the steps of today inside a ring of 48 dots that light up with the progress to the daily goal (the goal is the watch's own: KV record 0x424); the last lit dot is cream
 *   the heart      the last reading, how long ago it was taken, and the last 36 readings as thin bars (/nand/user/health/hr-point.rec: a 20-byte header, then records of 12 bytes: u32 unix
 *                  time, i16 zone minutes, u8 beats per minute; about one a minute while the watch is worn; a seek past the older ones, so the file may grow). Every 10 s.
 *   two pills      kilocalories and kilometres. The activity block is the firmware's RAM (0x3ce65d98): u32 steps +0x204, float kcal +0x210, float metres +0x218; read every 3 s.
 * Back leaves the app. ASCII texts, drawn with ds.h. */
#include "ds.h"
#include "health.h"
#include "icons.h"

#define HR_N 36
#define RING 48                                                  /* 48 dots of 6 px on a circle of radius 84 around (128, 146), from the top, clockwise */
#define RING_CX 128
#define RING_CY 146
#define CARD_Y 240                                               /* the heart card; its bars stand on its floor, 2 px wide with 1 px between */
#define CARD_H 100
#define BAR_X 140
#define BAR_FLOOR (CARD_Y + 86)
#define BAR_H 60
#define PILL_Y 346
static const signed char RING_PT[RING][2] = { {0, -84}, {11, -83}, {22, -81}, {32, -78}, {42, -73}, {51, -67}, {59, -59}, {67, -51}, {73, -42}, {78, -32}, {81, -22}, {83, -11}, {84, 0}, {83, 11}, {81, 22}, {78, 32}, {73, 42}, {67, 51}, {59, 59}, {51, 67}, {42, 73}, {32, 78}, {22, 81}, {11, 83}, {0, 84}, {-11, 83}, {-22, 81}, {-32, 78}, {-42, 73}, {-51, 67}, {-59, 59}, {-67, 51}, {-73, 42}, {-78, 32}, {-81, 22}, {-83, 11}, {-84, 0}, {-83, -11}, {-81, -22}, {-78, -32}, {-73, -42}, {-67, -51}, {-59, -59}, {-51, -67}, {-42, -73}, {-32, -78}, {-22, -81}, {-11, -83} };
typedef struct {
    lv_obj_t *root, *dot[RING], *bar[HR_N], *l_steps, *l_goal, *l_kcal, *l_km, *l_hr, *l_age; lv_timer_t *tm, *tm_act;
    u8 hr[HR_N], n, lit; u32 last_ts, goal; i32 last_tz;
    char t_steps[TXT_MAX], t_goal[TXT_MAX], t_kcal[TXT_MAX], t_km[TXT_MAX], t_hr[TXT_MAX], t_age[TXT_MAX];
} Health;

static int hr_load(Health *s) {
    u8 buf[HR_N * HR_REC]; u32 sz, n, k, got = 0; i32 fd = f_open(HR_PATH, "r");
    s->n = 0;
    if (fd < 0) return 0;
    sz = f_size(fd);
    if (sz < HR_HDR + HR_REC) { f_close(fd); return 0; }
    n = (sz - HR_HDR) / HR_REC; k = n < HR_N ? n : HR_N;
    f_seek(fd, (long long)(HR_HDR + HR_REC * (n - k)), 0);
    while (got < k * HR_REC) { i32 r = f_read(fd, buf + got, k * HR_REC - got); if (r <= 0) break; got += (u32)r; }
    f_close(fd);
    for (u32 i = 0; i < got / HR_REC; i++) {
        const u8 *r = buf + i * HR_REC; u32 ts = (u32)r[0] | (u32)r[1] << 8 | (u32)r[2] << 16 | (u32)r[3] << 24; u8 hr = r[6];
        if (ts < 1700000000u || hr < 30 || hr > 250) continue;
        s->hr[s->n++] = hr; s->last_ts = ts; s->last_tz = (i32)(short)((u16)r[4] | (u16)r[5] << 8);
    }
    return s->n;
}
static char *put_age(char *b, u32 sec) {
    if (sec < 90) return put(b, "ahora");
    b = put(b, "hace ");
    if (sec < 5400) { b = putn(b, (sec + 30) / 60, 1); return put(b, " min"); }
    if (sec < 172800) { b = putn(b, (sec + 1800) / 3600, 1); return put(b, " h"); }
    b = putn(b, sec / 86400, 1); return put(b, " d");
}
/* the daily goal of the watch (KV record 0x424: a u16 at its start, 1000 to 49000; the firmware itself falls back to 6000) */
static u32 read_goal(void) {
    u8 kv[0x24]; u32 g; fill8(kv, 0, sizeof kv); kv_read(0x424, kv, sizeof kv, 0);
    g = (u32)kv[0] | (u32)kv[1] << 8;
    return g >= 1000u && g <= 49000u ? g : 6000u;
}
static void act_render(Health *s) {
    char b[TXT_MAX], *p; u32 steps = ACT(0x204), km10 = (f2u(ACT(0x218)) + 50u) / 100u, lit;
    if (steps >= 1000000u) steps = 0;
    putn(b, steps, 1); set_txt(s->l_steps, s->t_steps, b);
    p = putn(b, f2u(ACT(0x210)), 1); put(p, " kcal"); set_txt(s->l_kcal, s->t_kcal, b);
    p = putn(b, km10 / 10u, 1); p = put(p, "."); p = putn(p, km10 % 10u, 1); put(p, " km"); set_txt(s->l_km, s->t_km, b);
    lit = steps * RING / s->goal; if (lit > RING) lit = RING;
    {
        for (u32 i = 0; i < RING; i++) set_bg_color(s->dot[i], i + 1 == lit ? DS_TEXT : i < lit ? DS_ACCENT : DS_SURFACE_P);
        s->lit = (u8)lit;
    }
}
static void health_render(Health *s) {
    char b[TXT_MAX];
    if (!s->n) { set_txt(s->l_hr, s->t_hr, "--"); set_txt(s->l_age, s->t_age, "sin medidas"); }
    else {
        u32 now = local_now(), then = s->last_ts + (u32)(s->last_tz * 60), age = now > then ? now - then : 0;
        putn(b, s->hr[s->n - 1], 1); set_txt(s->l_hr, s->t_hr, b);
        put_age(b, age); set_txt(s->l_age, s->t_age, b);
    }
    for (int i = 0; i < HR_N; i++) {                             /* the newest on the right; the oldest slots stay empty until there are 36 */
        int j = i - (HR_N - s->n); lv_obj_t *o = s->bar[i];
        if (j < 0) { lv_obj_add_flag(o, FL_HIDDEN); continue; }
        i32 v = s->hr[j], h = (v - 40) * BAR_H / 100; if (h < 3) h = 3; if (h > BAR_H) h = BAR_H;
        lv_obj_clear_flag(o, FL_HIDDEN); lv_obj_set_size(o, 2, h); lv_obj_set_pos(o, BAR_X + i * 3, BAR_FLOOR - h);
        set_bg_color(o, i == HR_N - 1 ? DS_TEXT : v > 110 ? DS_DANGER : DS_ACCENT);
    }
}
static void health_timer(lv_timer_t *t) { Health *s = (Health *)timer_user_data(t); hr_load(s); health_render(s); }
static void act_timer(lv_timer_t *t) { act_render((Health *)timer_user_data(t)); }
static lv_obj_t *pill(lv_obj_t *root, i32 x) {
    lv_obj_t *b = mk_box(root, x, PILL_Y, 123, 50, DS_R_COMPACT, DS_SURFACE);
    return mk_text(b, 0, 12, 123, DS_T_LABEL, DS_TEXT, ALIGN_CENTER, "");
}
static void *health_create(lv_obj_t *root) {
    Health *s = (Health *)app_malloc(sizeof(Health)); if (!s) return 0;
    fill8(s, 0, sizeof(Health)); s->root = root; s->goal = read_goal();
    ds_title(root, "Salud");
    for (int i = 0; i < RING; i++) s->dot[i] = mk_box(root, RING_CX + RING_PT[i][0] - 3, RING_CY + RING_PT[i][1] - 3, 6, 6, 3, DS_SURFACE_P);
    s->l_steps = mk_text(root, 0, RING_CY - 48, SCREEN_W, DS_T_HERO, DS_TEXT, ALIGN_CENTER, "");
    mk_text(root, 0, RING_CY + 6, SCREEN_W, DS_T_LABEL, DS_TEXT_2, ALIGN_CENTER, "pasos");
    { char g[TXT_MAX], *p = put(g, "meta "); putn(p, s->goal, 1); s->l_goal = mk_text(root, 0, RING_CY + 34, SCREEN_W, DS_T_CAPTION, DS_TEXT_2, ALIGN_CENTER, g); }
    mk_box(root, DS_EDGE, CARD_Y, SCREEN_W - 2 * DS_EDGE, CARD_H, DS_R_TILE, DS_SURFACE);
    s->l_hr = mk_text(root, 18, CARD_Y + 4, 112, DS_T_HERO, DS_TEXT, ALIGN_LEFT, "--");
    mk_text(root, 18, CARD_Y + 60, 112, DS_T_CAPTION, DS_TEXT_2, ALIGN_LEFT, "lpm");
    s->l_age = mk_text(root, 18, CARD_Y + 77, 118, DS_T_CAPTION, DS_TEXT_2, ALIGN_LEFT, "");
    for (int i = 0; i < HR_N; i++) { s->bar[i] = mk_box(root, BAR_X + i * 3, BAR_FLOOR - 3, 2, 3, 1, DS_ACCENT); lv_obj_add_flag(s->bar[i], FL_HIDDEN); }
    s->l_kcal = pill(root, DS_EDGE); s->l_km = pill(root, DS_EDGE + 123 + 4);
    act_render(s); hr_load(s); health_render(s);
    s->tm = lv_timer_create(health_timer, 10000, s); s->tm_act = lv_timer_create(act_timer, 3000, s);
    return s;
}
static int health_back(void *st) { (void)st; return 0; }
static void health_destroy(void *st) { Health *s = (Health *)st; if (s->tm) lv_timer_del(s->tm); if (s->tm_act) lv_timer_del(s->tm_act); app_free(s); }
APP_EXPORT(health, "Salud", &ICON_COUNTER, health_create, health_back, health_destroy)
