#include <stdio.h>
#include <stdlib.h>

#include "evsets/evsets.h"
#include "slice_partitioning/evsp.h"
#include "util/helper_thread.h"

#define SAMPLES 100

typedef struct time_result_s
{
    int low;
    int mid;
    int high;
} time_result_t;

static ht_ctrl_t *ht;

uint32_t get_access_time(void *address, address_state cache, uint32_t bound_low, uint32_t bound_high, time_result_t *results)
{
    results->low = 0;
    results->mid = 0;
    results->high = 0;
    uint32_t time = 0;

    // Warmup, and initialise evsets if not already done.
    set_addr_state((uintptr_t)address, cache);
    set_addr_state((uintptr_t)address, LLC);

    for (int i = 0; i < SAMPLES; ++i)
    {
        memaccess(address);
        memory_fences();
        set_addr_state((uintptr_t)address, cache);
        // Page walk i.e. put address of page into TLB
        memaccess((void *)((uintptr_t)address ^ 0x800)); 
        uint32_t temp = memaccesstime(address);
        if (temp > 1000)
        {
            i--;
        }
        else
        {
            time += temp;
            if (temp > bound_high)
            {
                results->high++;
            }
            else if (temp < bound_low)
            {
                results->low++;
            }
            else
            {
                results->mid++;
            }
        }
    }
    return time;
}

int main()
{
    size_t len = LLC_SIZE * 4;
    void *mem = (void *)initialise_memory(len, 0, PAGE_SIZE);

    time_result_t *results = calloc(1, sizeof(time_result_t));

    uint32_t time = get_access_time(mem, L1, 0, 35, results);
    printf("%15s : %3d | %3d | %3d\t%.3f\n", get_address_state_string(L1), results->low, results->mid, results->high, (double)time / SAMPLES);

    time = get_access_time(mem, L2, 35, 40, results);
    printf("%15s : %3d | %3d | %3d\t%.3f\n", get_address_state_string(L2), results->low, results->mid, results->high, (double)time / SAMPLES);

    uintptr_t o = 0;
    for (int i = 0; i < LLC_SLICES;)
    {
        void *addr = (void *)((uintptr_t)mem + o);
        if (get_address_slice(virtual_to_physical((uint64_t)addr)) != i)
        {
            o += CACHELINE;
        }
        else
        {
            time = get_access_time(addr, LLC, LLC_BOUND_LOW, LLC_BOUND_HIGH, results);
            printf("%15s/%d : %3d | %3d | %3d\t%.3f\n", get_address_state_string(LLC), i, results->low, results->mid, results->high, (double)time / SAMPLES);
            i++;
        }
    }

    time = get_access_time(mem, RAM_SMALLPAGE, 35, 90, results);
    printf("%15s : %3d | %3d | %3d\t%.3f\n", get_address_state_string(RAM_SMALLPAGE), results->low, results->mid, results->high, (double)time / SAMPLES);

    // ht = calloc(1, sizeof(ht_ctrl_t));
    // ht->affinity = 6;
    // ht->running = 0;
    // ht_start(ht);
    // for (int i = 0; i < 16; ++i)
    // {
    // 	time = get_access_time((void *)(mem + (i * CACHELINE)), RAM_SMALLPAGE, 40, 90, results);
    // 	printf("%15s : %3d | %3d | %3d\t%.3f\n", get_address_state_string(RAM_SMALLPAGE), results->low, results->mid, results->high, (double)time / SAMPLES);
    // }
    // ht_stop(ht);
    // free(ht);

    free(results);
    return 0;
}
