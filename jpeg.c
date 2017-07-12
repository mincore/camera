/* ===================================================
 * Copyright (C) 2015 chenshuangping All Right Reserved.
 *      Author: mincore@163.com
 *    Filename: jpeg.c
 *     Created: 2015-07-13 07:31
 * Description:
 * ===================================================
 */
#include <stdio.h>
#include <sys/ioctl.h>

#include "xf86drm.h"
#include "intel_bufmgr.h"
#include "jpeglib_interface.h"

#include "internal.h"

int jpeg_encode(struct yuv_frame *yuv_frame, int quality, JpegCropRect *rect, JpegOutputBuffer *out)
{
    JpegInputBuffer src;

    assert(yuv_frame->handle);

    const xcam_buf_info *info = xcam_buf_get_info(yuv_frame->handle);

    if (rect->x + rect->w > info->width) {
        VIDEO_ERROR("out of bound x:%d y:%d w:%d h:%d", rect->x, rect->y, rect->w, rect->h);
        return -1;
    }

    if (rect->y + rect->h > info->height) {
        VIDEO_ERROR("out of bound x:%d y:%d w:%d h:%d", rect->x, rect->y, rect->w, rect->h);
        return -1;
    }

    src.handle = xcam_buf_get_bo(yuv_frame->handle);
    src.w = info->width;
    src.h = info->height;
    src.ystride = info->width;
    src.uvstride = info->width;
    src.yoffset = 0;
    src.uvoffset = info->width*info->height;

    static int inited = 0;
    if (atomic_read(&inited) == 0 ) {
        atomic_set(&inited, 1);
        if (jpeg_init(USE_OPENCL) < 0) {
            VIDEO_ERROR("jpeg_init failed");
            return -1;
        }
    }

    if (jpeg_encode_crop_buffer(&src, quality, NULL, 0, out, rect) < 0) {
        VIDEO_ERROR("jpeg_encode_crop_buffer failed");
        return -1;
    }

    return 0;
}

int jpeg_encode_done(JpegOutputBuffer *buf)
{
    return jpeg_destroy_buffer();
}

