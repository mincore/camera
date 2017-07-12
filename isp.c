/* ===================================================
 * Copyright (C) 2015 chenshuangping All Right Reserved.
 *      Author: mincore@163.com
 *    Filename: isp.c
 *     Created: 2015-07-13 07:31
 * Description:
 * ===================================================
 */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

#include "internal.h"
#include "utils.h"
#include "xcam_def.h"
#include "xcam_x3a.h"
#include "IspProcess.h"
#include "face.h"


static int isp_face_backlight_task(IspFaceBlackLight *param)
{
    XCam3AWindow window[MAX_FACE_BACKLIGHT_SUPPORT];
    IspBacklightInfo backLightInfo;
    int i = 0;
    if(NULL == param)
    {
        return -1;
    }

    int validFaceNum = MIN(param->faceNum, MAX_FACE_BACKLIGHT_SUPPORT);
    memset(window, 0, sizeof(window));
    memcpy(&backLightInfo, &gBackLightInfo, sizeof(IspBacklightInfo));

    VIDEO_INFO("[ISP]face backlight status.....enable:%d, mode:%d, faceNum:%d", 
                                                                backLightInfo.enable, 
                                                                backLightInfo.backlightMode,
                                                                validFaceNum);

    // 满足条件人脸重点曝光条件时
    if (backLightInfo.enable == 1)
    {
        switch (backLightInfo.backlightMode)
        {
            case VideoBackLightModeTop:
                window[0].weight = 15;
                window[0].x_start = 0;
                window[0].y_start = 0;
                window[0].x_end = 1920;
                window[0].y_end = 540;
                validFaceNum = 1;
                VIDEO_INFO("[ISP]setting top backlight");
                break;
            case VideoBackLightModeDown:
                window[0].weight = 15;
                window[0].x_start = 0;
                window[0].y_start = 540;
                window[0].x_end = 1920;
                window[0].y_end = 1080;
                validFaceNum = 1;
                VIDEO_INFO("[ISP]setting down backlight");
                break;
            case VideoBackLightModeLeft:
                window[0].weight = 15;
                window[0].x_start = 0;
                window[0].y_start = 0;
                window[0].x_end = 960;
                window[0].y_end = 1080;
                validFaceNum = 1;
                VIDEO_INFO("[ISP]setting left backlight");
                break;
            case VideoBackLightModeRight:
                window[0].weight = 15;
                window[0].x_start = 960;
                window[0].y_start = 0;
                window[0].x_end = 1920;
                window[0].y_end = 1080;
                //validFaceNum = 1;
                VIDEO_INFO("[ISP]setting right backlight");
                //break;
            case VideoBackLightModeCustom:
                window[0].weight = 15;
                window[0].x_start = backLightInfo.customWindow.left;
                window[0].y_start = backLightInfo.customWindow.top;
                window[0].x_end = backLightInfo.customWindow.right;
                window[0].y_end = backLightInfo.customWindow.bottom;
                //validFaceNum = 1;
                //break;
            case VideoBackLightModeFace:
                printf("[ISP]face num = %d\n", validFaceNum);
                window[0].weight = 0;
                window[0].x_start =  0;
                window[0].y_start =  0;
                window[0].x_end =    1920;
                window[0].y_end =    1080;
                for (i = 0; i < validFaceNum; i ++)
                {
                    window[i].weight  = 15;
                    window[i].x_start =  param->face.rect[i].left;
                    window[i].y_start =  param->face.rect[i].top;
                    window[i].x_end   =  param->face.rect[i].right;
                    window[i].y_end   =  param->face.rect[i].bottom;
                    
                    VIDEO_INFO("[ISP]setting face backlight.....%d, %d, %d, %d", window[i].x_start,
                                                                        window[i].y_start,
                                                                        window[i].x_end,
                                                                        window[i].y_end);

                }
                if (validFaceNum == 0)
                {
                    validFaceNum = 1;
                }
                break;
            default:
                VIDEO_INFO("[ISP]wrong setting param for backlight");
                break;
        }
    }
    else
    {
        window[0].weight = 0;
        window[0].x_start =  0;
        window[0].y_start =  0;
        window[0].x_end =    1920;
        window[0].y_end =    1080;
        validFaceNum = 1;
        VIDEO_INFO("[ISP]close backlight");
    }

    // 设置曝光
    xcam_x3a_set_ae_mode(XCAM_AE_MODE_AUTO);
    xcam_x3a_set_ae_metering_mode(XCAM_AE_METERING_MODE_WEIGHTED_WINDOW);
    xcam_x3a_set_ae_window(window, validFaceNum);
    xcam_x3a_set_ae_speed(0.8);

    return 0;
}


static void back_light_thread(void *p)
{
    IspFaceBlackLight param;
    struct face_info_group *g = NULL;
    int i = 0;
    
    memset(&param, 0, sizeof(IspFaceBlackLight));
    enable_face_info_isp(1);
    
    while (1) 
    {
        //获取face坐标和数量信息存储在IspFaceBlackLight param中
        g = isp_get_face();
        
        param.faceNum = MIN(MAX_FACE_BACKLIGHT_SUPPORT, g->face_info_count);
        for (i = 0; i < param.faceNum; i ++)
        {
            param.face.rect[i].left     = g->face_info[i].x;
            param.face.rect[i].top         = g->face_info[i].y;
            param.face.rect[i].right     = g->face_info[i].w + g->face_info[i].x;
            param.face.rect[i].bottom     = g->face_info[i].h + g->face_info[i].y;
            
            //printf("[ISP]loop1\n");
        }
        isp_release_face(g);
        
        // 设置face信息到backlight中
        isp_face_backlight_task(&param);

        usleep(10 * 1000);
    }
}



void isp_thread_create()
{
    pthread_t pthread;
    pthread_create(&pthread, NULL, (void*)back_light_thread, NULL);
}

