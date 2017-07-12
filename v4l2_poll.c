/* ===================================================
 * Copyright (C) 2015 chenshuangping All Right Reserved.
 *      Author: mincore@163.com
 *    Filename: v4l2_poll.c
 *     Created: 2015-07-13 07:32
 * Description:
 * ===================================================
 */
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "xcam_def.h"
#include "xcam_v4l2.h"
#include "xcam_x3a.h"
#include "xcam_cl.h"
#include "xcam.h"

#include "internal.h"
#include "queue.h"
#include "yuv_wq.h"
#include "GPU_API.h"
#include "osd.h"
#include "debug.h"

extern void isp_sem_init();
static struct Queue g_v4l2_poll;
static uint64 g_sequence = 0;
static uint64 g_first_pts = 0;

struct yuv_frame *yuv_frame_alloc(XCAM_BUF_HANDLE handle, void *data, int size, uint64 sequence, uint64 pts)
{
    struct yuv_frame *f;

    f = calloc(1, sizeof(struct yuv_frame));
    f->sequence = sequence;
    f->pts = pts;
    f->handle = handle ? xcam_buf_get(handle) : NULL;
    f->data = data;
    f->size = size;
    INIT_LIST_HEAD(&f->entry);

    return f;
}

void *yuv_frame_get_data(struct yuv_frame *f)
{
    return f->data ? f->data : xcam_buf_get_data(f->handle);
}

void yuv_frame_free(struct yuv_frame *f)
{
    if (f->handle) {
        if (f->data == xcam_buf_get_data(f->handle))
            f->data = NULL;
        xcam_buf_put(f->handle);
    }

    if (f->data)
        free(f->data);

    free(f);
}

static void* v4l2_poll_thread(void *p)
{
    Queue *q = (Queue*)p;
    struct yuv_frame *yuv_frame;

    while (1) {
        yuv_frame = list_entry(queue_pop(q), struct yuv_frame, entry);
        //VIDEO_INFO("seq:%llu YUV TIME: %llu", yuv_frame->sequence, now());
        queue_yuv_to_all_wq(yuv_frame);
        yuv_frame_free(yuv_frame);
    }

    return NULL;
}

static void on_buf_ready(XCAM_BUF_HANDLE handle)
{
    struct yuv_frame *f;
    unsigned long long pts = now();

    if (g_first_pts == 0)
        g_first_pts = pts;

    if (debug_time())
        VIDEO_INFO("seq:%llu YUV TIME: %llu", g_sequence, pts);

    f = yuv_frame_alloc(handle, NULL, 0, g_sequence++, pts);

    if (queue_push(&g_v4l2_poll, &f->entry) == -1) {
        VIDEO_WARN("push to g_v4l2_poll failed");
        yuv_frame_free(f);
    }
}

unsigned long long v4l2_poll_first_pts()
{
    return g_first_pts;
}

static void on_stats_ready(XCAM_3AStats *stats)
{
}

static void v4l2_start_poll()
{
    pthread_t pthread;
    pthread_attr_t attr;
    struct sched_param  schedParam = {.sched_priority = 1};

    assert(pthread_attr_init(&attr) == 0);
    assert(pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED) == 0);
    assert(pthread_attr_setschedpolicy(&attr, SCHED_FIFO) == 0);
    assert(pthread_attr_setschedparam(&attr, &schedParam) == 0);
    assert(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) == 0);
    assert(pthread_create(&pthread, &attr, v4l2_poll_thread, &g_v4l2_poll) == 0);
}

int v4l2_poll_init()
{
    queue_init(&g_v4l2_poll, 25);
    v4l2_start_poll();

    xcam_init(on_buf_ready, on_stats_ready);
    xcam_v4l2_init();
    xcam_v4l2_set_sensor_id(0);
    xcam_v4l2_set_capture_mode(V4L2_CAPTURE_MODE);
    xcam_v4l2_set_mem_type(V4L2_MEMORY_DMABUF);
    xcam_v4l2_set_buffer_count(V4L2_BUFFER_COUNT);
    xcam_x3a_init();

    GPU_Enc_Init();
    GPU_Dec_Init();
    isp_sem_init();
    osd_init();
    debug_init();

    return 0;
}

int v4l2_poll_deinit()
{
    return 0;
}

int v4l2_poll_start(int w, int h, int fps)
{
    xcam_v4l2_set_framerate(fps, 1);
    xcam_v4l2_set_format(w, h, V4L2_PIX_FMT_NV12, w*h*3/2);
    xcam_start();
    return 0;
}

int v4l2_poll_stop()
{
    xcam_stop();
    return 0;
}
