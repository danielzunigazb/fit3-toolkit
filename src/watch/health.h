/* SPDX-License-Identifier: Apache-2.0 */
/* health.h: what the watch itself knows about its wearer, read where the firmware keeps it (docs/health.md). Shared by the Salud app and the launcher's header. */
#ifndef F3_HEALTH_H
#define F3_HEALTH_H
#include "ui.h"
#define HR_PATH "/nand/user/health/hr-point.rec"
#define HR_HDR 20
#define HR_REC 12
#define ACT(off) (*(volatile u32 *)(0x3ce65d98u + (off)))      /* the firmware's activity block: u32 steps at +0x204, float kcal +0x210, float metres +0x218 */

/* the integer part of an IEEE float given as its bits (no libgcc here): 0 for a negative, a tiny or an absurd one */
H u32 f2u(u32 b) {
    u32 e = (b >> 23) & 255u, m = (b & 0x7fffffu) | 0x800000u;
    if ((b >> 31) || e < 127u || e > 150u + 7u) return 0;
    return e >= 150u ? m << (e - 150u) : m >> (150u - e);
}
/* days since 1970-01-01 of a calendar date (Howard Hinnant's algorithm; years after 1970) */
H u32 days_civil(i32 y, i32 m, i32 d) {
    y -= m <= 2;
    i32 era = y / 400; u32 yoe = (u32)(y - era * 400);
    u32 doy = (153u * (u32)(m + (m > 2 ? -3 : 9)) + 2u) / 5u + (u32)d - 1u, doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return (u32)(era * 146097 + (i32)doe - 719468);
}
/* the clock of the watch as seconds since 1970 on the wall (local time): the records are unix time plus a zone in minutes */
H u32 local_now(void) {
    i32 t[9]; fill8(t, 0, sizeof t); rtc_get_time(t);
    return days_civil(t[6], t[5], t[4]) * 86400u + (u32)t[3] % 24u * 3600u + (u32)t[2] % 60u * 60u + (u32)t[1] % 60u;
}
/* the newest heart-rate reading: beats per minute and, in *age, how many seconds ago it was taken (against the clock of the watch); 0 when there is none */
H u32 hr_tail(u32 *age) {
    u8 r[HR_REC]; u32 sz, n, got = 0; i32 fd = f_open(HR_PATH, "r");
    if (fd < 0) return 0;
    sz = f_size(fd);
    if (sz < HR_HDR + HR_REC) { f_close(fd); return 0; }
    n = (sz - HR_HDR) / HR_REC;
    f_seek(fd, (long long)(HR_HDR + HR_REC * (n - 1)), 0);
    while (got < HR_REC) { i32 q = f_read(fd, r + got, HR_REC - got); if (q <= 0) break; got += (u32)q; }
    f_close(fd);
    if (got < HR_REC) return 0;
    u32 ts = (u32)r[0] | (u32)r[1] << 8 | (u32)r[2] << 16 | (u32)r[3] << 24, hr = r[6]; i32 tz = (i32)(short)((u16)r[4] | (u16)r[5] << 8);
    if (ts < 1700000000u || hr < 30 || hr > 250) return 0;
    u32 now = local_now(), then = ts + (u32)(tz * 60);
    if (age) *age = now > then ? now - then : 0;
    return hr;
}
#endif
