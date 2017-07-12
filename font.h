/* ===================================================
 * Copyright (C) 2015 chenshuangping All Right Reserved.
 *      Author: mincore@163.com
 *    Filename: font.h
 *     Created: 2015-07-13 07:30
 * Description:
 * ===================================================
 */
#ifndef ASSIC_H
#define ASSIC_H

/* GB2312
 * The value of the first byte is from 0xA1-0xF7 (161-247),
 * while the value of the second byte is from 0xA1-0xFE (161-254).
 */
#define FONT_IS_GB2312(a, b) ((a) >= 0xA1 && (a) <= 0xF7 && (b) >= 0xA1 && (b) <= 0xFE)
#define FONT_IS_ASSIC(a) ((a) >= 0x21 && (a) <= 0x7E)

const unsigned char *font_get_asc16_bitmap(unsigned char ch);
const unsigned char *font_get_asc48_bitmap(unsigned char ch);
const unsigned char *font_get_hzk16_bitmap(unsigned char ch0, unsigned char ch1);

#endif
