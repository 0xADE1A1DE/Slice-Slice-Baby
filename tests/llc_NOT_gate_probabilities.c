#include <stdio.h>
#include <stdlib.h>

#include "evsets/evsets.h"
#include "slice_partitioning/evsp.h"
#include "slice_partitioning/slicing.h"

#include "slice_partitioning/modified_NOT_gate.h"

#define SAMPLES 100000

typedef struct results_s
{
    uint32_t data[ROB_SIZE];
} results_t;

results_t get_address_NOT_gate_results(void *input)
{
    results_t results;

    for (int i = 0; i < ROB_SIZE; ++i)
    {
        results.data[i] = 0;
        void *output = (void *)((uintptr_t)input ^ 0x400);
        for (int s = 0; s < SAMPLES; s++)
        {
            uint16_t temp = modified_NOT_gate_raw(input, output, i);
            results.data[i] += temp;
        }
    }

    return results;
}

int main()
{
    size_t len = LLC_SIZE * 4;
    void *mem = (void *)initialise_memory(len, 0, PAGE_SIZE);

    for (int s = 0; s < LLC_SLICES; s++)
    {
        for (uintptr_t i = 0; i < len; i++)
        {
            void *addr = (void *)((uintptr_t)mem + i);
            int slice = get_address_slice(virtual_to_physical((uint64_t)addr));

            if (slice == s)
            {
                results_t results = get_address_NOT_gate_results(addr);
                break;
            }
        }
    }

    for (int t = 0; t < 1; t++)
    {
        for (int s = 0; s < LLC_SLICES; s++)
        {
            for (uintptr_t i = (t * 4 * KB); i < len; i += 0x40)
            {
                void *addr = (void *)((uintptr_t)mem + i);
                int slice = get_address_slice(virtual_to_physical((uint64_t)addr));

                if (slice == s)
                {
                    set_addr_state((uintptr_t)addr, LLC);
                    memory_fences();

                    results_t results = get_address_NOT_gate_results(addr);

                    for (int r = 0; r < ROB_SIZE; ++r)
                    {
                        printf("%d, %d, %.3f\n", s, r, ((double)results.data[r] / (double)SAMPLES) * 100.0);
                    }

                    break;
                }
            }
        }
    }

    return 0;
}
