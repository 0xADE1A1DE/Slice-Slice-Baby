#include "util.h"
#include <perf_counters.h>
#include <string.h>
#include <time.h>

// TODO add a 'measurement_util' which gets the uncore perfmon, simflag, simulation vars so that we can simplify the calls to using performance counters or simulated vars.
// TODO investigate finding XOR masks and seeing if a pattern can be found for NPOT slice processors
// Try doubling 'pattern length' for when the incorrect slices happen

int main(int argc, char const *argv[])
{
    start_timer();
    // Measurement Variables
    uncore_perfmon_t u;
    CBO_COUNTER_INFO_T *cbo_ctrs;
    size_t num_cbos;

    // Retrieved Information
    uint64_t *xor_mask = 0;
    size_t reduction_bits = 0;
    size_t addr_bits = 0;
    size_t seq_len = 1;
    int *base_sequence = calloc(seq_len, sizeof(int));
    int *sequences = calloc(MAX_ADDR_BITS * seq_len, sizeof(int));

    // Sim Variables
    int sim_flag = 0;
    size_t sim_num_cbos;
    uint64_t *sim_xor_mask = 0;
    size_t sim_reduction_bits = 0;
    size_t sim_addr_bits = 0;
    size_t sim_seq_len = 0;
    int *sim_base_sequence = 0;

    num_cbos = uncore_get_num_cbo(AFFINITY);
    cbo_ctrs = malloc(num_cbos * sizeof(CBO_COUNTER_INFO_T));
    for (int i = 0; i < num_cbos; ++i)
    {
        COUNTER_T temp = {0x34, 0x8F, 0, "UNC_CBO_CACHE_LOOKUP.ANY_MESI"};
        cbo_ctrs[i].counter = temp;
        cbo_ctrs[i].cbo = i;
        cbo_ctrs[i].flags = (MSR_UNC_CBO_PERFEVT_EN);
    }
    uncore_perfmon_init(&u, AFFINITY, UNCORE_PERFMON_SAMPLES, num_cbos, 0, 0, cbo_ctrs, NULL, NULL);
    uncore_enable_all_counters(&u);

    // Simulate a processor
    if (argc == 3 && strcmp(argv[1], "--sim") == 0)
    {
        if (get_sim_params(argv, &sim_num_cbos, &sim_xor_mask, &sim_reduction_bits, &sim_addr_bits, &sim_base_sequence, &sim_seq_len))
        {
            fprintf(stderr, "Usage: ./run.sh [--save] [--sim $FILE_NAME]\n");
            goto CLEANUP;
        }
        sim_flag = 1;
        num_cbos = sim_num_cbos;
    }
    else if (argc != 1 && argc != 3)
    {
        fprintf(stderr, "Usage: ./run.sh [--save | --sim $FILE_NAME]\n");
        goto CLEANUP;
    }

    if (is_power_of_two(num_cbos))
    {
        int b = find_set_bit(L3_CACHELINE);
        while (1)
        {
            int slice = -2;
            uint64_t address = (1ULL << b);

            if (!sim_flag)
                slice = get_address_slice(&u, address);
            else
                slice = get_address_slice_sim(address, sim_xor_mask, sim_reduction_bits, sim_addr_bits, sim_base_sequence, sim_seq_len);

            // Successfully got a slice value
            if (slice >= 0)
            {
                fprintf(stderr, "Bit %d XOR: %d\n", b, slice);
                update_xor_mask(&xor_mask, &reduction_bits, b, slice);
                // Go to next address bit
                b++;
            }
            //-1 is when we have an error mapping memory which means we have hit the memory limit for the processor
            else if (slice == -1)
            {
                addr_bits = b;
                break;
            }
        }
    }
    else
    {
        // Start from after cache line bits
        for (int b = find_set_bit(L3_CACHELINE);;)
        {
            for (int i = 0; i < seq_len; ++i)
            {
                uint64_t address = (1ULL << b) + (i * L3_CACHELINE);
                int slice = -2;
                while (slice < 0)
                {
                    if (!sim_flag)
                        slice = get_address_slice(&u, address);
                    else
                        slice = get_address_slice_sim(address, sim_xor_mask, sim_reduction_bits, sim_addr_bits, sim_base_sequence, sim_seq_len);
                    // Successfully got a slice value
                    if (slice >= 0)
                    {
                        break;
                    }
                    //-1 is when we have an error mapping memory which means we have hit the memory limit for the processor
                    else if (slice == -1)
                    {
                        addr_bits = b;
                        slice = -1;
                        // I am jumping out of a double nested loop because I don't want to precompute how many address bits there are. Forgive me.
                        goto EXIT_XOR_MAP_LOOP;
                    }
                }
                sequences[(b * seq_len) + i] = slice;
            }
            int correct;
            for (int i = 0; i < seq_len; ++i)
            {
                correct = 0;
                // XOR the location of each i with
                for (int j = 0; j < seq_len; ++j)
                {
                    int slice = sequences[(b * seq_len) + (j ^ i)];
                    if (base_sequence[j] == slice)
                        correct++;
                }
                if (correct == seq_len)
                {
                    fprintf(stderr, "Bit %d XOR: %d\n", b, i);
                    update_xor_mask(&xor_mask, &reduction_bits, b, i);
                    b++;
                    break;
                }
            }
            if (correct != seq_len)
            {
                // Double base sequence
                base_sequence = realloc(base_sequence, (seq_len << 1) * sizeof(int));
                for (size_t cache_line = seq_len; cache_line < (seq_len << 1);)
                {
                    int slice = -2;
                    if (!sim_flag)
                        slice = get_address_slice(&u, cache_line * L3_CACHELINE);
                    else
                        slice = get_address_slice_sim(cache_line * L3_CACHELINE, sim_xor_mask, sim_reduction_bits, sim_addr_bits, sim_base_sequence, sim_seq_len);
                    // Successfully got a slice value
                    if (slice >= 0)
                    {
                        base_sequence[cache_line] = slice;
                        cache_line++;
                    }
                    //-1 is when we have an error mapping memory which means we have hit the memory limit for the processor
                    else if (slice == -1)
                    {
                        fprintf(stderr, "Error: Hit memory limit for processor. Check membypass.ko is installed.\n");
                        goto CLEANUP;
                    }
                }
                seq_len <<= 1;
                fprintf(stderr, "Bit %d XOR not found! Doubled sequence length to %ld and retrying\n", b, seq_len);
                sequences = realloc(sequences, MAX_ADDR_BITS * seq_len * sizeof(int));
                // Setting prior bits to 0 through each of the XOR Masks
                for (int i = 0; i < reduction_bits; ++i)
                {
                    xor_mask[i] &= 0xFFFFFFFFFFFFFFFFULL << (find_set_bit(L3_CACHELINE) + find_set_bit(seq_len));
                }
                b = find_set_bit(L3_CACHELINE) + find_set_bit(seq_len);
            }
        }
    }
EXIT_XOR_MAP_LOOP:
    // Testing
    srand(time(NULL));
    fprintf(stderr, "\n");
    for (uint64_t i = 0; i < 32; ++i)
    {
        uint32_t lower = rand();
        uint32_t upper = rand();
        uint64_t address = (((uint64_t)upper << 32) | lower) % (1ULL << addr_bits);
        int slice = -2;
        while (slice < 0)
        {
            if (!sim_flag)
                slice = get_address_slice(&u, address);
            else
                slice = get_address_slice_sim(address, sim_xor_mask, sim_reduction_bits, sim_addr_bits, sim_base_sequence, sim_seq_len);
            // Successfully got a slice value
            if (slice >= 0)
                break;
            //-1 is when we have an error mapping memory which means we have hit the memory limit for the processor
            else if (slice == -1)
            {
                fprintf(stderr, "Error: Hit memory limit for processor. Check membypass.ko is installed.\n");
                goto CLEANUP;
            }
        }
        int cslice = get_address_slice_sim(address, xor_mask, reduction_bits, addr_bits, base_sequence, seq_len);
        if (slice == cslice)
            fprintf(stderr, "Address: %010lx | Slice: %d | CSlice: %d\n", address, slice, cslice);
        else
            fprintf(stderr, "Address: %010lx | Slice: %d | CSlice: %d **INCORRECT**\n", address, slice, cslice);
    }

    fprintf(stderr, "\n");
    for (int i = 0; i < reduction_bits; i++)
    {
        fprintf(stderr, "ID%02d = |", i);
        for (uint64_t b = addr_bits - 1; b >= find_set_bit(L3_CACHELINE); --b)
        {
            if (is_bit_k_set(xor_mask[i], b))
                fprintf(stderr, "▄");
            else
                fprintf(stderr, " ");
        }
        if (i < reduction_bits)
            fprintf(stderr, "|\n");
        else
            fprintf(stderr, "|\n\n");
    }

    fprintf(stderr, "\n");
    printf("%ld\n", num_cbos);
    printf("%ld\n", addr_bits);
    print_xor_mask(xor_mask, reduction_bits);
    print_base_sequence(base_sequence, seq_len);

CLEANUP:
    uncore_perfmon_destroy(&u);
    free(cbo_ctrs);
    free(xor_mask);
    free(base_sequence);
    free(sequences);

    if (sim_flag)
    {
        free(sim_xor_mask);
        free(sim_base_sequence);
    }
    double time = stop_timer();
    fprintf(stderr, "Time taken: %fs\n", time);
    return 0;
}
