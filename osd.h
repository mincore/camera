/* ===================================================
 * Copyright (C) 2015 chenshuangping All Right Reserved.
 *      Author: mincore@163.com
 *    Filename: osd.h
 *     Created: 2015-07-13 07:31
 * Description:
 * ===================================================
 */
#ifndef OSD_H
#define OSD_H

struct rect {
    int x;
    int y;
    int w;
    int h;
};

struct surface {
    unsigned char *base;
    int width;
    int height;
    int bpp;
};

/* text: please use Utf8ToGB2312 to convert
 *  example:
 *          char *str = "测试123";
 *          char dst[256] = {0};
 *          Utf8ToGB2312(str, strlen(str), dst, 256);
 *          surface_draw_text(&surface, dst, strlen(dst), &rect);
 *
 * If you are sure that the str is an assic string, then you can directly call surface_draw_text()
 */
int surface_draw_text(struct surface *s, struct rect *rc, unsigned char *text, int len);

int surface_draw_rect(struct surface *s, struct rect *rc);

int osd_init();
void osd_draw_timestamp16(void *yuv_data, int yuv_w, int yuv_h, int x, int y);
void osd_draw_timestamp48(void *yuv_data, int yuv_w, int yuv_h, int x, int y);


#endif
