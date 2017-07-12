/* ===================================================
 * Copyright (C) 2015 chenshuangping All Right Reserved.
 *      Author: mincore@163.com
 *    Filename: yuv_wq.c
 *     Created: 2015-07-13 07:32
 * Description:
 * ===================================================
 */
#define _GNU_SOURCE
#include <sched.h>

#include "yuv_wq.h"
#include "internal.h"

#define SCALE_WQ_COUNT (videoEncStreamNumber+1)
#define NON_SCALE_WQ_COUNT (videoEncStreamNumber)

#define WQ_PUSH(wq, f) do {\
    if ((f) && queue_push(&wq->src_yuv_queue, &(f)->entry) == -1) {\
        yuv_frame_free((f));\
    }\
} while(0)

#define WQ(wq) ((wq)->need_scale ? SCALE_WQ[(wq)->stream_id] : NON_SCALE_WQ[(wq)->stream_id])

struct yuv_wq *SCALE_WQ[SCALE_WQ_COUNT];
struct yuv_wq *NON_SCALE_WQ[NON_SCALE_WQ_COUNT];

static void *g_scale_ctx;
static GPU_SURFACE_ID g_src_surface;
static void *g_src_mmap;

static void *yuv_wq_loop(void *p)
{
    struct yuv_wq *wq = (struct yuv_wq *)p;
    struct yuv_frame *src_frame;
    struct list_head *entry;

    while (1) {
        entry = queue_peek_nowait(&wq->src_yuv_queue);
        if (!entry) {
            if (wq->quit)
                break;
            queue_wait(&wq->src_yuv_queue);
            continue;
        }

        src_frame = list_entry(entry, struct yuv_frame, entry);
        if (!atomic_read(&wq->stopping)) {
            wq->process_yuv(wq, src_frame);
        }
        queue_del(&wq->src_yuv_queue, entry);

        yuv_frame_free(src_frame);

        if (queue_count(&wq->src_yuv_queue) == 0 && wq->stopping)
            queue_signal(&wq->src_yuv_queue);
    }

    return NULL;
}

static void bind_cpu(pthread_attr_t *attr, int cpu_mask)
{
    if (cpu_mask > 0) {
        cpu_set_t cpus;
        int i;
        CPU_ZERO(&cpus);

        for (i=0; cpu_mask; cpu_mask = cpu_mask >> 1, i++) {
            if (cpu_mask & 1)
                CPU_SET(i, &cpus);
        }

        pthread_attr_setaffinity_np(attr, sizeof(cpu_set_t), &cpus);
    }
}

int yuv_wq_init(struct yuv_wq *wq, int src_yuv_queue_len, process_yuv_fn process_yuv, int cpu_mask)
{
    memset(wq, 0, sizeof(*wq));
    queue_init(&wq->src_yuv_queue, src_yuv_queue_len);
    wq->process_yuv = process_yuv;

    pthread_mutex_init(&wq->mmap_lock, NULL);

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    bind_cpu(&attr, cpu_mask);
    pthread_create(&wq->thread, &attr, yuv_wq_loop, wq);

    return 0;
}

int yuv_wq_deinit(struct yuv_wq *wq)
{
    return -1;
}

static int yuv_wq_need_scale(struct yuv_wq *wq)
{
    return !(wq->yuv_w == V4L2_FMT_WIDTH && wq->yuv_h == V4L2_FMT_HEIGHT);
}

static void yuv_wq_scale_prepare(struct yuv_wq *wq)
{
    if (g_src_mmap == NULL) {
        GPU_DEC_OPEN_PARAM dec_param = {.type = GPU_FRAME_TYPE_H264,};
        g_scale_ctx = GPU_Dec_Open(&dec_param);
        g_src_surface = GPU_Alloc_Surface(g_scale_ctx, V4L2_FMT_WIDTH, V4L2_FMT_HEIGHT, GPU_PIC_FMT_NV12, 1);
        g_src_mmap = GPU_Map_Surface(g_scale_ctx, g_src_surface, ACCESS_FOR_READ);
    }

    if (wq->need_scale) {
        pthread_mutex_lock(&wq->mmap_lock);
        assert(wq->dst_mmap == NULL);
        wq->dst_surface = GPU_Alloc_Surface(g_scale_ctx, wq->yuv_w, wq->yuv_h, GPU_PIC_FMT_NV12, 1);
        wq->dst_mmap = GPU_Map_Surface(g_scale_ctx, wq->dst_surface, ACCESS_FOR_WRITE);
        wq->scaled_yuv = malloc(FRAME_SIZE(wq->yuv_w, wq->yuv_h));
        pthread_mutex_unlock(&wq->mmap_lock);
    }
}

