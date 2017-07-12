/* ===================================================
 * Copyright (C) 2015 chenshuangping All Right Reserved.
 *      Author: mincore@163.com
 *    Filename: h264.c
 *     Created: 2015-07-13 07:30
 * Description:
 * ===================================================
 */
#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "h264.h"

static unsigned char sei_start_key[] = {0x00, 0x00, 0x00, 0x01, 0x06};
static unsigned char sei_end_key[] = {0x80, 0x00, 0x00, 0x00, 0x01};
static unsigned char sei_user_data_uuid[16] = {0x3A, 0xF2, 0x44, 0x65, 0xD0, 0xCA, 0x12, 0x33, 0x2E, 0x92, 0x01, 0x87, 0x23, 0x1B, 0x00, 0xFE};

static int sei_user_data_len(int user_data_len)
{
    int len = 0;
    len += 1;
    len += user_data_len/0xff;
    len += (user_data_len%0xff) ? 1 : 0;
    len += 16;
    len += user_data_len;
    return len;
}

#if 0
static unsigned char* sei_user_data_fill_head(unsigned char *sei)
{
    int i = 0;

    memcpy(&sei[i], sei_start_key, 5);
    i += 5;

    return &sei[i];
}

static unsigned char* sei_user_data_fill_tail(unsigned char *sei)
{
    int i = 0;

    sei[i++] = 0x80;

    return &sei[i];
}
#endif

static unsigned char* sei_user_data_fill_body(unsigned char *sei, void *user_data, int user_data_len)
{
    int len = user_data_len+16;
    int i = 0;
    int numff = len / 0xFF + 1;
    int size = len % 0xFF;

    sei[i++] = 0x05;
    for (; i < numff; i++)
        sei[i] = 0xFF;

    sei[i++] = size;

    memcpy(&sei[i], sei_user_data_uuid, 16);
    i += 16;

    memcpy(&sei[i], user_data, user_data_len);
    i += user_data_len;

    return &sei[i];
}

void *h264_clone_and_insert_user_data(
            void *h264_data, int h264_data_len,
            void *user_data, int user_data_len,
            int *new_h264_data_len)
{
    unsigned char *sei_start;
    unsigned char *sei_end;
    unsigned char *ret;

    assert(h264_data && h264_data_len > 0);

    if (user_data == NULL || user_data_len <= 0) {
        goto clone;
    }

    sei_start = (unsigned char *)memmem(h264_data, h264_data_len, sei_start_key, 5);
    if (!sei_start) {
        goto clone;
    }

    sei_end = (unsigned char *)memmem(sei_start+5, h264_data_len - (sei_start+5 - (unsigned char *)h264_data), sei_end_key, 5);
    if (!sei_end) {
        goto clone;
    }

    int head_len = sei_end - (unsigned char *)h264_data;
    int tail_len = h264_data_len - head_len;
    int insert_len = sei_user_data_len(user_data_len);

    *new_h264_data_len = h264_data_len + insert_len;
    ret = (unsigned char *)malloc(*new_h264_data_len);
    memcpy(ret, h264_data, head_len);
    unsigned char *p = sei_user_data_fill_body(ret + head_len, user_data, user_data_len);
    memcpy(p, sei_end, tail_len);

    return ret;

clone:
    ret = malloc(h264_data_len);
    memcpy(ret, h264_data, h264_data_len);
    *new_h264_data_len = h264_data_len;
    return ret;
}
