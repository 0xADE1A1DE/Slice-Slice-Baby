#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef HELPERS_H
#define HELPERS_H

void clflush(void *v, void *v1);
void print_set_bits(uint64_t addr);
size_t find_set_bit(uint64_t n);
int is_bit_k_set(uint64_t n, int k);
int count_set_bits(uint64_t n);
int is_power_of_two(uint64_t x);

double calculate_mean(double data[], uint32_t len);
double calculate_stddev(double data[], uint32_t len, double mean);
double calculate_zscore(double datum, double mean, double stddev);

// From https://cs.adelaide.edu.au/~yval/Mastik/
static inline int memaccess(void *v)
{
    int rv;
    asm volatile("mov (%1), %0" : "+r"(rv) : "r"(v) :);
    return rv;
}

static inline uint32_t memaccesstime(void *v)
{
    uint32_t rv;
    asm volatile("mfence\n"
                 "lfence\n"
                 "rdtscp\n"
                 "mov %%eax, %%esi\n"
                 "mov (%1), %%eax\n"
                 "rdtscp\n"
                 "sub %%esi, %%eax\n"
                 : "=&a"(rv)
                 : "r"(v)
                 : "ecx", "edx", "esi");
    return rv;
}

#endif // HELPERS_H