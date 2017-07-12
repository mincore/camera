/* ===================================================
 * Copyright (C) 2015 chenshuangping All Right Reserved.
 *      Author: mincore@163.com
 *    Filename: font.c
 *     Created: 2015-07-13 07:30
 * Description:
 * ===================================================
 */
#include "internal.h"
#include "font.h"

#define ASC16_FONT_PATH "/var/font/ASC16"
#define ASC16_OFFSET(C) ((((C)&0xFF)) * 16)

#define ASC48_FONT_PATH "/var/font/ASC48"
#define ASC48_OFFSET(C) ((((C)&0xFF) - 0x20) * 144)

#define HZ16_FONT_PATH "/var/font/HZ16"
#define HZ16_OFFSET(C1, C2) (((((C1)&0xFF)-0xA0-1)*94 + (((C2)&0xFF)-0xA0-1)) * 32)

static unsigned char *load_font(const char *file)
{
    FILE *fp;
    size_t fsize;
    unsigned char *font;

    if ((fp = fopen(file, "r")) == NULL) {
        VIDEO_ERROR("open %s failed\n", file);
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    font = malloc(fsize);
    if (fread(font, fsize, 1, fp) != 1) {
        free(font);
        font = NULL;
    }
    fclose(fp);

    return font;
}

const unsigned char *font_get_asc16_bitmap(unsigned char ch)
{
    static unsigned char *base = NULL;
    if (base == NULL)
        base = load_font(ASC16_FONT_PATH);
    if (base == NULL)
        return NULL;
    return base + ASC16_OFFSET(ch);
}

const unsigned char *font_get_asc48_bitmap(unsigned char ch)
{
    static unsigned char *base = NULL;
    if (base == NULL)
        base = load_font(ASC48_FONT_PATH);
    if (base == NULL)
        return NULL;
    return base + ASC48_OFFSET(ch);
}

const unsigned char *font_get_hzk16_bitmap(unsigned char ch0, unsigned char ch1)
{
    static unsigned char *base = NULL;
    if (base == NULL)
        base = load_font(HZ16_FONT_PATH);
    if (base == NULL)
        return NULL;

    return base + HZ16_OFFSET(ch0, ch1);
}
