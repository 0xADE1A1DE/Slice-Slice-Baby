//////////////////////////////////////////////////////////////////////////////////////////
// gcc -o example example_hash_function_usage.c
//////////////////////////////////////////////////////////////////////////////////////////

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

//////////////////////////////////////////////////////////////////////////////////////////

int get_msb(uint64_t n)
{
    if (n == 0)
        return 0;

    int msb = 0;
    n = n / 2;
    while (n != 0)
    {
        n = n / 2;
        msb++;
    }

    return (1 << msb);
}

int find_set_bit(uint64_t n)
{
    int pos = 0;
    n >>= 1;
    while (n != 0)
    {
        pos += 1;
        n >>= 1;
    }
    return pos;
}

int is_bit_k_set(uint64_t num, int k)
{
    if (num & (1ULL << (k)))
        return 1;
    else
        return 0;
}

uint64_t count_bits(uint64_t n)
{
    uint64_t count = 0;
    while (n)
    {
        n &= (n - 1);
        count++;
    }
    return count;
}

int calculate_xor_reduction_with_masks(uint64_t addr, uint64_t *masks, int reduction_bits)
{
    uint64_t res = 0;

    for (int b = 0; b < reduction_bits; ++b)
    {
        res |= ((uint64_t)(count_bits(masks[b] & addr) % 2) << b);
    }

    return res;
}

uint64_t calculate_xor_reduction(uint64_t addr, int *xor_map, int addr_bits)
{
    // Check if any bits are set past addr_bits
    if ((addr >> addr_bits) > 0)
        return -1;
    uint64_t xor_op = 0;

    for (int b = 0; b < addr_bits; ++b)
    {
        if (is_bit_k_set(addr, b))
            xor_op ^= (uint64_t)xor_map[b];
    }
    return xor_op;
}

int calculate_address_slice(uint64_t paddr, int *base_sequence, size_t seq_len, uint64_t *xor_masks, int reduction_bits)
{
    int calc_slice = -1;
    int xor_op = -1;

    // xor_op = calculate_xor_reduction(paddr, xor_map, addr_bits);
    xor_op = calculate_xor_reduction_with_masks(paddr, xor_masks, reduction_bits);

    // Get the slice of the address's sequence offset within the master sequence
    if (xor_op >= 0)
    {
        if (base_sequence == NULL)
        {
            calc_slice = xor_op;
        }
        else
        {
            int sequence_offset = ((uint64_t)paddr % (seq_len * 64)) >> 6;
            calc_slice = base_sequence[sequence_offset ^ xor_op];
        }
    }
    else
    {
        calc_slice = -1;
    }
    return calc_slice;
}

int main()
{
    uint64_t xor_masks[9] = {0x52c6a78000ULL, 0x30342b8000ULL, 0x547f480000ULL, 0x3d47f48000ULL, 0x1c5e518000ULL, 0x38bca30000ULL, 0x23bfe18000ULL, 0x0000000000ULL, 0x7368dc0000ULL};
    uint64_t test_xor_masks[1] = {0x54C896C740ULL};

    int base_sequence[512] = {0, 1,  4, 5,  1, 8, 5, 8, 3, 2, 7, 6, 2, 11, 6, 11, 1, 0,  5, 4,  0, 9,  4, 9,  2, 11, 6, 11, 3, 10, 7, 10, 2, 3, 6, 7, 11, 2, 11, 6, 9,  0, 9,  4, 8, 1, 8, 5, 3, 2, 7, 6, 10, 3, 10, 7, 8,  1, 8,  5, 9, 0, 9, 4, 6, 7, 2, 3, 11, 6, 11, 2, 5,  4, 1,  0, 8, 5, 8, 1, 7, 6, 3, 2, 10, 7, 10, 3, 8,  5, 8,  1, 9, 4, 9, 0, 4, 5,  0, 1,  5, 8,  1, 8,  7, 10, 3, 10, 6, 11, 2, 11, 5, 4,  1, 0,  4, 9,  0, 9,  6, 11, 2, 11, 7, 10, 3, 10, 6, 11, 2, 11, 7, 6, 3, 2, 5, 8, 1, 8, 4, 5,  0, 1,  7, 10, 3, 10, 6, 7, 2, 3, 4, 9, 0, 9, 5, 8,  1, 8,  8,  5, 8,  1, 5,  4, 1,  0, 11, 6, 11, 2, 10, 7, 10, 3, 9,  4, 9,  0, 4,  5, 0,  1, 10, 7, 10, 3, 11, 6, 11, 2, 8, 1, 8, 5, 1,  0, 5,  4, 11, 2, 11, 6, 2, 3, 6, 7, 9,  0, 9,  4, 0,  1, 4,  5, 10, 3, 10, 7, 11, 2, 11, 6, 2, 11, 6, 11, 3, 2, 7, 6, 1, 8, 5, 8, 0, 9,  4, 9,  3, 10, 7, 10, 2, 3, 6, 7, 0, 9, 4, 9, 1, 8,  5, 8,
                              3, 10, 7, 10, 2, 3, 6, 7, 0, 9, 4, 9, 1, 0,  5, 4,  2, 11, 6, 11, 3, 10, 7, 10, 1, 8,  5, 8,  0, 1,  4, 5,  9, 0, 9, 4, 8,  1, 8,  5, 10, 3, 10, 7, 3, 2, 7, 6, 8, 1, 8, 5, 9,  0, 9,  4, 11, 2, 11, 6, 2, 3, 6, 7, 9, 4, 9, 0, 4,  5, 0,  1, 10, 7, 10, 3, 7, 6, 3, 2, 8, 5, 8, 1, 9,  4, 9,  0, 11, 6, 11, 2, 6, 7, 2, 3, 7, 10, 3, 10, 6, 11, 2, 11, 4, 9,  0, 9,  5, 4,  1, 0,  6, 11, 2, 11, 7, 10, 3, 10, 5, 8,  1, 8,  4, 5,  0, 1,  5, 4,  1, 0,  4, 9, 0, 9, 6, 7, 2, 3, 7, 10, 3, 10, 4, 9,  0, 9,  5, 8, 1, 8, 7, 6, 3, 2, 6, 11, 2, 11, 11, 6, 11, 2, 10, 7, 10, 3, 4,  5, 0,  1, 9,  4, 9,  0, 10, 7, 10, 3, 11, 6, 11, 2, 5,  4, 1,  0, 8,  5, 8,  1, 3, 2, 7, 6, 10, 3, 10, 7, 0,  1, 4,  5, 9, 0, 9, 4, 10, 3, 10, 7, 11, 2, 11, 6, 1,  0, 5,  4, 8,  1, 8,  5, 1, 8,  5, 8,  0, 9, 4, 9, 2, 3, 6, 7, 3, 10, 7, 10, 0, 9,  4, 9,  1, 8, 5, 8, 3, 2, 7, 6, 2, 11, 6, 11};

    // for (int i = 0; i < 9; ++i)
    // {
    //     xor_masks[i] |= 1ULL << (find_set_bit(64) + i);
    // }

    for (uint64_t i = 0; i < 128; ++i)
    {
        int test_xor_op = calculate_xor_reduction_with_masks(i * 64, test_xor_masks, 1);
        int xor_op = calculate_xor_reduction_with_masks(i * 64, xor_masks, 9);
        int slice = calculate_address_slice(i * 64, base_sequence, 512, xor_masks, 9);

        printf("Addr: %ld | Slice: %d | XOR_OP: %d | TEST_XOR_OP: %d\n", i * 64, slice & 1, xor_op, test_xor_op);
    }
}
