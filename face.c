/* ===================================================
 * Copyright (C) 2015 chenshuangping All Right Reserved.
 *      Author: mincore@163.com
 *    Filename: face.c
 *     Created: 2015-07-13 07:30
 * Description:
 * ===================================================
 */
#include <semaphore.h>
#include <math.h>
#include <dlfcn.h>

#include "internal.h"
#include "packet.h"
#include "face.h"
#include "jpeg.h"
#include "ipc.h"

#define SNAP_YUV_W 960
#define SNAP_YUV_H 540

static MG_DT_HANDLE (*CALL_MG_DT_CreateHandle)          ();
static MG_RETCODE   (*CALL_MG_DT_ReleaseHandle)         (MG_DT_HANDLE handle);
static MG_DT_RESULT (*CALL_MG_DT_CreateResult)          (MG_FACEINFO* info);
static MG_RETCODE   (*CALL_MG_DT_ReleaseResult)         (MG_DT_RESULT result);
static MG_RETCODE   (*CALL_MG_DT_SetMode)               (MG_DT_HANDLE handle, MG_DETECTION_MODE mode);
static MG_RETCODE   (*CALL_MG_DT_SetFaceSizeRange)      (MG_DT_HANDLE handle, MG_INT32 minSize, MG_INT32 maxSize);
static MG_RETCODE   (*CALL_MG_DT_DetectFace)            (MG_DT_HANDLE handle, MG_IMAGEWRAPPER image, MGOUT MG_DT_RESULT result);
static MG_RETCODE   (*CALL_MG_DT_GetFaceCount)          (MG_DT_RESULT result, MG_INT32 *count);
static MG_RETCODE   (*CALL_MG_DT_GetFaceInfo)           (MG_DT_RESULT result, MG_INT32 index, MGOUT MG_FACEINFO *info);
static MG_RETCODE   (*CALL_MG_DT_SetTrackingWorkerCPUs) (MG_DT_HANDLE handle, int nr, const int *cpuIDs);

static void MGOPS_init()
{
    void *h = dlopen("/usr/lib/liblandmark.so", RTLD_NOW | RTLD_GLOBAL);
    CALL_MG_DT_CreateHandle          = dlsym(h, "MG_DT_CreateHandle");
    CALL_MG_DT_ReleaseHandle         = dlsym(h, "MG_DT_ReleaseHandle");
    CALL_MG_DT_CreateResult          = dlsym(h, "MG_DT_CreateResult");
    CALL_MG_DT_ReleaseResult         = dlsym(h, "MG_DT_ReleaseResult");
    CALL_MG_DT_SetMode               = dlsym(h, "MG_DT_SetMode");
    CALL_MG_DT_SetFaceSizeRange      = dlsym(h, "MG_DT_SetFaceSizeRange");
    CALL_MG_DT_DetectFace            = dlsym(h, "MG_DT_DetectFace");
    CALL_MG_DT_GetFaceCount          = dlsym(h, "MG_DT_GetFaceCount");
    CALL_MG_DT_GetFaceInfo           = dlsym(h, "MG_DT_GetFaceInfo");
    CALL_MG_DT_SetTrackingWorkerCPUs = dlsym(h, "MG_DT_SetTrackingWorkerCPUs");
}

static SnapEncPacket* alloc_SnapEncPacket(struct face_info_entry *fie)
{
    SnapEncPacket *p = calloc(1, LEN_PACKET);
    int face_count = fie->group.face_info_count;

    p->pts = fie->yuv_frame->pts;
    p->sequence = fie->yuv_frame->sequence;
    p->extraType = SnapExtraDataTypeFaceDetect;

    p->extra = ipc_mem_alloc(gipc, 1, LEN_EXTRA);
    if (!p->extra)
        return p;

    FaceSnapExtraData *extra = p->extra;
    extra->snap = NULL;
    extra->snapCount = face_count;

    if (face_count) {
        extra->snap = ipc_mem_alloc(gipc, 1, LEN_FACE*face_count);
        if (!extra->snap)
            return p;
    }

    int i;
    for (i=0; i<face_count; i++) {
        FaceSnap *snap = extra->snap + i;
        struct face_info *fi = &fie->group.face_info[i];

        snap->rect.left = fi->x;
        snap->rect.top = fi->y;
        snap->rect.right = fi->x + fi->w;
        snap->rect.bottom = fi->y + fi->h;
        snap->faceQuality = fi->quality;
        snap->trackId = fi->trackId;
    }

    return p;
}

