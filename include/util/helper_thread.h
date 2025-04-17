// Heavily influenced by https://github.com/zzrcxb/LLCFeasible, it works!

#ifndef __HELPER_THREAD_H
#define __HELPER_THREAD_H

#define _GNU_SOURCE
#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "evsets/evsets_helpers.h"
#include "util.h"

typedef enum
{
    HELPER_STOP,
    READ_SINGLE,
    WRITE_SINGLE,
    TIME_SINGLE,
    READ_ARRAY,
    TRAVERSE_ARRAY,
    TRAVERSE_ARRAY_WITH_L2
} ht_action;

typedef struct
{
    int running;
    int affinity;
    volatile int waiting;
    volatile ht_action action;
    void *volatile payload;
    void *volatile payload2;
    uint32_t result;
    pthread_t pid;
} ht_ctrl_t;

void *ht_worker(void *args);
void ht_wait(ht_ctrl_t *ctrl);
int ht_start(ht_ctrl_t *ctrl);
void ht_stop(ht_ctrl_t *ctrl);
void ht_read_single(ht_ctrl_t *ctrl, void *target);
void ht_write_single(ht_ctrl_t *ctrl, void *target);
uint32_t ht_time_single(ht_ctrl_t *ctrl, void *target);
void ht_traverse_array(ht_ctrl_t *ctrl, evset_t *evset);
void ht_traverse_array_with_l2(ht_ctrl_t *ctrl, evset_t *llc_evset, evset_t *l2_evset);

// struct ht_read_array
// {
//     uint8_t ** volatile addrs;
//     volatile size_t cnt, repeat, stride, block;
//     volatile int bwd;
// };

// struct _evtest_config;

// struct ht_traverse_cands
// {
//     void (*volatile traverse)(uint8_t **cands, size_t cnt, struct _evtest_config *c);
//     uint8_t ** volatile cands;
//     size_t volatile cnt;
//     struct _evtest_config * volatile tconfig;
// };

#endif //__HELPER_THREAD_H