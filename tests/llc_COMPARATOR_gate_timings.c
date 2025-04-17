#include <stdio.h>
#include <stdlib.h>

#include "slice_partitioning/comparator_gate.h"
#include "slice_partitioning/slicing.h"

#define NUM_SAMPLES 10000

int main()
{
    size_t len = LLC_SLICES * MB;
    uint8_t *mem = (uint8_t *)initialise_memory(len, 0, PAGE_SIZE);
    init_measuring_sticks_cheat();

    for (int t = 0; t < 1; ++t)
    {
        for (int s = 0; s < LLC_SLICES; s++)
        {
            size_t gate_output_totals[LLC_SLICES] = {0};
            for (int samples = 0; samples < NUM_SAMPLES; samples++)
            {
                for (uintptr_t i = 0; i < len; i += CACHELINE)
                {
                    void *address = (void *)((uintptr_t)mem + i);
                    if (get_address_slice(virtual_to_physical((uint64_t)address)) == s)
                    {
                        int gate_output[LLC_SLICES] = {0};
                        comparator_gate(address, (int *)gate_output);
                        for (int o = 0; o < LLC_SLICES; ++o)
                        {
                            gate_output_totals[o] += gate_output[o];
                        }
                        break;
                    }
                }
            }

            for (int o = 0; o < LLC_SLICES; ++o)
            {
                printf("%d:%d | %2.3f\n", s, o, ((double)gate_output_totals[o] / (double)(NUM_SAMPLES)));
            }
        }
        putchar('\n');
        sleep(1);
    }

    munmap(mem, len);
    return 0;
}