static void _free_SnapEncPacket(SnapEncPacket *p)
{
    if (p->buffer)
        ipc_mem_free(gipc, 1, p->buffer);

    FaceSnapExtraData *extra = p->extra;
    if (extra) {
        int i;
        for (i=0; i<extra->snapCount; i++) {
            if (extra->snap[i].buffer)
                ipc_mem_free(gipc, 1, extra->snap[i].buffer);
        }
        ipc_mem_free(gipc, 1, extra);
    }
}

static void free_SnapEncPacket(SnapEncPacket *p)
{
    _free_SnapEncPacket(p);
    free(p);
}


int facedetecter_release_packet(struct facedetecter *fd, SnapEncPacket *packet)
{
    _free_SnapEncPacket(packet);
    return 0;
}

struct snap_packet {
    struct list_head entry;
    SnapEncPacket *p;
};

/////////////for isp/////////////////////////
static int isp_enable = 0;
static Queue *g_face_info_queue_isp = NULL;
static sem_t g_isp_sem;

void isp_sem_init()
{
    sem_init(&g_isp_sem, 0, 0);
}

void enable_face_info_isp(int enable)
{
    isp_enable = enable;
}

struct face_info_group* isp_get_face()
{
    if (g_face_info_queue_isp == NULL)
        sem_wait(&g_isp_sem);

    return &((list_entry(queue_pop(g_face_info_queue_isp), struct face_info_entry, entry))->group);
}

void isp_release_face(struct face_info_group* g)
{
    free(container_of(g, struct face_info_entry, group));
}
////////////////////////////////////////////

#ifdef HAVE_H264_SEI_FACE_INFO
static Queue *g_face_info_queue = NULL;

Queue* get_face_info_queue()
{
    return g_face_info_queue;
}
#endif

static int face_rect_zoom(MG_RECTANGLE *rc, int max_w, int max_h, int factor)
{
    if (rc->left < 0)
        rc->left = 0;
    if (rc->top < 0)
        rc->top = 0;
    if (rc->right > max_w)
        rc->right = max_w;
    if (rc->bottom > max_h)
        rc->bottom = max_h;

    int x = rc->left * factor;
    int y = rc->top * factor;
    int w = MIN((rc->right - rc->left) * factor, max_w - x);
    int h = MIN((rc->bottom - rc->top) * factor, max_h - y);

    rc->left = x;
    rc->top = y;
    rc->right = x + w;
    rc->bottom = y + h;

    /* jpegcl has a weird behaviour:
     * 1. the (left,top) must be align to 2
     * 2. when rect is out of bound, the width and height must be align to 16 pixel
     */
    if ((x + ALIGN_UP(w, 16)) > max_w) {
        w = ALIGN_UP(w, 16);
        rc->left = max_w - w;
        rc->right = max_w;
    }

    if ((y + ALIGN_UP(h, 16)) > max_h) {
        h = ALIGN_UP(h, 16);
        rc->top = max_h - h;
        rc->bottom = max_h;
    }

    return 0;
}

static double face_threshold(double value, double threshold)
{
    threshold = fmax(0.001, fmin(0.999, threshold));
    value = 1 - value;
    threshold = 1 - threshold;
    return fmin(1.0, (0.8/threshold-1)/(threshold-1)*value*value+(1-(0.8/threshold-1)/(threshold-1))*value);
}

static double face_threshold_cut(double value, double threshold, double zeroValue)
{
    return face_threshold(fmin(fabs(value)/zeroValue, 1.0), threshold/zeroValue);
}

