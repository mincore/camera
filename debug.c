/* ===================================================
 * Copyright (C) 2015 chenshuangping All Right Reserved.
 *      Author: mincore@163.com
 *    Filename: debug.c
 *     Created: 2015-07-13 07:29
 * Description:
 * ===================================================
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>

#define DEBUG_PIPE "/tmp/vdbgs"

static int _debug_time = 0;

static int strargv(char *str, const char *split, char **argv, int size)
{
    char *token, *saveptr;
    int argc;

    for (argc = 0; argc < size; str = NULL) {
        token = strtok_r(str, split, &saveptr);
        if (!token)
            break;
        argv[argc++] = token;
    }

    return argc;
}

static void* debug_loop(void *arg)
{
    char buf[128];
    char *argv[16];
    int argc;

    if (mkfifo(DEBUG_PIPE, S_IRUSR | S_IWUSR | S_IWGRP) == -1 && errno != EEXIST) {
        return NULL;
    }

    FILE *fp = fopen(DEBUG_PIPE, "r");
    if (!fp) {
        return NULL;
    }

    while (fgets(buf, 128, fp)) {
        argc = strargv(buf, " ", argv, 16);
        if (argc == 0)
            continue;

        if (argc == 2 && strcmp(argv[0], "debug_time") == 0) {
            _debug_time = atoi(argv[1]);
            continue;
        }
    }

    return NULL;
}

int debug_init()
{
    pthread_t pthread;
    pthread_create(&pthread, NULL, debug_loop, NULL);
    pthread_detach(pthread);

    return 0;
}

int debug_time()
{
    return _debug_time;
}
