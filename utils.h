/* ===================================================
 * Copyright (C) 2015 chenshuangping All Right Reserved.
 *      Author: mincore@163.com
 *    Filename: utils.h
 *     Created: 2015-07-13 07:31
 * Description:
 * ===================================================
 */
#ifndef UTILS_H
#define UTILS_H

#include <api/VideoEnc.h>

int get_resolution(VideoEncResolution res, int *width, int *height);
const char *stream_to_string(VideoEncStream stream);
int slice_type_to_frame_type(int slice_type);

unsigned long long now();

void surface_nv12_copy_in(unsigned char *surface_mmap, unsigned char *src, int w, int h);
void surface_nv12_copy_out(unsigned char *surface_mmap, unsigned char *dst, int w, int h);
void gpu_encode_input_param_init(GPU_ENC_INPUT_PARAM *input_param, int fmt, int width, int height, void *raw_data);


#endif
