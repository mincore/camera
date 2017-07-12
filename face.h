/* ===================================================
 * Copyright (C) 2015 chenshuangping All Right Reserved.
 *      Author: mincore@163.com
 *    Filename: face.h
 *     Created: 2015-07-13 07:30
 * Description:
 * ===================================================
 */
#ifndef _VIDEO_FACE_H
#define _VIDEO_FACE_H

#include <api/SnapEnc.h>
#include <MGDetectionAPI.h>
#include "yuv_wq.h"

#define LEN_PACKET offsetof(SnapEncPacket, reserved)
#define LEN_EXTRA  offsetof(FaceSnapExtraData, reserved)
#define LEN_FACE   offsetof(FaceSnap, reserved)

#pragma pack(push, 1)
struct face_info {
    unsigned short x,y,w,h;
    double quality;
    unsigned trackId;
};

struct face_info_group {
    uint64 sequence;
    uint16 face_info_size;
    uint16 face_info_count;
    struct face_info face_info[0];
};
#pragma pack(pop)

struct face_info_entry {
    struct yuv_frame *yuv_frame;
    struct list_head entry;
    struct face_info_group group;
};

struct facedetecter {
    struct yuv_wq wq;
    Queue face_queue;
    MG_DT_HANDLE fd_handle;
    MG_DT_RESULT fd_result;
    MG_FACEINFO face_info;
#ifdef HAVE_H264_SEI_FACE_INFO
    Queue face_info_queue;
#endif
    int enable_face_info_isp;
    Queue face_info_queue_isp;
    Queue snap_queue;

    Rect rect;
    double threshold;
    int yuv_w;
    int yuv_h;
};

typedef struct {
    SnapEncDesc desc;
    struct facedetecter facedetecter;
} SnapEncoderPriv;

int facedetecter_init(struct facedetecter *fd);
int facedetecter_deinit(struct facedetecter *fd);
int facedetecter_start(struct facedetecter *fd);
int facedetecter_stop(struct facedetecter *fd);
int facedetecter_get_packet(struct facedetecter *fd, SnapEncPacket **packet);
int facedetecter_release_packet(struct facedetecter *fd, SnapEncPacket *packet);
int facedetecter_set_config(struct facedetecter *fd, SnapEncConfig *config);

#ifdef HAVE_H264_SEI_FACE_INFO
Queue* get_face_info_queue();
#endif

struct face_info_entry* face_info_entry_create(struct facedetecter *fd, uint64 sequence, unsigned short face_info_count, struct yuv_frame *);
void face_info_entry_destory(struct face_info_entry*);

/* for isp */
void enable_face_info_isp(int enable);
struct face_info_group* isp_get_face();
void isp_release_face(struct face_info_group* g);

#endif
