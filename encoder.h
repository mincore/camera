/* ===================================================
 * Copyright (C) 2015 chenshuangping All Right Reserved.
 *      Author: mincore@163.com
 *    Filename: encoder.h
 *     Created: 2015-07-13 07:30
 * Description:
 * ===================================================
 */
#ifndef _VIDEO_ENCODER_H
#define _VIDEO_ENCODER_H

#include <api/VideoEnc.h>
#include "yuv_wq.h"

struct encoder {
    struct yuv_wq wq;
    Queue h264_queue;
    void *encode_ctx;
    void *coded_data;
    int force_I_frame;
#ifdef HAVE_H264_SEI_FACE_INFO
    uint64 first_packet_time;
#endif
};

typedef struct {
    VideoEncDesc desc;
    VideoEncWaterMark mark;
    VideoEncROI roi;
    VideoEncFormat format;
    VideoEncCover *cover;
    int cover_count;
    struct encoder encoder;
} VideoEncoderPriv;

int encoder_init(struct encoder *encoder);
int encoder_deinit(struct encoder *encoder);
int encoder_start(struct encoder *encoder);
int encoder_stop(struct encoder *encoder);
int encoder_get_packet(struct encoder *encoder, VideoEncPacket *packet);
int encoder_release_packet(struct encoder *encoder, VideoEncPacket *packet);

#endif
