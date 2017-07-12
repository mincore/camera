/* ===================================================
 * Copyright (C) 2015 chenshuangping All Right Reserved.
 *      Author: mincore@163.com
 *    Filename: utils.c
 *     Created: 2015-07-13 07:31
 * Description:
 * ===================================================
 */
#include "internal.h"
#include "utils.h"

#define SLICE_TYPE_P            0
#define SLICE_TYPE_B            1
#define SLICE_TYPE_I            2

int get_resolution(VideoEncResolution res, int *width, int *height)
{
    static struct {
        int width;
        int height;
    } res_table[] = {
        {352, 288},
        {352, 240},
        {640, 480},
        {704, 576},
        {704, 480},
        {1280, 720},
        {1280, 960},
        {1920, 1080},
    };

    *width = res_table[res].width;
    *height = res_table[res].height;

    return 0;
}

const char *stream_to_string(VideoEncStream stream)
{
    const char *ret;

    switch (stream) {
    case videoEncStreamMain: ret = "videoEncStreamMain"; break;
    case videoEncStreamExtra1: ret = "videoEncStreamExtra1"; break;
    case videoEncStreamExtra2: ret = "videoEncStreamExtra2"; break;
    case videoEncStreamExtra3: ret = "videoEncStreamExtra3"; break;
    default: ret = "videoEncStreamNumber"; break;
    }

    return ret;
}

int slice_type_to_frame_type(int slice_type)
{
    int ret = videoEncFrameP;
    switch (slice_type) {
    case SLICE_TYPE_I: ret = videoEncFrameI; break;
    }

    return ret;
}

unsigned long long now()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((unsigned long long)tv.tv_sec)*1000 + tv.tv_usec/1000;
}

void surface_nv12_copy_in(unsigned char *surface_mmap, unsigned char *src, int w, int h)
{
    unsigned char *y_src;
    unsigned char *u_src;
    unsigned char *y_dst;
    unsigned char *u_dst;
    int y_size;
    int row;

    if (ALIGN_UP(w, 128) == w) {
        y_size = w*h;
        y_src = src;
        u_src = y_src + y_size;

        y_dst = surface_mmap;
        u_dst = y_dst + w * ALIGN_UP(h, 32);;

        memcpy(y_dst, y_src, y_size);
        memcpy(u_dst, u_src, w*(h/2));

        return;
    }

    y_src = src;
    u_src = src + w * h;

    y_dst = surface_mmap;
    u_dst = surface_mmap + ALIGN_UP(w, 128) * ALIGN_UP(h, 32);
    for (row = 0; row < h; row++) {
        memcpy(y_dst, y_src, w);

        y_src += w;
        y_dst += ALIGN_UP(w, 128);
    }

    for (row = 0; row < (h / 2); row++) {
        memcpy(u_dst, u_src, w);

        u_src += w;
        u_dst += ALIGN_UP(w, 128);
    }


}

void surface_nv12_copy_out(unsigned char *surface_mmap, unsigned char *dst, int w, int h)
{
    unsigned char *y_src;
    unsigned char *u_src;
    unsigned char *y_dst;
    unsigned char *u_dst;
    int y_size;
    int row;

    if (ALIGN_UP(w, 128) == w)
    {
        y_size = w*ALIGN_UP(h, 32);
        y_src = surface_mmap;
        u_src = y_src + y_size;

        y_dst = dst;
        u_dst = y_dst + w * h;

        memcpy(y_dst, y_src, y_size);
        memcpy(u_dst, u_src, w*(h/2));

        return;
    }

    y_src = surface_mmap;
    u_src = surface_mmap + ALIGN_UP(w, 128) * ALIGN_UP(h, 32);

    y_dst = dst;
    u_dst = dst + w * h;
    for (row = 0; row < h; row++) {
        memcpy(y_dst, y_src, w);

        y_src += ALIGN_UP(w, 128);
        y_dst += w;
    }

    for (row = 0; row < (h / 2); row++) {
        memcpy(u_dst, u_src, w);

        u_src += ALIGN_UP(w, 128);
        u_dst += w;
    }

}

void gpu_encode_input_param_init(GPU_ENC_INPUT_PARAM *input_param, int fmt, int width, int height, void *raw_data)
{
    memset(input_param, 0, sizeof(GPU_ENC_INPUT_PARAM));

    input_param->type = GPU_ENC_INPUT_TYPE_RAW;
    input_param->value.raw.fmt = fmt;
    input_param->value.raw.y_buf = raw_data;
    input_param->value.raw.y_pitch = width;
    if (fmt == GPU_PIC_FMT_YV12) {
        input_param->value.raw.u_buf = raw_data + width * height;
        input_param->value.raw.v_buf = raw_data + width * height + ((width * height) >> 2);
        input_param->value.raw.u_pitch = width / 2;
        input_param->value.raw.v_pitch = width / 2;
    } else if (fmt == GPU_PIC_FMT_NV12) {
        input_param->value.raw.u_buf = raw_data + width * height;
        input_param->value.raw.v_buf = input_param->value.raw.u_buf + 1;
        input_param->value.raw.u_pitch = width;
        input_param->value.raw.v_pitch = width;
    }
    input_param->value.raw.width = width;
    input_param->value.raw.height = height;
}


