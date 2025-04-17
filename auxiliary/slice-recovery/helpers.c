#include "helpers.h"

void clflush(void *v, void *v1)
{
    asm volatile("clflushopt 0(%0)\n" : : "r"(v) :);
}

void print_set_bits(uint64_t addr)
{
    printf("\t");
    for (int i = 0; i < 64; ++i)
    {
        if ((addr >> i) & 1)
        {
            printf("%d,", i);
        }
    }
}

size_t find_set_bit(uint64_t n)
{
    size_t pos = -1;
    for (int i = 63; i >= 0; i--)
    {
        if ((n >> i) & 1)
        {
            pos = i;
            break;
        }
    }
    return pos;
}

int find_lower_power_of_two(uint64_t n)
{
    if (n == 0)
        return 1; // Special case for 0

    unsigned int pow = 1;
    while (pow < n)
    {
        pow <<= 1; // Left shift to double the power
    }

    return pow >> 1;
}

int is_bit_k_set(uint64_t n, int k)
{
    if (n & (1ULL << (k)))
        return 1;
    else
        return 0;
}

int count_set_bits(uint64_t n)
{
    int count = 0;

    while (n > 0)
    {
        count += (n & 1);
        n >>= 1;
    }

    return count;
}

// https://stackoverflow.com/questions/600293/how-to-check-if-a-number-is-a-power-of-2#600306
int is_power_of_two(uint64_t x)
{
    return (x != 0) && ((x & (x - 1)) == 0);
}

double calculate_mean(double data[], uint32_t len)
{
    double sum = 0.0;
    for (int i = 0; i < len; ++i)
    {
        sum += data[i];
    }
    return (sum / (double)len);
}

double calculate_stddev(double data[], uint32_t len, double mean)
{
    double stddev = 0.0;
    for (int i = 0; i < len; ++i)
    {
        stddev += pow(data[i] - mean, 2);
    }
    return sqrt(stddev / len);
}

double calculate_zscore(double datum, double mean, double stddev)
{
    return ((datum - mean) / stddev);
}
