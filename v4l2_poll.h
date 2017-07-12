/* ===================================================
 * Copyright (C) 2015 chenshuangping All Right Reserved.
 *      Author: mincore@163.com
 *    Filename: v4l2_poll.h
 *     Created: 2015-07-13 07:32
 * Description:
 * ===================================================
 */
#ifndef V4L2_POLL_H
#define V4L2_POLL_H

#define V4L2_FMT_FPS 25
#define V4L2_FMT_WIDTH 1920
#define V4L2_FMT_HEIGHT 1080

#define V4L2_CAPTURE_MODE 0x4000
#define V4L2_BUFFER_COUNT 25

int v4l2_poll_init();
int v4l2_poll_deinit();

int v4l2_poll_start(int w, int h, int fps);
int v4l2_poll_stop();

unsigned long long v4l2_poll_first_pts();


#endif
