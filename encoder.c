/* ===================================================
 * Copyright (C) 2015 chenshuangping All Right Reserved.
 *      Author: mincore@163.com
 *    Filename: encoder.c
 *     Created: 2015-07-13 07:30
 * Description:
 * ===================================================
 */
#include <errno.h>
#include "internal.h"
#include "packet.h"
#include "encoder.h"
#include "osd.h"
#include "h264.h"
#include "face.h"

#include "ipc.h"

#define MAX_CACHED_FRAME 25

struct h264_packet {
    struct packet packet;
#ifdef HAVE_H264_SEI_FACE_INFO
    int wait_for_face_info;
#endif
};

static struct h264_packet* h264_packet_alloc(void *data, int size, int type, struct yuv_frame *yuv_frame)
{
    struct h264_packet *h264_packet = malloc(sizeof(struct h264_packet));
#ifdef HAVE_H264_SEI_FACE_INFO
    h264_packet->wait_for_face_info = yuv_frame->wait_for_face_info;
#endif
#if 0
    void *buffer = h264_clone_and_insert_user_data(data, size, NULL, 0, &size);
    packet_init(&h264_packet->packet, buffer, size, yuv_frame->sequence, yuv_frame->pts, type);
#else
    packet_init(&h264_packet->packet, data, size, yuv_frame->sequence, yuv_frame->pts, type);
#endif
    return h264_packet;
}

static void h264_packet_free(struct h264_packet *h264_packet)
{
    ipc_mem_free(gipc, 0, h264_packet->packet.data);
    //free(h264_packet->packet.data);
    free(h264_packet);
}

#ifdef HAVE_H264_SEI_FACE_INFO
static struct face_info_entry *encoder_get_face_info(struct encoder *encoder, struct h264_packet *h264_packet)
{
    struct face_info_entry *face_info_entry;
    uint64 sequence = h264_packet->packet.sequence;
    Queue *face_info_queue = get_face_info_queue();

    while (1) {
        struct list_head *entry = queue_peek_nowait(face_info_queue);
        if (!entry)
            return NULL;

        face_info_entry = container_of(entry, struct face_info_entry, entry);
        if (face_info_entry->group.sequence == sequence) {
            queue_del(face_info_queue, entry);
            VIDEO_DEBUG("%llu == %llu", face_info_entry->group.sequence, sequence);
            break;
        } else if (face_info_entry->group.sequence > sequence) {
            VIDEO_DEBUG("%llu > %llu", face_info_entry->group.sequence, sequence);
            face_info_entry = NULL;
            break;
        } else {
            queue_del(face_info_queue, entry);
            VIDEO_DEBUG("%llu < %llu", face_info_entry->group.sequence, sequence);
            face_info_entry_destory(face_info_entry);
            face_info_entry = NULL;
            continue;
        }
    }

    return face_info_entry;
}

void encoder_wait_for_face_info(struct encoder *encoder, struct h264_packet *h264_packet)
{
    if (!h264_packet->wait_for_face_info)
        return;

    Queue *face_info_queue = get_face_info_queue();
    if (!face_info_queue)
        return;

    uint64 sequence = h264_packet->packet.sequence;
    struct face_info_entry *face_info_entry;

again:
    face_info_entry = encoder_get_face_info(encoder, h264_packet);
    if (face_info_entry == NULL) {
        unsigned long long expire_time = now() + 240;
        struct timespec ts = { .tv_sec = expire_time/1000, .tv_nsec = (expire_time%1000)*1000000 };
        pthread_mutex_lock(&face_info_queue->lock);
        int ret = pthread_cond_timedwait(&face_info_queue->cond, &face_info_queue->lock, &ts);
        pthread_mutex_unlock(&face_info_queue->lock);

        if (ret == 0)
            goto again;
    }

    if (face_info_entry) {
        if (face_info_entry->group.face_info_count) {
            void *old_buffer = h264_packet->packet.data;
            size_t old_len = h264_packet->packet.data_len;
            void *sei_buffer = &face_info_entry->group;
            size_t sei_len = sizeof(struct face_info_group) +
                face_info_entry->group.face_info_count * face_info_entry->group.face_info_size;

            //VIDEO_INFO("seq %llu face_info_count: %d, sei_len:%d", sequence, face_info_entry->group.face_info_count, sei_len);
            h264_packet->packet.data = h264_clone_and_insert_user_data(
                    old_buffer, old_len, sei_buffer, sei_len, &h264_packet->packet.data_len);
            free(old_buffer);
        }
        face_info_entry_destory(face_info_entry);
    }
}
#endif

int encoder_get_packet(struct encoder *encoder, VideoEncPacket *out)
{
    struct packet *packet = list_entry(queue_pop(&encoder->h264_queue), struct packet, entry);
    struct h264_packet *h264_packet = container_of(packet, struct h264_packet, packet);

#ifdef HAVE_H264_SEI_FACE_INFO
    if (packet->sequence == 0) {
        encoder->first_packet_time = now();
    }
    encoder_wait_for_face_info(encoder, h264_packet);
#endif

    out->width = encoder->wq.yuv_w;
    out->height= encoder->wq.yuv_h;
    out->utc = packet->pts;
    out->pts = packet->pts - v4l2_poll_first_pts();
    out->sequence = packet->sequence;
    out->buffer = packet->data; /* FIXME: use fixed memory so that user don't have to free it */
    out->length = packet->data_len;
    out->type = packet->type;

    free(h264_packet);

    return 0;
}

