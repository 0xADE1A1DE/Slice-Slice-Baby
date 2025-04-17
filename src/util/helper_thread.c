// Heavily influenced by https://github.com/zzrcxb/LLCFeasible, it works!
#include "util/helper_thread.h"

void *ht_worker(void *args)
{
    ht_ctrl_t *ctrl = args;
    while (1)
    {
        ctrl->waiting = 1;
        while (ctrl->waiting)
            ;
        switch (ctrl->action)
        {
        case HELPER_STOP: {
            return NULL;
        }
        case READ_SINGLE: {
            memaccess(ctrl->payload);
            break;
        }
        case WRITE_SINGLE: {
            ((elem_t *)(ctrl->payload))->pad[0]++;
            break;
        }
        case TIME_SINGLE: {
            memaccess((void *)((uintptr_t)ctrl->payload ^ 0x800));
            ctrl->result = memaccesstime(ctrl->payload);
            break;
        }
        case READ_ARRAY: {
            //     struct ht_read_array *arr = ctrl->payload;

            //     prime_cands_daniel(arr->addrs, arr->cnt, arr->repeat,
            //                        arr->stride, arr->block);
            fprintf(stderr, "ht_worker(): READ_ARRAY not implemented.\n");
            break;
        }
        case TRAVERSE_ARRAY: {
            evset_t *evset = ctrl->payload;
            LLC_TRAVERSE(evset->cs, evset->size, LLC_TRAVERSE_REPEATS);
            break;
        }
        case TRAVERSE_ARRAY_WITH_L2: {
            evset_t *llc_evset = ctrl->payload;
            evset_t *l2_evset = ctrl->payload2;
            skx_sf_cands_traverse_st(llc_evset->cs, llc_evset->size, l2_evset);
            break;
        }
        }
    }
}

void ht_wait(ht_ctrl_t *ctrl)
{
    while (!ctrl->waiting)
        ;
}

int ht_start(ht_ctrl_t *ctrl)
{
    ctrl->waiting = 0;
    memory_fences();

    pthread_attr_t attr;
    cpu_set_t cpuset;

    // Initialise thread attributes
    if (pthread_attr_init(&attr) != 0)
    {
        perror("Failed to initialise thread attributes");
        exit(1);
    }

    // Set CPU affinity to the specified processor
    CPU_ZERO(&cpuset);
    CPU_SET(ctrl->affinity, &cpuset);
    if (pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpuset) != 0)
    {
        perror("Failed to set CPU affinity");
        exit(1);
    }

    if (pthread_create(&ctrl->pid, &attr, ht_worker, ctrl))
    {
        perror("Failed to start the helper thread!");
        return 1;
    }
    ht_wait(ctrl);
    ctrl->running = 1;
    return 0;
}

void ht_stop(ht_ctrl_t *ctrl)
{
    if (ctrl->running)
    {
        ctrl->action = HELPER_STOP;
        memory_fences();
        ctrl->waiting = 0;
        pthread_join(ctrl->pid, NULL);
        ctrl->running = 0;
    }
}

void ht_read_single(ht_ctrl_t *ctrl, void *target)
{
    assert(ctrl->running);
    ctrl->action = READ_SINGLE;
    ctrl->payload = target;
    memory_fences();
    ctrl->waiting = 0;
    ht_wait(ctrl);
}

void ht_write_single(ht_ctrl_t *ctrl, void *target)
{
    assert(ctrl->running);
    ctrl->action = WRITE_SINGLE;
    ctrl->payload = target;
    memory_fences();
    ctrl->waiting = 0;
    ht_wait(ctrl);
}

uint32_t ht_time_single(ht_ctrl_t *ctrl, void *target)
{
    assert(ctrl->running);
    ctrl->action = TIME_SINGLE;
    ctrl->payload = target;
    memory_fences();
    ctrl->waiting = 0;
    ht_wait(ctrl);
    memory_fences();
    return ctrl->result;
}

void ht_traverse_array(ht_ctrl_t *ctrl, evset_t *evset)
{
    assert(ctrl->running);
    ctrl->action = TRAVERSE_ARRAY;
    ctrl->payload = (void *)evset;
    memory_fences();
    ctrl->waiting = 0;
    ht_wait(ctrl);
}

void ht_traverse_array_with_l2(ht_ctrl_t *ctrl, evset_t *llc_evset, evset_t *l2_evset)
{
    assert(ctrl->running);
    ctrl->action = TRAVERSE_ARRAY_WITH_L2;
    ctrl->payload = (void *)llc_evset;
    ctrl->payload2 = (void *)l2_evset;
    memory_fences();
    ctrl->waiting = 0;
    ht_wait(ctrl);
}
