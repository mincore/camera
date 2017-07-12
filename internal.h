/* ===================================================
 * Copyright (C) 2015 chenshuangping All Right Reserved.
 *      Author: mincore@163.com
 *    Filename: internal.h
 *     Created: 2015-07-13 07:30
 * Description:
 * ===================================================
 */
#ifndef _VIDEO_INTELNAL_H
#define _VIDEO_INTELNAL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <semaphore.h>

#include <GPU_API.h>
#include <api/VideoEnc.h>
#include <api/VideoIn.h>
#include <api/SnapEnc.h>

#include "queue.h"
#include "v4l2_poll.h"
#include "utils.h"
#include "xcam.h"
#include "encoder.h"
#include "face.h"
#include "debug.h"

#define PRINT_RED          "\033[40;31m"
#define PRINT_GREEN        "\033[40;32m"
#define PRINT_YELLOW       "\033[40;33m"
#define PRINT_BLUE         "\033[40;34m"
#define PRINT_END          "\033[0m\n"

#define VIDEO_INFO(fmt, ...) do { \
    printf (PRINT_GREEN "VIDEO INFO %s:%d: " fmt PRINT_END, __func__, __LINE__, ## __VA_ARGS__); \
} while (0)

#define VIDEO_WARN(fmt, ...) do { \
    printf (PRINT_YELLOW "VIDEO WARN %s:%d: " fmt PRINT_END, __func__, __LINE__, ## __VA_ARGS__); \
} while (0)

#define VIDEO_ERROR(fmt, ...) do { \
    printf (PRINT_RED "VIDEO ERROR %s:%d: " fmt PRINT_END, __func__, __LINE__, ## __VA_ARGS__); \
} while (0)

#ifdef DEBUG
#define VIDEO_DEBUG(fmt, ...) do { \
    printf (PRINT_BLUE "VIDEO DEBUG %s:%d: " fmt PRINT_END, __func__, __LINE__, ## __VA_ARGS__); \
} while (0)
#else
#define VIDEO_DEBUG(fmt, ...)
#endif

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

#define FRAME_SIZE(w, h) (((w)*(h))+(((w)*(h))>>1))
#define ALIGN_UP(i, n) (((i) + (n) - 1) & ~((n) - 1))
#define ALIGN_DOWN(i, n) ((i) & ~((n) - 1))

#define MIN(a, b) ((a) < (b)? (a): (b))
#define MAX(a, b) ((a) > (b)? (a): (b))
#define MIN3(a,b,c) MIN(a, MIN(b,c))
#define MAX3(a,b,c) MAX(a, MIN(b,c))

struct yuv_frame {
    struct list_head entry;

    XCAM_BUF_HANDLE handle;
    void *data;
    int size;

    uint64 sequence;
    uint64 pts;

#ifdef HAVE_H264_SEI_FACE_INFO
    int wait_for_face_info;
#endif
};

struct yuv_frame *yuv_frame_alloc(XCAM_BUF_HANDLE handle, void *buffer, int size, uint64 sequence, uint64 pts);
void   yuv_frame_free(struct yuv_frame *yuv_frame);
void*  yuv_frame_get_data(struct yuv_frame *yuv_frame);

extern struct ipc *gipc;

#endif
