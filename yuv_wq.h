/* ===================================================
 * Copyright (C) 2015 chenshuangping All Right Reserved.
 *      Author: mincore@163.com
 *    Filename: yuv_wq.h
 *     Created: 2015-07-13 07:32
 * Description:
 * ===================================================
 */
#ifndef _VIDEO_YUV_WQ_H
#define _VIDEO_YUV_WQ_H

#include <pthread.h>

#include <api/VideoEnc.h>
#include "GPU_API.h"
#include "queue.h"

struct yuv_frame;
struct yuv_wq;

typedef int (*process_yuv_fn)(struct yuv_wq *wq, struct yuv_frame *frame);

struct yuv_wq {
    int yuv_w;
    int yuv_h;
    process_yuv_fn process_yuv;
    Queue src_yuv_queue;
    pthread_t thread;

    int quit;
    int stopping;
    int need_scale;
    int need_xcam_buf;
    VideoEncStream stream_id;

    GPU_SURFACE_ID dst_surface;
    void *dst_mmap;
    void *scaled_yuv;
    pthread_mutex_t mmap_lock;
};

int yuv_wq_init(struct yuv_wq *wq, int src_yuv_queue_len, process_yuv_fn process_yuv, int cpu_mask);
int yuv_wq_deinit(struct yuv_wq *wq);
int yuv_wq_start(struct yuv_wq *wq, VideoEncStream stream_id, int yuv_w, int yuv_h);
int yuv_wq_stop(struct yuv_wq *wq);
int yuv_wq_can_queue(struct yuv_wq *wq, int can_queue);

int queue_yuv_to_all_wq(struct yuv_frame *frame);

#endif
