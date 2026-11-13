/* SPDX-License-Identifier: Apache-2.0 */
/* apps/hola.c: a greeting that changes when you tap it. The smallest app that uses a label that changes, a tap and the palette.
 *
 * Tap the card: the next greeting. The counter under it says how many times you have tapped. Nothing is saved. */
#include "ui.h"
#include "icons.h"

static const char *const GREET[] = { "Hola", "Hello", "Bonjour", "Ciao", "Hallo" };
#define N_GREET (sizeof GREET / sizeof GREET[0])

typedef struct {
    i32 i, taps;
    lv_obj_t *card, *greet, *count;
    char t_greet[TXT_MAX], t_count[TXT_MAX];
} Hola;

static void hola_render(Hola *s) {
    char b[TXT_MAX]; putn(b, (u32)s->taps, 1);
    set_txt(s->greet, s->t_greet, GREET[s->i]);
    set_txt(s->count, s->t_count, b);
}
static void hola_click(lv_event_t *e) {
    Hola *s = (Hola *)lv_event_get_user_data(e);
    if (lv_event_get_code(e) != EV_CLICKED) return;
    s->i = (s->i + 1) % N_GREET; s->taps++;
    motor_once(1, 1);
    hola_render(s);
}
static void *hola_create(lv_obj_t *root) {
    Hola *s = (Hola *)app_malloc(sizeof(Hola)); if (!s) return 0;
    fill8(s, 0, sizeof(Hola));
    mk_text(root, 0, 8, 256, FT_SMALL, C_MUTED, ALIGN_CENTER, "Hola");
    s->card = mk_box(root, 1, 42, 254, 290, 26, C_CARD);
    lv_obj_add_event_cb(s->card, hola_click, 0, s);
    s->greet = mk_text(s->card, 0, 90, 254, FT_NAME, C_WHITE, ALIGN_CENTER, "");
    s->count = mk_text(s->card, 0, 200, 254, FT_SMALL, C_MUTED, ALIGN_CENTER, "");
    mk_text(s->card, 0, 240, 254, FT_SMALL, C_MUTED, ALIGN_CENTER, "Tap to change");
    hola_render(s);
    return s;
}
static int hola_back(void *st) { (void)st; return 0; }
static void hola_destroy(void *st) { app_free(st); }
APP_EXPORT(hola, "Hola", &ICON_COUNTER, hola_create, hola_back, hola_destroy)