static double face_quality(MG_FACEINFO *face_info)
{
    return pow(face_threshold_cut(face_info->gaussian, 0.1, 0.5)*
               face_threshold_cut(face_info->motion, 0.05, 0.5)*
               pow(face_threshold_cut(face_info->yaw, 0.17, 0.5), 3)*
               face_threshold_cut(face_info->pitch, 0.17, 0.5),
               1.0/6);
}

struct face_info_entry* face_info_entry_create(struct facedetecter *fd, uint64 sequence, unsigned short face_count, struct yuv_frame *yuv_frame)
{
    int i;
    struct face_info_entry *face_info_entry;
    MG_RETCODE ret_code;

    face_info_entry = malloc(sizeof(struct face_info_entry) + face_count*sizeof(struct face_info));
    face_info_entry->yuv_frame = yuv_frame;
    face_info_entry->group.sequence = sequence;
    face_info_entry->group.face_info_size = sizeof(struct face_info);
    face_info_entry->group.face_info_count = face_count;

    for (i = 0; i < face_count; i++) {
        if ((ret_code = CALL_MG_DT_GetFaceInfo(fd->fd_result, i, &fd->face_info)) != MG_RETCODE_OK) {
            VIDEO_ERROR("MG_DT_GetFaceInfo failed, ret_code = %d", ret_code);
            break;
        }

        VIDEO_DEBUG("[seq:%llu[%d]] (%d, %d), (%d, %d)", sequence, i,
                                                      fd->face_info.faceRectangle.left,
                                                      fd->face_info.faceRectangle.top,
                                                      fd->face_info.faceRectangle.right,
                                                      fd->face_info.faceRectangle.bottom);

        if (face_rect_zoom(&fd->face_info.faceRectangle, V4L2_FMT_WIDTH, V4L2_FMT_HEIGHT, 2) == -1) {
            VIDEO_WARN("rect zoom 2 failed: %d %d %d %d", fd->face_info.faceRectangle.left,
                                                          fd->face_info.faceRectangle.top,
                                                          fd->face_info.faceRectangle.right,
                                                          fd->face_info.faceRectangle.bottom);
            continue;
        }

        struct face_info *fi = &face_info_entry->group.face_info[i];
        fi->x = fd->face_info.faceRectangle.left;
        fi->y = fd->face_info.faceRectangle.top;
        fi->w = fd->face_info.faceRectangle.right - fd->face_info.faceRectangle.left;
        fi->h = fd->face_info.faceRectangle.bottom - fd->face_info.faceRectangle.top;
        fi->quality = face_quality(&fd->face_info);
        fi->trackId = fd->face_info.ID;

        VIDEO_DEBUG("[seq:%llu[%d]] after zomm: (%d, %d), (%d, %d)", sequence, i,
                                                      fd->face_info.faceRectangle.left,
                                                      fd->face_info.faceRectangle.top,
                                                      fd->face_info.faceRectangle.right,
                                                      fd->face_info.faceRectangle.bottom);
    }

    return face_info_entry;
}

void face_info_entry_destory(struct face_info_entry* face_info_entry)
{
    free(face_info_entry);
}

int facedetecter_get_packet(struct facedetecter *fd, SnapEncPacket **out)
{
    struct snap_packet *snap_packet = list_entry(queue_pop(&fd->face_queue), struct snap_packet, entry);
    *out = snap_packet->p;
    free(snap_packet);

    return LEN_PACKET;
}

#ifdef HAVE_H264_SEI_FACE_INFO
static int facedetector_queue_face_info(struct facedetecter *fd, int face_count, struct yuv_frame *yuv_frame)
{
    struct face_info_entry *face_info_entry;

    face_info_entry = face_info_entry_create(fd, yuv_frame->sequence, face_count, NULL);

    if (queue_push(&fd->face_info_queue, &face_info_entry->entry) == -1) {
        VIDEO_WARN("queue %llu face_info failed", yuv_frame->sequence);
        face_info_entry_destory(face_info_entry);
    }

    return 0;
}
#endif

