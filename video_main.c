/* ===================================================
 * Copyright (C) 2015 chenshuangping All Right Reserved.
 *      Author: mincore@163.com
 *    Filename: video_main.c
 *     Created: 2015-07-13 07:32
 * Description:
 * ===================================================
 */
#include "internal.h"
#include "ipc.h"
#include "queue.h"
#include "videoin.h"
#include "IspProcess.h"

enum {
    IPC_VIN_CREATE,
    IPC_VIN_START,
    IPC_VIN_STOP,
    IPC_VIN_SET_COLOR,
    IPC_VIN_SET_EXPOSURER,
    IPC_VIN_SET_LIGHTREGULATION,
    IPC_VIN_SET_WHITEBALANCE,
    IPC_VIN_SET_IMAGENHANCE,
    IPC_VIN_SET_MIRROR,
    IPC_VIN_SET_STANDARD,
    IPC_VIN_SET_DAYNIGHT,
    IPC_VIN_SET_BACKGROUNDCOLOR,

    IPC_VENC_CREATE,
    IPC_VENC_START,
    IPC_VENC_STOP,
    IPC_VENC_SET_FORMAT,
    IPC_VENC_FORCE_I_FRAME,
    IPC_VENC_GET_PACKET,
    IPC_VENC_RELEASE_PACKET,

    IPC_VSNP_CREATE,
    IPC_VSNP_START,
    IPC_VSNP_STOP,
    IPC_VSNP_GET_PACKET,
    IPC_VSNP_RELEASE_PACKET,
    IPC_VSNP_SET_CONFIG,
};

struct ipc *gipc;

VideoEncoderPriv enc_priv[4];
SnapEncoderPriv snp_priv;

static int create_thread(void *(*func) (void *), void *arg)
{
    pthread_t pthread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    return pthread_create(&pthread, &attr, func, arg);
}

struct slow_task {
    struct list_head entry;
    struct ipc *ipc;
    int id;
    void *buf;
    int size;
};

struct Queue q_venc;
struct Queue q_vsnp;

static void _fix_snap_packet(SnapEncPacket *packet, int c)
{
    if (packet->buffer)
        packet->buffer = ipc_mem_fix_ptr(gipc, packet->buffer, c);

    if (!packet->extra)
        return;

    if (c) {
        FaceSnapExtraData* extra = (FaceSnapExtraData*)packet->extra;
        FaceSnap *snap = extra->snap;

        int i;
        for (i=0; i<extra->snapCount; i++) {
            if (snap[i].buffer)
                snap[i].buffer = ipc_mem_fix_ptr(gipc, snap[i].buffer, c);
        }

        extra->snap = ipc_mem_fix_ptr(gipc, extra->snap, c);
        packet->extra = ipc_mem_fix_ptr(gipc, packet->extra, c);
    }
    else {
        FaceSnapExtraData* extra = ipc_mem_fix_ptr(gipc, packet->extra, c);
        FaceSnap *snap = ipc_mem_fix_ptr(gipc, extra->snap, c);

        int i;
        for (i=0; i<extra->snapCount; i++) {
            if (snap[i].buffer)
                snap[i].buffer = ipc_mem_fix_ptr(gipc, snap[i].buffer, c);
        }

        extra->snap = snap;
        packet->extra = extra;
    }
}

static void *venc_slow_task_loop(void *arg)
{
    while (1) {
        struct slow_task *st =
            list_entry(queue_pop(&q_venc), struct slow_task, entry);
        int id2 = (st->id >> 8) & 0xFF;
        VideoEncPacket packet;
        encoder_get_packet(&enc_priv[id2].encoder, &packet);
        VIDEO_DEBUG
            ("IPC_VENC_GET_PACKET\tid:%03d, seq:%llu, %p, %ld, done",
             st->id, packet.sequence, packet.buffer, packet.length);
        packet.buffer = ipc_mem_fix_ptr(st->ipc, packet.buffer, 1);
        ipc_reply(st->ipc, st->id, &packet, sizeof(VideoEncPacket));
        free(st);
    }

    return NULL;
}

static void *vsnp_slow_task_loop(void *arg)
{
    while (1) {
        struct slow_task *st =
            list_entry(queue_pop(&q_vsnp), struct slow_task, entry);
        SnapEncPacket *packet;
        int len = facedetecter_get_packet(&snp_priv.facedetecter, &packet);
        VIDEO_DEBUG("IPC_VSNP_GET_PACKET done, st: %p, id: %d", st,
                st->id);
        _fix_snap_packet(packet, 1);
        ipc_reply(st->ipc, st->id, packet, len);
        free(st);
    }

    return NULL;
}

