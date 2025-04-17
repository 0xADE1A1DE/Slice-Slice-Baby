#include <stdio.h>
#include <stdlib.h>

#include "evsets/evsets.h"
#include "slice_partitioning/evsp.h"
#include "slice_partitioning/slicing.h"

#define SAMPLES 10

typedef struct results_s
{
    double average;
    double stddev;
} results_t;

results_t get_address_time(void *addr)
{
    uint32_t *data = calloc(SAMPLES, sizeof(int));
    for (int i = 0; i < SAMPLES; ++i)
    {
        tlb_warmup_ptr((elem_t *)addr);
        set_addr_state((uintptr_t)addr, LLC);
        tlb_warmup_ptr((elem_t *)addr);
        memory_fences();
        uint32_t temp = memaccesstime(addr);
        if (temp > LLC_BOUND_HIGH)
        {
            i--;
        }
        else
        {
            data[i] = temp;
        }
    }
    results_t results;
    results.average = calculate_mean(data, SAMPLES);
    results.stddev = calculate_stddev(data, SAMPLES, results.average);
    return results;
}

int predict_slice(double *averages, double measured_avg)
{
    for (int i = 0; i < LLC_SLICES - 1; i++)
    {
        double threshold = (averages[i] + averages[i + 1]) / 2.0;
        if (measured_avg < threshold)
        {
            return i; // belongs to slice 'i'
        }
    }
    // If no slice was found, it belongs to the last slice
    return LLC_SLICES - 1;
}

int main()
{
    size_t len = LLC_SIZE * 4;
    void *calib_mem = (void *)initialise_memory(len, 0, PAGE_SIZE);
    void *mem = (void *)initialise_memory(len, 0, PAGE_SIZE);

    for (int s = 0; s < LLC_SLICES; s++)
    {
        for (uintptr_t i = 0; i < len; i++)
        {
            void *addr = (void *)((uintptr_t)calib_mem + i);
            int slice = get_address_slice(virtual_to_physical((uint64_t)addr));

            if (slice == s)
            {
                results_t results = get_address_time(addr);
                break;
            }
        }
    }

    results_t *slice_results = calloc(LLC_SLICES, sizeof(results_t));
    double *slice_averages = calloc(LLC_SLICES, sizeof(double));
    for (int t = 0; t < 1000; t++)
    {
        for (int s = 0; s < LLC_SLICES; s++)
        {
            for (uintptr_t i = (t * 4 * KB); i < len; i += 0x40)
            {
                void *addr = (void *)((uintptr_t)calib_mem + i);
                int slice = get_address_slice(virtual_to_physical((uint64_t)addr));

                if (slice == s)
                {
                    set_addr_state((uintptr_t)addr, LLC);
                    memory_fences();

                    results_t results = get_address_time(addr);
                    slice_results[s].average += results.average;
                    slice_results[s].stddev += results.stddev;
                    break;
                }
            }
        }
    }

    for (int s = 0; s < LLC_SLICES; s++)
    {
        slice_averages[s] = slice_results[s].average / 1000.0;
        printf("%d, %f, %f\n", s, slice_results[s].average / 1000.0, slice_results[s].stddev / 1000.0);
    }

    int confusion_matrix[LLC_SLICES][LLC_SLICES] = {0}; // Initialize a 2D array to store the confusion matrix

    for (uintptr_t i = 0; i < 0x40 * 100000; i += 0x40)
    {
        void *addr = (void *)((uintptr_t)mem + i);
        int slice = get_address_slice(virtual_to_physical((uint64_t)addr));
        set_addr_state((uintptr_t)addr, LLC);
        memory_fences();

        results_t results = get_address_time(addr);
        int cslice = predict_slice(slice_averages, results.average);

        // Update the confusion matrix
        confusion_matrix[slice][cslice]++;
    }

    // Print the confusion matrix
    printf("Confusion Matrix:\n");

    for (int i = 0; i < LLC_SLICES; i++)
    {
        printf("%6d  ", i);
        for (int j = 0; j < LLC_SLICES; j++)
        {
            printf("%6d", confusion_matrix[i][j]);
        }
        printf("\n");
    }

    free(slice_results);
    free(slice_averages);
    return 0;
}