static void yuv_wq_scale_done(struct yuv_wq *wq)
{
    if (wq->need_scale) {
        pthread_mutex_lock(&wq->mmap_lock);
        GPU_Unmap_Surface(g_scale_ctx, wq->dst_surface);
        GPU_Clear_Surface(g_scale_ctx, wq->dst_surface);
        wq->dst_mmap = NULL;
        free(wq->scaled_yuv);
        wq->scaled_yuv = NULL;
        pthread_mutex_unlock(&wq->mmap_lock);
    }
}

int yuv_wq_can_queue(struct yuv_wq *wq, int can_queue)
{
    if (wq->need_scale)
        SCALE_WQ[wq->stream_id] = can_queue ? wq : NULL;
    else
        NON_SCALE_WQ[wq->stream_id] = can_queue ? wq : NULL;

    return 0;
}

int yuv_wq_start(struct yuv_wq *wq, VideoEncStream stream_id, int yuv_w, int yuv_h)
{
    wq->stream_id = stream_id;
    wq->yuv_w = yuv_w;
    wq->yuv_h = yuv_h;
    wq->need_scale = yuv_wq_need_scale(wq);
    yuv_wq_scale_prepare(wq);
    atomic_set(&wq->stopping, 0);
    yuv_wq_can_queue(wq, 1);
    return 0;
}

int yuv_wq_stop(struct yuv_wq *wq)
{
    yuv_wq_can_queue(wq, 0);
    atomic_set(&wq->stopping, 1);
    queue_signal(&wq->src_yuv_queue);
    queue_wait_empty(&wq->src_yuv_queue);
    yuv_wq_scale_done(wq);
    return 0;
}

static void *yuv_wq_scale(struct yuv_wq *wq, int *out_len)
{
    void *out = NULL;

    GPU_RECT src_rect;
    src_rect.x = src_rect.y = 0;
    src_rect.width = V4L2_FMT_WIDTH;
    src_rect.height = V4L2_FMT_HEIGHT;

    GPU_RECT dst_rect;
    dst_rect.x = dst_rect.y = 0;
    dst_rect.width = wq->yuv_w;
    dst_rect.height = wq->yuv_h;

    pthread_mutex_lock(&wq->mmap_lock);
    if (wq->dst_mmap && wq->scaled_yuv) {
        assert(wq->need_scale);
        GPU_Transform_Surface(g_scale_ctx, g_src_surface, &src_rect, wq->dst_surface, &dst_rect);
        *out_len = FRAME_SIZE(dst_rect.width, dst_rect.height);
        out = malloc(*out_len);
        surface_nv12_copy_out(wq->dst_mmap, out, dst_rect.width, dst_rect.height);
    }
    pthread_mutex_unlock(&wq->mmap_lock);

    return out;
}

static struct yuv_frame *yuv_wq_create_scaled_yuv_frame(struct yuv_wq *wq, struct yuv_frame *yuv_frame)
{
    void *scaled_yuv;
    int len;

    scaled_yuv = yuv_wq_scale(wq, &len);
    if (!scaled_yuv)
        return NULL;

    return yuv_frame_alloc(wq->need_xcam_buf ? yuv_frame->handle : NULL,
                            scaled_yuv, len, yuv_frame->sequence, yuv_frame->pts);
}

static void queue_yuv_to_scale_wq(struct yuv_frame *yuv_frame)
{
    struct yuv_frame *f;
    struct yuv_wq *wq;
    int i;
    int filled = 0;

    for (i=0; i<SCALE_WQ_COUNT; i++) {
        wq = SCALE_WQ[i];
        if (wq) {
            if (filled == 0) {
                surface_nv12_copy_in(g_src_mmap, yuv_frame_get_data(yuv_frame), V4L2_FMT_WIDTH, V4L2_FMT_HEIGHT);
                filled = 1;
            }
            f = yuv_wq_create_scaled_yuv_frame(wq, yuv_frame);
            WQ_PUSH(wq, f);
        }
    }
}

static void queue_yuv_to_non_scale_wq(struct yuv_frame *yuv_frame)
{
    struct yuv_frame *f;
    struct yuv_wq *wq;
    int i;

    for (i=0; i<NON_SCALE_WQ_COUNT; i++) {
        wq = NON_SCALE_WQ[i];
        if (wq) {
            f = yuv_frame_alloc(yuv_frame->handle, NULL, 0, yuv_frame->sequence, yuv_frame->pts);
            WQ_PUSH(wq, f);
        }
    }
}

int queue_yuv_to_all_wq(struct yuv_frame *yuv_frame)
{
    queue_yuv_to_scale_wq(yuv_frame);
    queue_yuv_to_non_scale_wq(yuv_frame);
    return 0;
}