static int facedetector_queue_face_info_isp(struct facedetecter *fd, int face_count, struct yuv_frame *yuv_frame)
{
    struct face_info_entry *face_info_entry;

    face_info_entry = face_info_entry_create(fd, yuv_frame->sequence, face_count, NULL);

    if (queue_push(&fd->face_info_queue_isp, &face_info_entry->entry) == -1) {
        VIDEO_WARN("queue %llu face_info failed", yuv_frame->sequence);
        face_info_entry_destory(face_info_entry);
    }

    return 0;
}

static int facedetecter_do_snap(struct facedetecter *fd, struct face_info_entry *fie)
{
    int face_count = fie->group.face_info_count;
    int i;
    SnapEncPacket *p;
    FaceSnapExtraData *extra;

    if (face_count == 0)
        return -1;

    p = alloc_SnapEncPacket(fie);
    if (!p)
        return -1;

    p->width  = fd->wq.yuv_w;
    p->height = fd->wq.yuv_h;

    extra = p->extra;
    if (extra) {
        for (i = 0; i < face_count; i++) {
            struct face_info *fi = &fie->group.face_info[i];

            if (fi->quality < fd->threshold)
                continue;

            JpegOutputBuffer image;
            JpegCropRect rc = {fi->x, fi->y, fi->w, fi->h};

            if (jpeg_encode(fie->yuv_frame, 95, &rc, &image) == 0) {
                FaceSnap *face_snap = &extra->snap[i];
                face_snap->length = image.output_size;
                face_snap->buffer = ipc_mem_alloc(gipc, 1, image.output_size);
                if (face_snap->buffer)
                        memcpy(face_snap->buffer, image.output_buffer, image.output_size);
                jpeg_encode_done(&image);
            } else {
                VIDEO_WARN("jpeg_encode x:%d y:%d w:%d h:%d failed", rc.x, rc.y, rc.w, rc.h);
            }
        }
    }

#if 1
#include "osd.h"
    unsigned char *base = (unsigned char *)xcam_buf_get_data(fie->yuv_frame->handle);
    struct surface s = { base, V4L2_FMT_WIDTH, V4L2_FMT_HEIGHT, 1 };

    for (i=0; i<face_count; i++) {
        struct face_info *fi = &fie->group.face_info[i];
        struct rect rc = {fi->x, fi->y, fi->w, fi->h};
        surface_draw_rect(&s, &rc);
    }
#endif

    JpegOutputBuffer image;
    JpegCropRect rc = {0, 0, V4L2_FMT_WIDTH, V4L2_FMT_HEIGHT};

    if (jpeg_encode(fie->yuv_frame, 80, &rc, &image) == 0) {
        p->length = image.output_size;
        p->buffer = ipc_mem_alloc(gipc, 1, image.output_size);
        if (p->buffer)
            memcpy(p->buffer, image.output_buffer, image.output_size);
        jpeg_encode_done(&image);

        struct snap_packet *snap_packet = calloc(1, sizeof(struct snap_packet));
        snap_packet->p = p;

        if (queue_push(&fd->face_queue, &snap_packet->entry) == -1) {
            VIDEO_WARN("queue face_queue failed");
            free_SnapEncPacket(p);
            free(snap_packet);
        }
    } else {
        VIDEO_WARN("jpeg_encode big image failed");
        free_SnapEncPacket(p);
    }

    yuv_frame_free(fie->yuv_frame);

    return 0;
}

static void* snap_thread(void *p)
{
    struct facedetecter *fd = p;
    struct face_info_entry *face_info_entry;

    while (1) {
        face_info_entry = list_entry(queue_pop(&fd->snap_queue), struct face_info_entry, entry);
        facedetecter_do_snap(fd, face_info_entry);
    }

    return NULL;
}

static int facedetecter_snap(struct facedetecter *fd, int face_count, struct yuv_frame *yuv_frame)
{
    struct face_info_entry *face_info_entry;

    if (face_count == 0)
        return -1;

    face_info_entry = face_info_entry_create(fd, yuv_frame->sequence, face_count, yuv_frame);

    if (queue_push(&fd->snap_queue, &face_info_entry->entry) == -1) {
       VIDEO_WARN("queue face_queue failed");
       face_info_entry_destory(face_info_entry);
    }

    return 0;
}

