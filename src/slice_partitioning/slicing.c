#include "slice_partitioning/slicing.h"
#include "config.h"

#if defined INTEL_GEN6_4_SLICE
const int seq_len = 0;
const int reduction_bits = 2;
const int addr_bits = 39;
const uint64_t xor_mask[2] = {0x5b5f575440ULL, 0x6eb5faa880ULL};
const int *base_sequence = NULL;
#elif defined INTEL_GEN6_ALTERNATE
const int seq_len = 0;
const int reduction_bits = 3;
const int addr_bits = 39;
const uint64_t xor_mask[3] = {0x5b5f575440ULL, 0x6eb5faa880ULL, 0x3cccc93100ULL};
const int *base_sequence = NULL;
#elif defined INTEL_GEN13_8_SLICE || defined INTEL_GEN11_8_SLICE
const int seq_len = 0;
const int reduction_bits = 3;
const int addr_bits = 39;
const uint64_t xor_mask[3] = {0x5b5f575440ULL, 0x71aeeb1200ULL, 0x6d87f2c00ULL};
const int *base_sequence = NULL;
#elif defined INTEL_GEN8_6_SLICE || defined INTEL_GEN9_6_SLICE
const int seq_len = 128;
const int reduction_bits = 7;
const int addr_bits = 39;
const uint64_t xor_mask[7] = {0x21ae7be000ULL, 0x435cf7c000ULL, 0x2717946000ULL, 0x4e2f28c000ULL, 0x1c5e518000ULL, 0x38bca30000ULL, 0x50d73de000ULL};
const int base_sequence[128] = {0, 1, 2, 3, 1, 4, 3, 4, 1, 0, 3, 2, 0, 5, 2, 5, 1, 0, 3, 2, 0, 5, 2, 5, 0, 5, 2, 5, 1, 4, 3, 4, 0, 1, 2, 3, 5, 0, 5, 2, 5, 0, 5, 2, 4, 1, 4, 3, 1, 0, 3, 2, 4, 1, 4, 3, 4, 1, 4, 3, 5, 0, 5, 2, 2, 3, 0, 1, 5, 2, 5, 0, 3, 2, 1, 0, 4, 3, 4, 1, 3, 2, 1, 0, 4, 3, 4, 1, 4, 3, 4, 1, 5, 2, 5, 0, 2, 3, 0, 1, 3, 4, 1, 4, 3, 4, 1, 4, 2, 5, 0, 5, 3, 2, 1, 0, 2, 5, 0, 5, 2, 5, 0, 5, 3, 4, 1, 4};
#elif defined INTEL_GEN12_10_SLICE || defined INTEL_GEN10_10_SLICE
const int seq_len = 128;
const int reduction_bits = 7;
const int addr_bits = 39;
const uint64_t xor_mask[7] = {0x21ae7be000ULL, 0x435cf7c000ULL, 0x2717946000ULL, 0x4e2f28c000ULL, 0x1c5e518000ULL, 0x38bca30000ULL, 0x50d73de000ULL};
const int base_sequence[128] = {0, 5, 0, 5, 3, 6, 3, 6, 1, 4, 1, 4, 2, 7, 2, 7, 1, 4, 1, 4, 2, 7, 2, 7, 0, 5, 8, 9, 3, 6, 9, 8, 4, 1, 4, 1, 7, 2, 7, 2, 5, 0, 9, 8, 6, 3, 8, 9, 5, 0, 5, 0, 6, 3, 6, 3, 4, 1, 8, 9, 7, 2, 9, 8, 4, 1, 4, 1, 7, 2, 7, 2, 5, 0, 5, 0, 6, 3, 6, 3, 5, 0, 5, 0, 6, 3, 6, 3, 8, 9, 4, 1, 9, 8, 7, 2, 0, 5, 0, 5, 3, 6, 3, 6, 9, 8, 1, 4, 8, 9, 2, 7, 1, 4, 1, 4, 2, 7, 2, 7, 8, 9, 0, 5, 9, 8, 3, 6};
#elif defined INTEL_GEN13_12_SLICE
const int seq_len = 512;
const int reduction_bits = 9;
const int addr_bits = 39;
const uint64_t xor_mask[9] = {0x52c6a38000ULL, 0x30342b8000ULL, 0x547f480000ULL, 0x3d47f08000ULL, 0x1c5e518000ULL, 0x38bca30000ULL, 0x23bfe18000ULL, 0x0000000000ULL, 0x7368d80000ULL};
const int base_sequence[512] = {0, 1,  4, 5,  1, 8, 5, 8, 3, 2, 7, 6, 2, 11, 6, 11, 1, 0,  5, 4,  0, 9,  4, 9,  2, 11, 6, 11, 3, 10, 7, 10, 2, 3, 6, 7, 11, 2, 11, 6, 9,  0, 9,  4, 8, 1, 8, 5, 3, 2, 7, 6, 10, 3, 10, 7, 8,  1, 8,  5, 9, 0, 9, 4, 6, 7, 2, 3, 11, 6, 11, 2, 5,  4, 1,  0, 8, 5, 8, 1, 7, 6, 3, 2, 10, 7, 10, 3, 8,  5, 8,  1, 9, 4, 9, 0, 4, 5,  0, 1,  5, 8,  1, 8,  7, 10, 3, 10, 6, 11, 2, 11, 5, 4,  1, 0,  4, 9,  0, 9,  6, 11, 2, 11, 7, 10, 3, 10, 6, 11, 2, 11, 7, 6, 3, 2, 5, 8, 1, 8, 4, 5,  0, 1,  7, 10, 3, 10, 6, 7, 2, 3, 4, 9, 0, 9, 5, 8,  1, 8,  8,  5, 8,  1, 5,  4, 1,  0, 11, 6, 11, 2, 10, 7, 10, 3, 9,  4, 9,  0, 4,  5, 0,  1, 10, 7, 10, 3, 11, 6, 11, 2, 8, 1, 8, 5, 1,  0, 5,  4, 11, 2, 11, 6, 2, 3, 6, 7, 9,  0, 9,  4, 0,  1, 4,  5, 10, 3, 10, 7, 11, 2, 11, 6, 2, 11, 6, 11, 3, 2, 7, 6, 1, 8, 5, 8, 0, 9,  4, 9,  3, 10, 7, 10, 2, 3, 6, 7, 0, 9, 4, 9, 1, 8,  5, 8,
                                3, 10, 7, 10, 2, 3, 6, 7, 0, 9, 4, 9, 1, 0,  5, 4,  2, 11, 6, 11, 3, 10, 7, 10, 1, 8,  5, 8,  0, 1,  4, 5,  9, 0, 9, 4, 8,  1, 8,  5, 10, 3, 10, 7, 3, 2, 7, 6, 8, 1, 8, 5, 9,  0, 9,  4, 11, 2, 11, 6, 2, 3, 6, 7, 9, 4, 9, 0, 4,  5, 0,  1, 10, 7, 10, 3, 7, 6, 3, 2, 8, 5, 8, 1, 9,  4, 9,  0, 11, 6, 11, 2, 6, 7, 2, 3, 7, 10, 3, 10, 6, 11, 2, 11, 4, 9,  0, 9,  5, 4,  1, 0,  6, 11, 2, 11, 7, 10, 3, 10, 5, 8,  1, 8,  4, 5,  0, 1,  5, 4,  1, 0,  4, 9, 0, 9, 6, 7, 2, 3, 7, 10, 3, 10, 4, 9,  0, 9,  5, 8, 1, 8, 7, 6, 3, 2, 6, 11, 2, 11, 11, 6, 11, 2, 10, 7, 10, 3, 4,  5, 0,  1, 9,  4, 9,  0, 10, 7, 10, 3, 11, 6, 11, 2, 5,  4, 1,  0, 8,  5, 8,  1, 3, 2, 7, 6, 10, 3, 10, 7, 0,  1, 4,  5, 9, 0, 9, 4, 10, 3, 10, 7, 11, 2, 11, 6, 1,  0, 5,  4, 8,  1, 8,  5, 1, 8,  5, 8,  0, 9, 4, 9, 2, 3, 6, 7, 3, 10, 7, 10, 0, 9,  4, 9,  1, 8, 5, 8, 3, 2, 7, 6, 2, 11, 6, 11};
