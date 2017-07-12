/* ===================================================
 * Copyright (C) 2015 chenshuangping All Right Reserved.
 *      Author: mincore@163.com
 *    Filename: jpeg.h
 *     Created: 2015-07-13 07:31
 * Description:
 * ===================================================
 */
#ifndef _VIDEO_JPEG_H
#define _VIDEO_JPEG_H

#include "jpeglib_interface.h"

struct yuv_frame;

int jpeg_encode(struct yuv_frame *yuv_frame, int quality, JpegCropRect *rect, JpegOutputBuffer *out);
int jpeg_encode_done(JpegOutputBuffer *buf);

#endif
