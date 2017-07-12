/* ===================================================
 * Copyright (C) 2015 chenshuangping All Right Reserved.
 *      Author: mincore@163.com
 *    Filename: videoin.c
 *     Created: 2015-07-13 07:32
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

#include "videoin.h"

int vin_set_color(struct VideoInput *thiz, VideoColor *param)
{
    double brightness = 0;
    double contrast = 0;
    double hue = 0;
    double saturation = 0;
    double sharpness = 0;

    if(NULL == param)
    {
      VIDEO_INFO("vin_set_color capality is null\n");
      return -1;
    }
    brightness = MIN(0.999, (param->brightness - 50.0)/50.0);
    contrast = MIN(0.999, (param->contrast - 50.0)/50.0);
    hue = MIN(0.999, (50.0 - 50.0)/50.0);
    saturation = MIN(0.999, (param->saturation - 50.0)/50.0);
    sharpness = MIN(0.999, (param->sharpness- 50.0)/50.0);

    VIDEO_INFO("brightness=%f,contrast=%f,hue=%f,saturation=%f\n",brightness,contrast,hue,saturation);
    xcam_x3a_set_manual_brightness(brightness);
    xcam_x3a_set_manual_contrast(contrast);
    xcam_x3a_set_manual_hue(hue);
    xcam_x3a_set_manual_saturation(saturation);
    xcam_x3a_set_manual_sharpness(sharpness);
    return 0;
}

int vin_set_exposurer(struct VideoInput *thiz, VideoExposure *param)
{
    VideoExposureMode exposure_mode = VideoExposureModeDefaultAuto;
    double auto_gain = 0;
    long long int auto_maxshutter = 0;
    long long int auto_minshutter = 0;
    long long int lown_maxshutter = 0;
    long long int lown_minshutter = 0;
    double lows_gain = 0;

    exposure_mode = param->exposureMode;
    auto_gain = (double)param->detail.autoMode.gainMax;
    auto_maxshutter = (long long int)param->detail.autoMode.shutterMax;
    auto_minshutter = 99;
    lown_maxshutter = (long long int)param->detail.lowSmear.shutterMax;
    lown_minshutter = 99;
    lows_gain = param->detail.lowNoise.gainMax;
    xcam_x3a_set_ae_mode (VideoExposureModeDefaultAuto);
    auto_gain = 1.0 + (auto_gain * 250) / 100.0;
    lows_gain = 1.0 + (lows_gain * 250) / 100.0;

    switch (exposure_mode)
    {
    case VideoExposureModeDefaultAuto:
          xcam_x3a_set_ae_max_analog_gain(auto_gain);
          xcam_x3a_set_ae_exposure_time_range(auto_minshutter, auto_maxshutter);
        break;
    case VideoExposureModeLowNoise:
          xcam_x3a_set_ae_max_analog_gain(lows_gain);
          xcam_x3a_set_ae_exposure_time_range(99, 40000);
          break;
    case VideoExposureModeLowSmear:
          xcam_x3a_set_ae_max_analog_gain(251);
          xcam_x3a_set_ae_exposure_time_range(lown_minshutter, lown_maxshutter);
          break;
    default:
          VIDEO_INFO("[ISP]ae settting mode not supprotted\n");
    }


    VIDEO_INFO("exposure_mode==%d\n",exposure_mode);
    VIDEO_INFO("auto_gain==%f\n",auto_gain);
    VIDEO_INFO("auto_minshutter==%lld\n",auto_minshutter);
    VIDEO_INFO("auto_maxshutter==%lld\n",auto_maxshutter);
    VIDEO_INFO("lows_gain==%f\n",lows_gain);
    VIDEO_INFO("lown_minshutter==%lld\n",lown_minshutter);
    VIDEO_INFO("lown_maxshutter==%lld\n",lown_maxshutter);

    return 0;
}

int vin_set_lightregulation(struct VideoInput *thiz, VideoLightRegulation *param)
{
    gBackLightInfo.backlightMode     = param->backlight.mode;
    gBackLightInfo.enable             = param->backlight.enable;
    gBackLightInfo.customWindow.left =  param->backlight.detail.custom.rect.left;
    gBackLightInfo.customWindow.top =  param->backlight.detail.custom.rect.top;
    gBackLightInfo.customWindow.right =  param->backlight.detail.custom.rect.right;
    gBackLightInfo.customWindow.bottom =  param->backlight.detail.custom.rect.bottom;

    return 0;
}

int vin_set_whitebalance(struct VideoInput *thiz, VideoWhiteBalance *param)
{
    int wb_mode = 0;
    double r_gain = 0;
    double b_gain = 0;

    if(WhiteBalanceManual == param->whiteBalance || WhiteBalanceCustom == param->whiteBalance )
    {
        wb_mode = XCAM_AWB_MODE_MANUAL;
    }
    else if(WhiteBalanceAuto == param->whiteBalance)
    {
        wb_mode = XCAM_AWB_MODE_AUTO;
    }
    else if(WhiteBalanceNight == param->whiteBalance)
    {
        wb_mode = XCAM_AWB_MODE_DAYLIGHT;
    }
    else if(WhiteBalanceSunny == param->whiteBalance)
    {
        wb_mode = XCAM_AWB_MODE_SUNSET;
    }
    else if(WhiteBalanceCloudy == param->whiteBalance)
    {
        wb_mode = XCAM_AWB_MODE_CLOUDY;
    }
    else
    {

    }
    xcam_x3a_set_awb_mode(wb_mode);
    if(XCAM_AWB_MODE_MANUAL == wb_mode)
    {
        r_gain = 1.0*param->detail.custom.gainRed/255.0;
        b_gain = 1.0*param->detail.custom.gainBlue/255.0;
        xcam_x3a_set_awb_manual_gain(0.5,r_gain,b_gain,0.5);
    }

    return 0;
}

int vin_set_imagenhance(struct VideoInput *thiz, VideoImageEnhancement *param)
{
    int enble = 0;
    double noise_level = 0;

    //mode = param->denoise.mode;

    enble = param->denoise.enable;
    if(0 == enble)
    {
        noise_level = -1;
    }
    else if(1 == enble)
    {
        noise_level = (param->denoise.detail.autoType.level - 50)/ 50.0;
    }

    //xcam_x3a_set_noise_reduction_level(noise_level);
    xcam_x3a_set_temporal_noise_reduction_level(noise_level);

    VIDEO_INFO("denosie enable = %d\n",enble);
    VIDEO_INFO("denosie level in = %u\n", param->denoise.detail.autoType.level);
    VIDEO_INFO("denosie level out = %f\n", noise_level);

    return 0;
}

int vin_set_mirror(struct VideoInput *thiz, VideoMirror *param)
{
    return 0;
}

int vin_set_standard(struct VideoInput *thiz, VideoStandard *param)
{
    return 0;
}

int vin_set_daynight(struct VideoInput *thiz, VideoDayNight *param)
{
    xcam_x3a_set_night_mode(VideoDayNightModeNight == param->mode);
    return 0;
}

int vin_set_backgroundcolor(struct VideoInput *thiz, Color *color)
{
    return 0;
}
