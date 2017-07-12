/* ===================================================
 * Copyright (C) 2015 chenshuangping All Right Reserved.
 *      Author: mincore@163.com
 *    Filename: h264.h
 *     Created: 2015-07-13 07:30
 * Description:
 * ===================================================
 */
#ifndef _VIDEO_H264_H
#define _VIDEO_H264_H

/**
 * h264_clone_and_insert_user_data - copy h264 into a new buffer and insert user data
 *
 * @h264_data: input h264_data, will not be touched.
 * @h264_data_len: input h264_data_len
 * @user_data: input user_data
 * @user_data_len: input user_data_len
 * @new_h264_data_len: output new_h264_data_len
 *
 * The returned memory is alloced by this function and must be freed by the caller.
 * If user_data is NULL or user_data_len <= 0, then this function just copy h264_data
 *     into a new alloced buffer. In this case, *new_h264_data_len == h264_data_len
 * If insert failed, the result is same as above.
 */
void *h264_clone_and_insert_user_data(
            void *h264_data, int h264_data_len,
            void *user_data, int user_data_len,
            int *new_h264_data_len);

#endif
