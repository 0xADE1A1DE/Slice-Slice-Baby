#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "evsets/evsets.h"
#include "slice_partitioning/evsp.h"
#include "util/helper_thread.h"

#define SAMPLES 10000

typedef struct time_result_s
{
    int low;
    int mid;
    int high;
    uint32_t total_time;
    uint32_t times[SAMPLES];
} time_result_t;

uint32_t get_access_time(void *address, address_state cache, uint32_t bound_low, uint32_t bound_high, time_result_t *results)
{
    results->low = 0;
    results->mid = 0;
    results->high = 0;
    results->total_time = 0;
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
            results->times[i] = temp;
            results->total_time += temp;
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
    time = results->total_time;
    return time;
}

int main()
{
    size_t len = LLC_SIZE * 4;
    void *mem = (void *)initialise_memory(len, 0, PAGE_SIZE);

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
            time_result_t *results = calloc(1, sizeof(time_result_t));
            get_access_time(addr, LLC, 0, 100, results);
            for (int s = 0; s < SAMPLES; s++)
            {
                printf("%d,%d\n", i, results->times[s]);
            }
            free(results);
            i++;
        }
    }

    return 0;
}
