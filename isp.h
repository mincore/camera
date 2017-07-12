/* ===================================================
 * Copyright (C) 2015 chenshuangping All Right Reserved.
 *      Author: mincore@163.com
 *    Filename: isp.h
 *     Created: 2015-07-13 07:31
 * Description:
 * ===================================================
 */
#ifndef __ISP_PROCESS_H__
#define __ISP_PROCESS_H__

#define MAX_FACE_BACKLIGHT_SUPPORT (6)

typedef struct IspFaceBlackLight {
    int faceNum;
    struct {
        Rect            rect[MAX_FACE_BACKLIGHT_SUPPORT];    ///< 背光补偿自定义模式下的区域
    }face;
    int                    reserved[10];                    ///< 保留字节
} IspFaceBlackLight;


typedef struct IspBacklightInfo {
    VideoBackLightMode backlightMode;
    uint                enable;
    Rect                customWindow;
}IspBacklightInfo;

void isp_thread_create();

extern IspBacklightInfo gBackLightInfo;

#endif