static int facedetecter_init_image(struct facedetecter *fd, struct yuv_frame *yuv_frame, MG_IMAGEWRAPPER *image)
{
    if (fd->rect.right == fd->rect.left || fd->rect.bottom == fd->rect.top) {
        image->grayBuffer = yuv_frame_get_data(yuv_frame);
        image->width = fd->yuv_w;
        image->height = fd->yuv_h;
        return 0;
    }

    int w = fd->rect.right - fd->rect.left;
    int h = fd->rect.bottom - fd->rect.top;
    image->grayBuffer = malloc(w*h);
    image->width = w;
    image->height = h;

    char *data = yuv_frame_get_data(yuv_frame);
    int i;
    for (i=0; i<h; i++) {
        char *p1 = (char*)image->grayBuffer + w*i;
        char *p2 = data + fd->yuv_w*(fd->rect.top+i) + fd->rect.left;
        memcpy(p1, p2, w);
    }

    return 1;
}

static int facedetecter_detect(struct yuv_wq *wq, struct yuv_frame *yuv_frame)
{
    struct facedetecter *fd = container_of(wq, struct facedetecter, wq);
    MG_IMAGEWRAPPER gray_image_info;
    MG_RETCODE ret_code;
    int face_count;

    int need_free = facedetecter_init_image(fd, yuv_frame, &gray_image_info);

    yuv_wq_can_queue(wq, 0);
    uint64 begin = now();
    if ((ret_code = CALL_MG_DT_DetectFace(fd->fd_handle, gray_image_info, fd->fd_result)) != MG_RETCODE_OK) {
        VIDEO_ERROR("MG_DT_DetectFace failed, ret_code = %d", ret_code);
        return -1;
    }
    uint64 end = now();
    yuv_wq_can_queue(wq, 1);

    if (need_free)
        free((char*)gray_image_info.grayBuffer);

    if ((ret_code = CALL_MG_DT_GetFaceCount(fd->fd_result, &face_count)) != MG_RETCODE_OK) {
        VIDEO_ERROR("MG_DT_GetFaceCount failed, ret_code = %d", ret_code);
        return -1;
    }

#ifdef HAVE_H264_SEI_FACE_INFO
    facedetector_queue_face_info(fd, face_count, yuv_frame);
#endif

    if (face_count) {
        VIDEO_INFO("==========================detect seq:%llu use %llums, has %d faces", yuv_frame->sequence, end - begin, face_count);
        if (isp_enable)
            facedetector_queue_face_info_isp(fd, face_count, yuv_frame);

        struct yuv_frame *new_yuv_frame = yuv_frame_alloc(yuv_frame->handle, NULL, 0, yuv_frame->sequence, yuv_frame->pts);
        if (facedetecter_snap(fd, face_count, new_yuv_frame) == -1)
            yuv_frame_free(new_yuv_frame);
    }

    return 0;
}

int facedetecter_init(struct facedetecter *fd)
{
    MG_RETCODE ret_code;

    memset(fd, 0, sizeof(struct facedetecter));

    MGOPS_init();

    fd->fd_handle = CALL_MG_DT_CreateHandle();
    if (fd->fd_handle == NULL) {
        VIDEO_ERROR("create face handle failed!");
        return -1;
    }

    fd->fd_result = CALL_MG_DT_CreateResult(&fd->face_info);
    if (fd->fd_result == NULL) {
        VIDEO_ERROR("create face result failed!");
        return -1;
    }

    if ((ret_code = CALL_MG_DT_SetMode(fd->fd_handle, MG_DETECTION_MODE_TRACKING_FAST)) != MG_RETCODE_OK) {
        VIDEO_ERROR("MG_DT_SetMode failed, ret_code = %d", ret_code);
        return -1;
    }

    int cpuids[4] = {-1, -1, -1, -1};
    if ((ret_code = CALL_MG_DT_SetTrackingWorkerCPUs(fd->fd_handle, ARRAY_SIZE(cpuids), cpuids)) != MG_RETCODE_OK) {
        VIDEO_ERROR("MG_DT_SetTrackingWorkerCPUs failed, ret_code = %d", ret_code);
        return -1;
    }

    fd->yuv_w = SNAP_YUV_W;
    fd->yuv_h = SNAP_YUV_H;

    queue_init(&fd->face_queue, 25);
    yuv_wq_init(&fd->wq, 8, facedetecter_detect, 0);
    fd->wq.need_xcam_buf = 1;

#if HAVE_H264_SEI_FACE_INFO
    assert(g_face_info_queue == NULL);
    queue_init(&fd->face_info_queue, 8);
    g_face_info_queue = &fd->face_info_queue;
#endif

    assert(g_face_info_queue_isp == NULL);
    queue_init(&fd->face_info_queue_isp, 25);
    g_face_info_queue_isp = &fd->face_info_queue_isp;
    sem_post(&g_isp_sem);

    queue_init(&fd->snap_queue, 0);
    pthread_t pthread;
    pthread_create(&pthread, NULL, snap_thread, fd);
    pthread_detach(pthread);

    return 0;
}

