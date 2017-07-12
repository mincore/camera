/* ===================================================
 * Copyright (C) 2015 chenshuangping All Right Reserved.
 *      Author: mincore@163.com
 *    Filename: osd.c
 *     Created: 2015-07-13 07:31
 * Description:
 * ===================================================
 */
#include "internal.h"
#include "osd.h"
#include "font.h"

static int surface_draw_bytes(struct surface *s, struct rect *rc, const unsigned char *ptext, int count, int text_w, int text_h)
{
    int i, j, k;
    unsigned char tmp;
    unsigned char *buf;

    buf = s->base + rc->y * s->width + rc->x;

    for (i=0; i<text_h; i++) {
        for (j=0; j<text_w/8; j++) {
            tmp = *ptext++;
            if (tmp == 0)
                continue;
            for (k=0; k<8; k++) {
                if (tmp & (1<<(7-k)) ) {
                    int index = i*s->width + j*8 + k;
                    buf[index] = 0xFF;
                }
            }
        }
    }
    return 0;
}

#if 0
static int surface_draw_char(struct surface *s, struct rect *rc, unsigned char ch)
{
    return surface_draw_bytes(s, rc, font_get_asc48_bitmap(ch), 1, 24, 48);
}

static int surface_draw_word(struct surface *s, struct rect *rc, unsigned char *word)
{
    return surface_draw_bytes(s, rc, font_get_hzk16_bitmap(word[0], word[1]), 2, 16, 16);
}
#endif

/* text: please use Utf8ToGB2312 to convert
 *  example:
 *          char *str = "测试123";
 *          char dst[256] = {0};
 *          Utf8ToGB2312(str, strlen(str), dst, 256);
 *          surface_draw_text(&surface, dst, strlen(dst), &rect);
 *
 * If you are sure that the str is an assic string, then you can directly call surface_draw_text()
 */
int surface_draw_text16(struct surface *s, struct rect *rc, unsigned char *text, int len)
{
    struct rect rcTmp;
    int i;

    memcpy(&rcTmp, rc, sizeof(rcTmp));

    for (i=0; i<len;) {

        if (FONT_IS_ASSIC(text[i])) {
            surface_draw_bytes(s, &rcTmp, font_get_asc16_bitmap(text[i]), 1, 8, 16);
            rcTmp.x += 8;
            i++;
            continue;
        }

        if (i != len-1 && FONT_IS_GB2312(text[i], text[i+1])) {
            surface_draw_bytes(s, rc, font_get_hzk16_bitmap(text[i], text[i+1]), 2, 16, 16);
            rcTmp.x += 16;
            i+=2;
            continue;
        }

        rcTmp.x += 8;
        i++;
    }

    return 0;
}

int surface_draw_text48(struct surface *s, struct rect *rc, unsigned char *text, int len)
{
    struct rect rcTmp;
    int i;

    memcpy(&rcTmp, rc, sizeof(rcTmp));

    for (i=0; i<len;) {

        if (FONT_IS_ASSIC(text[i])) {
            surface_draw_bytes(s, &rcTmp, font_get_asc48_bitmap(text[i]), 1, 24, 48);
            rcTmp.x += 24;
            i++;
            continue;
        }

        if (i != len-1 && FONT_IS_GB2312(text[i], text[i+1])) {
           surface_draw_bytes(s, rc, font_get_hzk16_bitmap(text[i], text[i+1]), 2, 16, 16);
            rcTmp.x += 16;
            i+=2;
            continue;
        }

        rcTmp.x += 24;
        i++;
    }

    return 0;
}


int surface_draw_rect(struct surface *s, struct rect *rc)
{
    unsigned char *start = s->base;
    int pitch = s->width * s->bpp;
    unsigned char color = 0xFF;
    int x = rc->x;
    int y = rc->y;
    int w = rc->w;
    int h = rc->h;
    unsigned char *dst;
    int i;

    dst = start + pitch * y + x;
    memset(dst, color, w);

    dst = start + pitch * (y+1) + x;
    memset(dst, color, w);

    for (i=y+2; i<y+h-2; i++) {
        dst = start + pitch * i + x;
        dst[0] = color;
        dst[1] = color;
        dst[w-2] = color;
        dst[w-1] = color;
    }

    dst = start + pitch * (y+h-2) + x;
    memset(dst, color, w);

    dst = start + pitch * (y+h-1) + x;
    memset(dst, color, w);

    return 0;
}

static int build_timestamp(char *str, int len)
{
    time_t t;
    struct tm tm;

    t = time(NULL);
    localtime_r(&t, &tm);

    return snprintf(str, len, "%04d/%02d/%02d %02d:%02d:%02d\n",
           tm.tm_year+1900, tm.tm_mon+1,
           tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
}

int osd_init()
{
    return 0;
}

void osd_draw_timestamp16(void *yuv_data, int yuv_w, int yuv_h, int x, int y)
{
    char time_str[256] = {0};
    int len = build_timestamp(time_str, 256);
    struct surface s = {
        .base = (unsigned char *)yuv_data,
        .width = yuv_w,
        .height = yuv_h,
        .bpp = 1,
    };
    struct rect rc = {
        .x = x,
        .y = y,
    };

    surface_draw_text16(&s, &rc, (unsigned char *)time_str, len);
}

void osd_draw_timestamp48(void *yuv_data, int yuv_w, int yuv_h, int x, int y)
{
    char time_str[256] = {0};
    int len = build_timestamp(time_str, 256);
    struct surface s = {
        .base = (unsigned char *)yuv_data,
        .width = yuv_w,
        .height = yuv_h,
        .bpp = 1,
    };
    struct rect rc = {
        .x = x,
        .y = y,
    };

    surface_draw_text48(&s, &rc, (unsigned char *)time_str, len);
}