#elif defined INTEL_GEN14_12_SLICE
const int seq_len = 512;
const int reduction_bits = 9;
const int addr_bits = 37;
const uint64_t xor_mask[9] = {0x2f52c6a78000ULL, 0xcb0342b8000ULL, 0x35d47f480000ULL, 0x39bd47f48000ULL, 0x109c5e518000ULL, 0x2038bca30000ULL, 0xe23bfe18000ULL, 0x0000000000ULL, 0x31f368dc0000ULL};
const int base_sequence[512] = {0, 1,  4, 5,  1, 8, 5, 8, 3, 2, 7, 6, 2, 11, 6, 11, 1, 0,  5, 4,  0, 9,  4, 9,  2, 11, 6, 11, 3, 10, 7, 10, 2, 3, 6, 7, 11, 2, 11, 6, 9,  0, 9,  4, 8, 1, 8, 5, 3, 2, 7, 6, 10, 3, 10, 7, 8,  1, 8,  5, 9, 0, 9, 4, 6, 7, 2, 3, 11, 6, 11, 2, 5,  4, 1,  0, 8, 5, 8, 1, 7, 6, 3, 2, 10, 7, 10, 3, 8,  5, 8,  1, 9, 4, 9, 0, 4, 5,  0, 1,  5, 8,  1, 8,  7, 10, 3, 10, 6, 11, 2, 11, 5, 4,  1, 0,  4, 9,  0, 9,  6, 11, 2, 11, 7, 10, 3, 10, 6, 11, 2, 11, 7, 6, 3, 2, 5, 8, 1, 8, 4, 5,  0, 1,  7, 10, 3, 10, 6, 7, 2, 3, 4, 9, 0, 9, 5, 8,  1, 8,  8,  5, 8,  1, 5,  4, 1,  0, 11, 6, 11, 2, 10, 7, 10, 3, 9,  4, 9,  0, 4,  5, 0,  1, 10, 7, 10, 3, 11, 6, 11, 2, 8, 1, 8, 5, 1,  0, 5,  4, 11, 2, 11, 6, 2, 3, 6, 7, 9,  0, 9,  4, 0,  1, 4,  5, 10, 3, 10, 7, 11, 2, 11, 6, 2, 11, 6, 11, 3, 2, 7, 6, 1, 8, 5, 8, 0, 9,  4, 9,  3, 10, 7, 10, 2, 3, 6, 7, 0, 9, 4, 9, 1, 8,  5, 8,
                                3, 10, 7, 10, 2, 3, 6, 7, 0, 9, 4, 9, 1, 0,  5, 4,  2, 11, 6, 11, 3, 10, 7, 10, 1, 8,  5, 8,  0, 1,  4, 5,  9, 0, 9, 4, 8,  1, 8,  5, 10, 3, 10, 7, 3, 2, 7, 6, 8, 1, 8, 5, 9,  0, 9,  4, 11, 2, 11, 6, 2, 3, 6, 7, 9, 4, 9, 0, 4,  5, 0,  1, 10, 7, 10, 3, 7, 6, 3, 2, 8, 5, 8, 1, 9,  4, 9,  0, 11, 6, 11, 2, 6, 7, 2, 3, 7, 10, 3, 10, 6, 11, 2, 11, 4, 9,  0, 9,  5, 4,  1, 0,  6, 11, 2, 11, 7, 10, 3, 10, 5, 8,  1, 8,  4, 5,  0, 1,  5, 4,  1, 0,  4, 9, 0, 9, 6, 7, 2, 3, 7, 10, 3, 10, 4, 9,  0, 9,  5, 8, 1, 8, 7, 6, 3, 2, 6, 11, 2, 11, 11, 6, 11, 2, 10, 7, 10, 3, 4,  5, 0,  1, 9,  4, 9,  0, 10, 7, 10, 3, 11, 6, 11, 2, 5,  4, 1,  0, 8,  5, 8,  1, 3, 2, 7, 6, 10, 3, 10, 7, 0,  1, 4,  5, 9, 0, 9, 4, 10, 3, 10, 7, 11, 2, 11, 6, 1,  0, 5,  4, 8,  1, 8,  5, 1, 8,  5, 8,  0, 9, 4, 9, 2, 3, 6, 7, 3, 10, 7, 10, 0, 9,  4, 9,  1, 8, 5, 8, 3, 2, 7, 6, 2, 11, 6, 11};
