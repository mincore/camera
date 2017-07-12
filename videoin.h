/* ===================================================
 * Copyright (C) 2015 chenshuangping All Right Reserved.
 *      Author: mincore@163.com
 *    Filename: videoin.h
 *     Created: 2015-07-13 07:32
 * Description:
 * ===================================================
 */
#ifndef _VIDEOIN_H
#define _VIDEOIN_H

#include <api/VideoIn.h>

int vin_set_color(struct VideoInput *thiz, VideoColor *param);
int vin_set_exposurer(struct VideoInput *thiz, VideoExposure *param);
int vin_set_lightregulation(struct VideoInput *thiz, VideoLightRegulation *param);
int vin_set_whitebalance(struct VideoInput *thiz, VideoWhiteBalance *param);
int vin_set_imagenhance(struct VideoInput *thiz, VideoImageEnhancement *param);
int vin_set_mirror(struct VideoInput *thiz, VideoMirror *param);
int vin_set_standard(struct VideoInput *thiz, VideoStandard *param);
int vin_set_daynight(struct VideoInput *thiz, VideoDayNight *param);
int vin_set_backgroundcolor(struct VideoInput *thiz, Color *color);
int vin_start(struct VideoInput *thiz);
int vin_stop(struct VideoInput *thiz);

#endif