int facedetecter_deinit(struct facedetecter *fd)
{
    MG_RETCODE ret_code;

    if ((ret_code = CALL_MG_DT_ReleaseResult(fd->fd_result)) != MG_RETCODE_OK) {
        VIDEO_ERROR("MG_DT_ReleaseResult failed, ret_code = %d", ret_code);
        return -1;
    }

    if ((ret_code = CALL_MG_DT_ReleaseHandle(fd->fd_handle)) != MG_RETCODE_OK) {
        VIDEO_ERROR("MG_DT_ReleaseResult failed, ret_code = %d", ret_code);
        return -1;
    }

    return 0;
}

int facedetecter_start(struct facedetecter *fd)
{
    yuv_wq_start(&fd->wq, videoEncStreamNumber, fd->yuv_w, fd->yuv_h);
    return 0;
}

int facedetecter_stop(struct facedetecter *fd)
{
    yuv_wq_stop(&fd->wq);
    return 0;
}

static int facedetecter_set_yuv_size(struct facedetecter *fd, int width, int height)
{
    fd->yuv_w = width;
    fd->yuv_h = height;
    return 0;
}

static int facedetecter_set_face_size(struct facedetecter *fd, int min_size, int max_size)
{
    MG_RETCODE ret_code;

    if (min_size < 24)
        min_size = 24;

    if (min_size > max_size)
        max_size = fd->yuv_w;

    if ((ret_code = CALL_MG_DT_SetFaceSizeRange(fd->fd_handle, min_size, max_size)) != MG_RETCODE_OK) {
        VIDEO_ERROR("MG_DT_SetFaceSizeRange min:%d max:%d failed, ret_code = %d", min_size, max_size, ret_code);
        return -1;
    }

    return 0;
}

static int facedetecter_set_snap_rect(struct facedetecter *fd, Rect *rect)
{
    int w = rect->right - rect->left;
    int h = rect->bottom - rect->top;
    if (w <= 0 || h <= 0 || w > fd->yuv_w || h > fd->yuv_h)
        return -1;

    fd->rect.left = rect->left;
    fd->rect.right = rect->right;
    fd->rect.top = rect->top;
    fd->rect.bottom = rect->bottom;

    return 0;
}

static int facedetecter_set_threshold(struct facedetecter *fd, double threshold)
{
    fd->threshold = threshold;
    return 0;
}

int facedetecter_set_config(struct facedetecter *fd, SnapEncConfig *config)
{
    int ret = -1;

    switch (config->type) {
    case SnapConfigYUVSize:
        ret = facedetecter_set_yuv_size(fd, config->cfg.yuvSize.width, config->cfg.yuvSize.height);
        break;
    case SnapConfigSnapRect:
        ret = facedetecter_set_snap_rect(fd, &config->cfg.snapRect);
        break;
    case SnapConfigFaceSize:
        ret = facedetecter_set_face_size(fd, config->cfg.faceSize.min, config->cfg.faceSize.max);
        break;
    case SnapConfigThreshold:
        ret = facedetecter_set_threshold(fd, config->cfg.threshold);
        break;
    default:
        VIDEO_ERROR("unknown config: %d", config->type);
    }

    return ret;
}