#elif defined INTEL_XEON_GOLD_6130
const int seq_len = 0;
const int reduction_bits = 4;
const int addr_bits = 37;
const uint64_t xor_mask[4] = {0x1b5f575440ULL, 0xeb5faa880ULL, 0x1cccc93100ULL, 0x11aeeb1200ULL};
const int *base_sequence = NULL;

#elif defined INTEL_XEON_GOLD_6152
int seq_len;
int reduction_bits;
int addr_bits;
uint64_t *xor_mask;
int *base_sequence;

void parse_file(const char *filename)
{
    FILE *file = fopen(filename, "r");
    if (!file)
    {
        perror("Failed to open file");
        exit(1);
    }

    char *line = NULL;
    size_t len = 0;

    // Read and ignore the first line
    getline(&line, &len, file);

    // Read the second line for addr_bits
    getline(&line, &len, file);

    // Handle commas in the second line
    char *token = strtok(line, ",");
    if (token != NULL)
    {
        addr_bits = atoi(token); // Convert the first value to addr_bits
    }

    // Read the third line for reduction_bits and xor_mask
    getline(&line, &len, file);
    token = strtok(line, ",");
    reduction_bits = 0;

    // Count hex values for xor_mask
    while (token != NULL)
    {
        reduction_bits++;
        token = strtok(NULL, ",");
    }

    // Allocate memory for xor_mask
    xor_mask = malloc(reduction_bits * sizeof(uint64_t));

    // Reset the file pointer and read the third line again
    fseek(file, 0, SEEK_SET);
    for (int i = 0; i < 3; i++)
    {
        getline(&line, &len, file); // Skip lines 1 and 2
    }
    token = strtok(line, ",");
    int index = 0;

    while (token != NULL)
    {
        xor_mask[index] = strtoull(token, NULL, 16);
        index++;
        token = strtok(NULL, ",");
    }

    // Read the fourth line for base_sequence
    getline(&line, &len, file);
    token = strtok(line, ",");
    seq_len = 0;

    // Count base sequence elements
    while (token != NULL)
    {
        seq_len++;
        token = strtok(NULL, ",");
    }

    // Allocate memory for base_sequence
    base_sequence = malloc(seq_len * sizeof(int));

    // Reset the file pointer and read the fourth line again to fill base_sequence
    fseek(file, 0, SEEK_SET);
    for (int i = 0; i < 4; i++)
    {
        getline(&line, &len, file); // Skip lines 1, 2, and 3
    }
    token = strtok(line, ",");
    index = 0;

    while (token != NULL)
    {
        base_sequence[index] = atoi(token);
        index++;
        token = strtok(NULL, ",");
    }

    // Clean up
    free(line);
    fclose(file);
}
#endif

int get_seq_len()
{
    return seq_len;
}

int get_reduction_bits()
{
    return reduction_bits;
}

const int *get_base_sequence()
{
    return (int *)base_sequence;
}

const uint64_t *get_xor_mask()
{
    return (uint64_t *)xor_mask;
}

int get_address_slice(uint64_t address)
{
#if defined INTEL_XEON_GOLD_6152
    if (base_sequence == NULL)
        parse_file("../src/slice_partitioning/hash_functions/xeon_22.txt");
#endif

    if ((address >> (addr_bits + 1)) > 0)
    {
        return -1;
    }

    int slice = 0;
    for (int b = 0; b < reduction_bits; ++b)
    {
        slice |= ((__builtin_popcountll(xor_mask[b] & address) % 2) << b);
    }

    if (seq_len > 0)
    {
        int sequence_offset = ((uint64_t)address % (seq_len * CACHELINE)) >> 6;
        slice = base_sequence[sequence_offset ^ slice];
    }

    return slice;
}
