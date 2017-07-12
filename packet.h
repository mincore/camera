/* ===================================================
 * Copyright (C) 2015 chenshuangping All Right Reserved.
 *      Author: mincore@163.com
 *    Filename: packet.h
 *     Created: 2015-07-13 07:31
 * Description:
 * ===================================================
 */
#ifndef _VIDEO_PACKET_H
#define _VIDEO_PACKET_H

#include <api/Types.h>
#include "list.h"

struct packet {
    struct list_head entry;
    void *data;
    int data_len;
    int type;
    uint64 sequence;
    uint64 pts;
};

int packet_init(struct packet *packet, void *data, int size, uint64 sequence, uint64 pts, int type);

#endif