static void run_slow_task(struct Queue *q, struct ipc *ipc, int id, void *buf,
              int size)
{
    struct slow_task *st = malloc(sizeof(struct slow_task));
    st->ipc = ipc;
    st->id = id;
    st->buf = buf;
    st->size = size;
    queue_push(q, &st->entry);
}

static void _cb(struct ipc *ipc, int id, void *buf, int size)
{
    int reply = (id >> 30) == 0;
    int id2 = (id >> 8) & 0xFF;

    switch (id & 0xFF) {
    case IPC_VIN_CREATE:
        VIDEO_INFO("IPC_VIN_CREATE");
        v4l2_poll_init();
        VIDEO_INFO("IPC_VIN_CREATE done");
        break;
    case IPC_VIN_START:
        VIDEO_INFO("IPC_VIN_START");
        v4l2_poll_start(V4L2_FMT_WIDTH, V4L2_FMT_HEIGHT, 25);
        //isp_thread_create();
        VIDEO_INFO("IPC_VIN_START done");
        break;
    case IPC_VIN_STOP:
        VIDEO_INFO("IPC_VIN_STOP");
        v4l2_poll_stop();
        VIDEO_INFO("IPC_VIN_STOP done");
        break;
    case IPC_VIN_SET_COLOR:
        VIDEO_INFO("IPC_VIN_SET_COLOR");
        assert(size == sizeof(VideoColor));
        vin_set_color(NULL, (VideoColor *) buf);
        VIDEO_INFO("IPC_VIN_SET_COLOR done");
        break;
    case IPC_VIN_SET_EXPOSURER:
        VIDEO_INFO("IPC_VIN_SET_EXPOSURER");
        assert(size == sizeof(VideoExposure));
        vin_set_exposurer(NULL, (VideoExposure *) buf);
        VIDEO_INFO("IPC_VIN_SET_EXPOSURER done");
        break;
    case IPC_VIN_SET_LIGHTREGULATION:
        VIDEO_INFO("IPC_VIN_SET_LIGHTREGULATION");
        assert(size == sizeof(VideoLightRegulation));
        vin_set_lightregulation(NULL, (VideoLightRegulation *) buf);
        VIDEO_INFO("IPC_VIN_SET_LIGHTREGULATION done");
        break;
    case IPC_VIN_SET_WHITEBALANCE:
        VIDEO_INFO("IPC_VIN_SET_WHITEBALANCE");
        assert(size == sizeof(VideoWhiteBalance));
        vin_set_whitebalance(NULL, (VideoWhiteBalance *) buf);
        VIDEO_INFO("IPC_VIN_SET_WHITEBALANCE done");
        break;
    case IPC_VIN_SET_IMAGENHANCE:
        VIDEO_INFO("IPC_VIN_SET_IMAGENHANCE");
        assert(size == sizeof(VideoImageEnhancement));
        vin_set_imagenhance(NULL, (VideoImageEnhancement *) buf);
        VIDEO_INFO("IPC_VIN_SET_IMAGENHANCE done");
        break;
    case IPC_VIN_SET_MIRROR:
        VIDEO_INFO("IPC_VIN_SET_MIRROR");
        assert(size == sizeof(VideoMirror));
        vin_set_mirror(NULL, (VideoMirror *) buf);
        VIDEO_INFO("IPC_VIN_SET_MIRROR done");
        break;
    case IPC_VIN_SET_STANDARD:
        VIDEO_INFO("IPC_VIN_SET_STANDARD");
        assert(size == sizeof(VideoStandard));
        vin_set_standard(NULL, (VideoStandard *) buf);
        VIDEO_INFO("IPC_VIN_SET_STANDARD done");
        break;
    case IPC_VIN_SET_DAYNIGHT:
        VIDEO_INFO("IPC_VIN_SET_DAYNIGHT");
        assert(size == sizeof(VideoDayNight));
        vin_set_daynight(NULL, (VideoDayNight *) buf);
        VIDEO_INFO("IPC_VIN_SET_DAYNIGHT done");
        break;
    case IPC_VIN_SET_BACKGROUNDCOLOR:
        VIDEO_INFO("IPC_VIN_SET_BACKGROUNDCOLOR");
        assert(size == sizeof(Color));
        vin_set_backgroundcolor(NULL, (Color *) buf);
        VIDEO_INFO("IPC_VIN_SET_BACKGROUNDCOLOR done");
        break;
    case IPC_VENC_CREATE:
        queue_init(&q_venc, 0);
        create_thread(venc_slow_task_loop, NULL);
        VIDEO_INFO("IPC_VENC_CREATE");
        assert(id2 < 4);
        enc_priv[id2].desc.stream = id2;
        encoder_init(&enc_priv[id2].encoder);
        VIDEO_INFO("IPC_VENC_CREATE done");
        break;
    case IPC_VENC_START:
        VIDEO_INFO("IPC_VENC_START");
        assert(id2 < 4);
        encoder_start(&enc_priv[id2].encoder);
        VIDEO_INFO("IPC_VENC_START done");
        break;
    case IPC_VENC_STOP:
        VIDEO_INFO("IPC_VENC_STOP");
        assert(id2 < 4);
        encoder_stop(&enc_priv[id2].encoder);
        VIDEO_INFO("IPC_VENC_STOP done");
        break;
    case IPC_VENC_SET_FORMAT:
        VIDEO_INFO("IPC_VENC_SET_FORMAT");
        assert(id2 < 4 && size == sizeof(VideoEncFormat));
        memcpy(&enc_priv[id2].format, (VideoEncFormat *) buf, size);
        VIDEO_INFO("IPC_VENC_SET_FORMAT done");
        break;
    case IPC_VENC_FORCE_I_FRAME:
        VIDEO_INFO("IPC_VENC_FORCE_I_FRAME");
        assert(id2 < 4);
        enc_priv[id2].encoder.force_I_frame = 1;
        VIDEO_INFO("IPC_VENC_FORCE_I_FRAME done");
        break;
    case IPC_VENC_GET_PACKET:
        assert(id2 < 4);
        reply = 0;
        run_slow_task(&q_venc, ipc, id, NULL, 0);
        break;
    case IPC_VENC_RELEASE_PACKET:
        assert(id2 < 4 && size == sizeof(VideoEncPacket));
        ((VideoEncPacket *)buf)->buffer = ipc_mem_fix_ptr(ipc, ((VideoEncPacket *)buf)->buffer, 0);
        encoder_release_packet(&enc_priv[id2].encoder,
                       (VideoEncPacket *) buf);
        break;
    case IPC_VSNP_CREATE:
        VIDEO_INFO("IPC_VSNP_CREATE");
        queue_init(&q_vsnp, 0);
        create_thread(vsnp_slow_task_loop, NULL);
        facedetecter_init(&snp_priv.facedetecter);
        VIDEO_INFO("IPC_VSNP_CREATE done");
        break;
    case IPC_VSNP_START:
        VIDEO_INFO("IPC_VSNP_START");
        facedetecter_start(&snp_priv.facedetecter);
        VIDEO_INFO("IPC_VSNP_START done");
        break;
    case IPC_VSNP_STOP:
        VIDEO_INFO("IPC_VSNP_STOP");
        facedetecter_stop(&snp_priv.facedetecter);
        VIDEO_INFO("IPC_VSNP_STOP done");
        break;
    case IPC_VSNP_GET_PACKET:
        reply = 0;
        run_slow_task(&q_vsnp, ipc, id, NULL, 0);
        break;
    case IPC_VSNP_RELEASE_PACKET:
        assert(size == sizeof(SnapEncPacket));
        _fix_snap_packet((SnapEncPacket*)buf, 0);
        facedetecter_release_packet(&snp_priv.facedetecter,
                        (SnapEncPacket*)buf);
        break;
    case IPC_VSNP_SET_CONFIG:
        VIDEO_INFO("IPC_VSNP_SET_CONFIG");
        assert(size == sizeof(SnapEncConfig));
        facedetecter_set_config(&snp_priv.facedetecter,
                        (SnapEncConfig*)buf);
        VIDEO_INFO("IPC_VSNP_SET_CONFIG done");
        break;
    }

    if (reply)
        ipc_reply(ipc, id, NULL, 0);
}

int main(int argc, char *argv[])
{
    VIDEO_INFO("video start");

    gipc = ipc_create(_cb);
    if (!gipc)
        return -1;

    ipc_wait(gipc);

    VIDEO_INFO("video quit");

    _exit(0);

    return 0;
}