int encoder_release_packet(struct encoder *encoder, VideoEncPacket *packet)
{
    ipc_mem_free(gipc, 0, packet->buffer);
    //free(packet->buffer);
    return 0;
}

static int encoder_encode_h264(struct yuv_wq *wq, struct yuv_frame *yuv_frame)
{
    struct encoder *encoder = container_of(wq, struct encoder, wq);
    VideoEncoderPriv *priv = container_of(encoder, VideoEncoderPriv, encoder);
    GPU_ENC_OUTPUT_PARAM output_param = {.coded_data = encoder->coded_data};
    GPU_ENC_INPUT_PARAM input_param;

    output_param.coded_data = ipc_mem_alloc(gipc, 0, 512*1024);
    if (!output_param.coded_data) {
        VIDEO_INFO("alloc memory failed");
        return 0;
    }

    const char *stream_string = stream_to_string(priv->desc.stream);
    void *yuv_data = yuv_frame_get_data(yuv_frame);

    if (wq->yuv_w >= 640 )
        osd_draw_timestamp48(yuv_data, wq->yuv_w, wq->yuv_h, 50, 20);
    else
        osd_draw_timestamp16(yuv_data, wq->yuv_w, wq->yuv_h, 50, 20);

    gpu_encode_input_param_init(&input_param, GPU_PIC_FMT_NV12, wq->yuv_w, wq->yuv_h, yuv_data);
    input_param.force_I_frame = encoder->force_I_frame;

    GPU_Enc_Encode(encoder->encode_ctx, &input_param, &output_param);

    if (input_param.force_I_frame) {
        encoder->force_I_frame = 0;
    }

    if (output_param.coded_len == 0) {
        VIDEO_ERROR("decode %s coded_len == 0", stream_string);
        return -1;
    }

    struct h264_packet *h264_packet;
    h264_packet = h264_packet_alloc(output_param.coded_data,
                                    output_param.coded_len,
                                    slice_type_to_frame_type(output_param.slice_type),
                                    yuv_frame);

    if (queue_push(&encoder->h264_queue, &h264_packet->packet.entry) == -1) {
        VIDEO_WARN("queue h264 to %s failed, seq:%llu", stream_string, yuv_frame->sequence);
        h264_packet_free(h264_packet);
    }

    if (debug_time())
        VIDEO_INFO("seq:%llu, ENCODE H264 TIME:%llu", yuv_frame->sequence, now());

    return 0;
}

int encoder_init(struct encoder *encoder)
{
    memset(encoder, 0, sizeof(struct encoder));
    queue_init(&encoder->h264_queue, 0);
    yuv_wq_init(&encoder->wq, 8, encoder_encode_h264, 0);
    return 0;
}

int encoder_deinit(struct encoder *encoder)
{
    return 0;
}

int encoder_start(struct encoder *encoder)
{
    VideoEncoderPriv *priv = container_of(encoder, VideoEncoderPriv, encoder);
    GPU_ENC_OPEN_PARAM enc_param;
    int w;
    int h;

    get_resolution(priv->format.resolution, &w, &h);
    VIDEO_INFO("%s width:%d height:%d", stream_to_string(priv->desc.stream), w, h);

    enc_param.type = GPU_FRAME_TYPE_H264;
    enc_param.width = w;
    enc_param.height = h;
    enc_param.frame_rate = priv->format.fps;
    enc_param.intra_count = priv->format.gop;
    enc_param.bit_rate = priv->format.bitRate.size;
    enc_param.qp_value = 70; /*default qp_value is 70*/
    enc_param.rate_control = GPU_ENC_RC_CQP;

    if (priv->format.bitRate.type == videoEncBitrateCtrlConstant)
        enc_param.rate_control = GPU_ENC_RC_CBR;
    else {
        int quality = priv->format.bitRate.detail.variable.quality;
        assert(quality >= 1 && quality <= 6);
        enc_param.qp_value += 5 * (quality - 1);
    }

    if (enc_param.frame_rate == 0)
        enc_param.frame_rate = 25;

    if (enc_param.intra_count == 0)
        enc_param.intra_count = 50;

    if (enc_param.bit_rate == 0)
        enc_param.bit_rate = -1;

    VIDEO_INFO("fps:%d, intra_count:%d, bitRateType:%d", enc_param.frame_rate, enc_param.intra_count, enc_param.rate_control);

    assert(encoder->encode_ctx == NULL);
    encoder->encode_ctx = GPU_Enc_Open(&enc_param);
    assert(encoder->encode_ctx);
    encoder->coded_data = malloc(FRAME_SIZE(w, h));

    yuv_wq_start(&encoder->wq, priv->desc.stream, w, h);

    return 0;
}

int encoder_stop(struct encoder *encoder)
{
    yuv_wq_stop(&encoder->wq);

    if (encoder->encode_ctx) {
        GPU_Enc_Close(encoder->encode_ctx);
        encoder->encode_ctx = NULL;
    }

    if (encoder->coded_data) {
        free(encoder->coded_data);
        encoder->coded_data = NULL;
    }

    return 0;
}
