#define _GNU_SOURCE
#ifndef UTILS_H
#define UTILS_H
#include "main.h"
#include <sched.h>
//===================================================
#define LNEXT(t) (*(void **)(t))
#define OFFSET(p, o) ((void *)((uintptr_t)(p) + (o)))
#define NEXTPTR(p) (OFFSET((p), sizeof(void *)))
//===================================================

//======================Prototypes definitions===========================
unsigned long long my_rand(unsigned long long limit);
double nCr(int n, int r);
float binomialProbability(int n, int k, float p);
float probabilityToBeEvictionSet(float p, int N);
void gaurd();
void flush(void *head);
void collectReps(void *head);
void collectCands(void *head);
void set_cpu();
void mergeLists(void *first, void *sec);
void *getPointer(void *head, int position);
void memoryaccess(void *address, int direction);
void Prune_memoryaccess(void *srart, void *stop);
Struct logsGarbege();
int CheckResult();
Struct InitData(int N_c, int N_R, uint64_t offset);
void collectEvictionSet(Struct addresses);
int checkEviction(void *head, void *x, void *pp);
void printMapping();
Probe_Args remove_congrunt_addresses(void *head, int size);
void *getMappingHead();
Struct prepareForMapping(uint64_t offset);
void statistics(int NumExp, int AVGmappingSize, double AVGtime);
//==========================================================================

static inline int memaccess(void *v)
{
    int rv;
    __asm__ volatile("mov (%1), %0" : "+r"(rv) : "r"(v) :);
    return rv;
}

static inline uint32_t memaccesstime(void *v)
{
    uint32_t rv;
    __asm__ volatile("mfence\n"
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

static inline void clflush(void *v)
{
    __asm__ volatile("clflush 0(%0)" : : "r"(v) :);
}

static inline uint32_t rdtscp()
{
    uint32_t rv;
    __asm__ volatile("rdtscp" : "=a"(rv)::"edx", "ecx");
    return rv;
}

static inline uint64_t rdtscp64()
{
    uint32_t low, high;
    __asm__ volatile("rdtscp" : "=a"(low), "=d"(high)::"ecx");
    return (((uint64_t)high) << 32) | low;
}

static inline void mfence()
{
    __asm__ volatile("mfence");
}

#endif
