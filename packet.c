/* ===================================================
 * Copyright (C) 2015 chenshuangping All Right Reserved.
 *      Author: mincore@163.com
 *    Filename: packet.c
 *     Created: 2015-07-13 07:31
 * Description:
 * ===================================================
 */
#include <stdlib.h>
#include <string.h>
#include "packet.h"

int packet_init(struct packet *packet, void *data, int size, uint64 sequence, uint64 pts, int type)
{
    INIT_LIST_HEAD(&packet->entry);
    packet->sequence = sequence;
    packet->pts = pts;
    packet->type = type;
    packet->data = data;
    packet->data_len = size;
    return 0;
}

