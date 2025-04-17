#define _GNU_SOURCE
#include "utils.h"

extern void *CandAddressesPool, *RepAddressesPool;
extern void **GarbageReps, **GarbageCands;
extern int GarbageRepsIDX, GarbageCandsIDX, MappingIdx, Stride, S, mappingSize_array[BufferSize];
extern int *EvictionSetSize;
extern void ***Mapping;
extern clock_t time_array[BufferSize];

unsigned long long my_rand(unsigned long long limit)
{
    return ((unsigned long long)(((unsigned long long)rand() << 48) ^ ((unsigned long long)rand() << 32) ^ ((unsigned long long)rand() << 16) ^ (unsigned long long)rand())) % limit;
}

double nCr(int n, int r)
{
    // Since nCr is same as nC(n-r)
    // To decrease number of iterations
    if (r > n / 2)
        r = n - r;

    double answer = 1;
    for (int i = 1; i <= r; i++)
    {
        answer *= (n - r + i);
        answer /= i;
    }

    return answer;
}

// function to calculate binomial r.v. probability
float binomialProbability(int n, int k, float p)
{
    return nCr(n, k) * pow(p, k) * pow(1 - p, n - k);
}

float probabilityToBeEvictionSet(float p, int N)
{
    float dist = 0;
    for (int k = 0; k < W; k++)
    {
        dist = dist + binomialProbability(N, k, p);
    }
    return 1 - dist;
}

void gaurd()
{
    __asm__ volatile("mfence\n"
                     "lfence\n");
}

void flush(void *head)
{
    void *p, *pp;
    p = head;
    do
    {
        pp = p;
        p = LNEXT(p);
        gaurd();
        clflush(pp);
        gaurd();
    } while (p != (void *)head);
    gaurd();
    clflush(p);
    gaurd();
}

void set_cpu()
{
    cpu_set_t set;
    CPU_ZERO(&set);   // clear cpu mask
    CPU_SET(4, &set); // set cpu 0
    sched_setaffinity(0, sizeof(cpu_set_t), &set);
}

void collectReps(void *head)
{
    void *p = head;
    do
    {
        GarbageReps[GarbageRepsIDX] = p;
        GarbageRepsIDX++;
        p = LNEXT(p);
    } while (p != (void *)head);
    // flush(head);
}

void collectCands(void *head)
{
    void *p = head;
    do
    {
        GarbageCands[GarbageCandsIDX] = p;
        GarbageCandsIDX++;
        p = LNEXT(p);
    } while (p != (void *)head);
    // flush(head);
}

void mergeLists(void *first, void *sec)
{
    void *firstTail, *secTail;
    firstTail = LNEXT(NEXTPTR(first));
    secTail = LNEXT(NEXTPTR(sec));
    LNEXT(firstTail) = sec;
    LNEXT(secTail) = first;
    LNEXT(OFFSET(first, sizeof(void *))) = secTail;
    LNEXT(OFFSET(sec, sizeof(void *))) = firstTail;
}

void *getPointer(void *head, int position)
{
    void *p;
    int cnt = 0;
    p = head;
    do
    {
        gaurd();
        p = LNEXT(p);
        cnt++;
        if (cnt == position)
            return p;
    } while (p != (void *)head);
    gaurd();
    return NULL;
}

void memoryaccess(void *address, int direction)
{
    void *p, *pp;
    p = address;
    pp = p;
    do
    {
        gaurd();
        if (direction)
            p = LNEXT(p);
        else
            p = LNEXT(NEXTPTR(p));
    } while (p != (void *)pp);
}

/*void memoryaccess(void* head, int direction) {
    void* p;
    __asm__ __volatile__ (
        "mov %[head], %[p]\n"   // Move the head pointer to p
        "mov %[p], %%rax\n"     // Move p to rax
        "mov %[direction], %%ecx\n"  // Move direction to ecx
        "cmp $0, %%ecx\n"       // Compare direction with 0
        "jne forward\n"         // Jump to forward if direction is not equal to 0
        "backward:\n"
        "add $8, %%rax\n"       // Increment p by 8
        "mov (%%rax), %%rax\n"  // Load the value at address pointed by rax into rax
        "cmp %[head], %%rax\n"  // Compare the value in rax with the head pointer
        "jne backward\n"         // Jump back to forward if the values are not equal
        "jmp exit\n"
        "forward:\n"
        "mov (%%rax), %%rax\n"  // Load the value at address pointed by rax into rax
        "cmp %[head], %%rax\n"  // Compare the value in rax with the head pointer
        "jne forward\n"         // Jump back to forward if the values are not equal
        "exit:\n"
        : [p] "=r" (p)
        : [head] "r" (head), [direction] "r" (direction)  // Input: head and direction are used as register operands
        : "%rax", "%ecx"        // Clobbered registers: rax and rcx are modified in the assembly code
    );
}*/

/*
void Prune_memoryaccess(void* start, void* stop) {
    void* p;
    __asm__ volatile (
        "mov %[start], %[p]\n"   // Move the start pointer to p
        "mov %[p], %%rax\n"     // Move p to rax
        "mov %[stop], %%rbx\n"  // Move stop to rbx
        "fw:\n"
        "mov (%%rax), %%rax\n"  // Load the value at address pointed by rax into rax
        "cmp %%rax, %%rbx\n"    // Compare the value in rax with stop
        "jne fw\n"         // Jump back to forward if the values are not equal
        : [p] "=r" (p)
        : [start] "r" (start), [stop] "r" (stop)  // Input: start and stop are used as register operands
        : "%rax", "%rbx"        // Clobbered registers: rax and rbx are modified in the assembly code
    );
}*/

void Prune_memoryaccess(void *start, void *stop)
{
    // Only FW direction
    void *p;
    p = start;
    do
    {
        gaurd();
        p = LNEXT(p);
    } while (p != (void *)stop);
    gaurd();
}

Struct logsGarbege()
{ // FILE *file
    Struct addresses;
    for (int i = 0; i < GarbageCandsIDX; i++)
    {
        LNEXT(GarbageCands[i]) = GarbageCands[(i + 1) % GarbageCandsIDX];
        LNEXT(OFFSET(GarbageCands[i], sizeof(void *))) = GarbageCands[(i + GarbageCandsIDX - 1) % GarbageCandsIDX];
    }
    for (int i = 0; i < GarbageRepsIDX; i++)
    {
        LNEXT(GarbageReps[i]) = GarbageReps[(i + 1) % GarbageRepsIDX];
        LNEXT(OFFSET(GarbageReps[i], sizeof(void *))) = GarbageReps[(i + GarbageRepsIDX - 1) % GarbageRepsIDX];
    }

    addresses.candidates = GarbageCands[0];
    addresses.N_c = GarbageCandsIDX;
    addresses.Representatives = GarbageReps[0];
    addresses.N_R = GarbageRepsIDX;
    // flush(addresses.candidates);
    // flush(addresses.Representatives);
    GarbageCandsIDX = 0;
    GarbageRepsIDX = 0;
    return addresses;
}

#include <fcntl.h>
#include <unistd.h>

static int pagemap_fd = -1;
uint64_t virtual_to_physical(uint64_t vaddr)
{
    // Uncomment this eventually!
    // memaccess((void *)vaddr);

    if (pagemap_fd == -1)
        pagemap_fd = open("/proc/self/pagemap", O_RDONLY);

    unsigned long paddr = -1;
    unsigned long index = (vaddr / 4096) * sizeof(paddr);
    if (pread(pagemap_fd, &paddr, sizeof(paddr), index) != sizeof(paddr))
    {
        return -1;
    }
    paddr &= 0x7fffffffffffff;
    return (paddr << 12) | (vaddr & (4096 - 1));
}
#if defined INTEL_GEN6_4_SLICE
#define NUM_SLICES 4
const int seq_len = 0;
const int reduction_bits = 2;
const int addr_bits = 39;
const uint64_t xor_mask[2] = {0x5b5f575440ULL, 0x6eb5faa880ULL};
const int *base_sequence = NULL;
#elif defined INTEL_GEN6_ALTERNATE
#define NUM_SLICES 8
const int seq_len = 0;
const int reduction_bits = 3;
const int addr_bits = 39;
const uint64_t xor_mask[3] = {0x5b5f575440ULL, 0x6eb5faa880ULL, 0x3cccc93100ULL};
const int *base_sequence = NULL;
#elif defined INTEL_GEN13_8_SLICE || defined INTEL_GEN11_8_SLICE
#define NUM_SLICES 8
const int seq_len = 0;
const int reduction_bits = 3;
const int addr_bits = 39;
const uint64_t xor_mask[3] = {0x5b5f575440ULL, 0x71aeeb1200ULL, 0x6d87f2c00ULL};
const int *base_sequence = NULL;
#elif defined INTEL_GEN8_6_SLICE || defined INTEL_GEN9_6_SLICE
#define NUM_SLICES 6
const int seq_len = 128;
const int reduction_bits = 7;
const int addr_bits = 39;
const uint64_t xor_mask[7] = {0x21ae7be000ULL, 0x435cf7c000ULL, 0x2717946000ULL, 0x4e2f28c000ULL, 0x1c5e518000ULL, 0x38bca30000ULL, 0x50d73de000ULL};
const int base_sequence[128] = {0, 1, 2, 3, 1, 4, 3, 4, 1, 0, 3, 2, 0, 5, 2, 5, 1, 0, 3, 2, 0, 5, 2, 5, 0, 5, 2, 5, 1, 4, 3, 4, 0, 1, 2, 3, 5, 0, 5, 2, 5, 0, 5, 2, 4, 1, 4, 3, 1, 0, 3, 2, 4, 1, 4, 3, 4, 1, 4, 3, 5, 0, 5, 2, 2, 3, 0, 1, 5, 2, 5, 0, 3, 2, 1, 0, 4, 3, 4, 1, 3, 2, 1, 0, 4, 3, 4, 1, 4, 3, 4, 1, 5, 2, 5, 0, 2, 3, 0, 1, 3, 4, 1, 4, 3, 4, 1, 4, 2, 5, 0, 5, 3, 2, 1, 0, 2, 5, 0, 5, 2, 5, 0, 5, 3, 4, 1, 4};
#elif defined INTEL_GEN12_10_SLICE || defined INTEL_GEN10_10_SLICE
#define NUM_SLICES 10
const int seq_len = 128;
const int reduction_bits = 7;
const int addr_bits = 39;
const uint64_t xor_mask[7] = {0x21ae7be000ULL, 0x435cf7c000ULL, 0x2717946000ULL, 0x4e2f28c000ULL, 0x1c5e518000ULL, 0x38bca30000ULL, 0x50d73de000ULL};
const int base_sequence[128] = {0, 5, 0, 5, 3, 6, 3, 6, 1, 4, 1, 4, 2, 7, 2, 7, 1, 4, 1, 4, 2, 7, 2, 7, 0, 5, 8, 9, 3, 6, 9, 8, 4, 1, 4, 1, 7, 2, 7, 2, 5, 0, 9, 8, 6, 3, 8, 9, 5, 0, 5, 0, 6, 3, 6, 3, 4, 1, 8, 9, 7, 2, 9, 8, 4, 1, 4, 1, 7, 2, 7, 2, 5, 0, 5, 0, 6, 3, 6, 3, 5, 0, 5, 0, 6, 3, 6, 3, 8, 9, 4, 1, 9, 8, 7, 2, 0, 5, 0, 5, 3, 6, 3, 6, 9, 8, 1, 4, 8, 9, 2, 7, 1, 4, 1, 4, 2, 7, 2, 7, 8, 9, 0, 5, 9, 8, 3, 6};
#elif defined INTEL_GEN13_12_SLICE
#define NUM_SLICES 12
const int seq_len = 512;
const int reduction_bits = 9;
const int addr_bits = 39;
const uint64_t xor_mask[9] = {0x52c6a38000ULL, 0x30342b8000ULL, 0x547f480000ULL, 0x3d47f08000ULL, 0x1c5e518000ULL, 0x38bca30000ULL, 0x23bfe18000ULL, 0x0000000000ULL, 0x7368d80000ULL};
const int base_sequence[512] = {0, 1,  4, 5,  1, 8, 5, 8, 3, 2, 7, 6, 2, 11, 6, 11, 1, 0,  5, 4,  0, 9,  4, 9,  2, 11, 6, 11, 3, 10, 7, 10, 2, 3, 6, 7, 11, 2, 11, 6, 9,  0, 9,  4, 8, 1, 8, 5, 3, 2, 7, 6, 10, 3, 10, 7, 8,  1, 8,  5, 9, 0, 9, 4, 6, 7, 2, 3, 11, 6, 11, 2, 5,  4, 1,  0, 8, 5, 8, 1, 7, 6, 3, 2, 10, 7, 10, 3, 8,  5, 8,  1, 9, 4, 9, 0, 4, 5,  0, 1,  5, 8,  1, 8,  7, 10, 3, 10, 6, 11, 2, 11, 5, 4,  1, 0,  4, 9,  0, 9,  6, 11, 2, 11, 7, 10, 3, 10, 6, 11, 2, 11, 7, 6, 3, 2, 5, 8, 1, 8, 4, 5,  0, 1,  7, 10, 3, 10, 6, 7, 2, 3, 4, 9, 0, 9, 5, 8,  1, 8,  8,  5, 8,  1, 5,  4, 1,  0, 11, 6, 11, 2, 10, 7, 10, 3, 9,  4, 9,  0, 4,  5, 0,  1, 10, 7, 10, 3, 11, 6, 11, 2, 8, 1, 8, 5, 1,  0, 5,  4, 11, 2, 11, 6, 2, 3, 6, 7, 9,  0, 9,  4, 0,  1, 4,  5, 10, 3, 10, 7, 11, 2, 11, 6, 2, 11, 6, 11, 3, 2, 7, 6, 1, 8, 5, 8, 0, 9,  4, 9,  3, 10, 7, 10, 2, 3, 6, 7, 0, 9, 4, 9, 1, 8,  5, 8,
                                3, 10, 7, 10, 2, 3, 6, 7, 0, 9, 4, 9, 1, 0,  5, 4,  2, 11, 6, 11, 3, 10, 7, 10, 1, 8,  5, 8,  0, 1,  4, 5,  9, 0, 9, 4, 8,  1, 8,  5, 10, 3, 10, 7, 3, 2, 7, 6, 8, 1, 8, 5, 9,  0, 9,  4, 11, 2, 11, 6, 2, 3, 6, 7, 9, 4, 9, 0, 4,  5, 0,  1, 10, 7, 10, 3, 7, 6, 3, 2, 8, 5, 8, 1, 9,  4, 9,  0, 11, 6, 11, 2, 6, 7, 2, 3, 7, 10, 3, 10, 6, 11, 2, 11, 4, 9,  0, 9,  5, 4,  1, 0,  6, 11, 2, 11, 7, 10, 3, 10, 5, 8,  1, 8,  4, 5,  0, 1,  5, 4,  1, 0,  4, 9, 0, 9, 6, 7, 2, 3, 7, 10, 3, 10, 4, 9,  0, 9,  5, 8, 1, 8, 7, 6, 3, 2, 6, 11, 2, 11, 11, 6, 11, 2, 10, 7, 10, 3, 4,  5, 0,  1, 9,  4, 9,  0, 10, 7, 10, 3, 11, 6, 11, 2, 5,  4, 1,  0, 8,  5, 8,  1, 3, 2, 7, 6, 10, 3, 10, 7, 0,  1, 4,  5, 9, 0, 9, 4, 10, 3, 10, 7, 11, 2, 11, 6, 1,  0, 5,  4, 8,  1, 8,  5, 1, 8,  5, 8,  0, 9, 4, 9, 2, 3, 6, 7, 3, 10, 7, 10, 0, 9,  4, 9,  1, 8, 5, 8, 3, 2, 7, 6, 2, 11, 6, 11};
#elif defined INTEL_GEN14_12_SLICE
#define NUM_SLICES 12
const int seq_len = 512;
const int reduction_bits = 9;
const int addr_bits = 37;
const uint64_t xor_mask[9] = {0x2f52c6a78000ULL, 0xcb0342b8000ULL, 0x35d47f480000ULL, 0x39bd47f48000ULL, 0x109c5e518000ULL, 0x2038bca30000ULL, 0xe23bfe18000ULL, 0x0000000000ULL, 0x31f368dc0000ULL};
const int base_sequence[512] = {0, 1,  4, 5,  1, 8, 5, 8, 3, 2, 7, 6, 2, 11, 6, 11, 1, 0,  5, 4,  0, 9,  4, 9,  2, 11, 6, 11, 3, 10, 7, 10, 2, 3, 6, 7, 11, 2, 11, 6, 9,  0, 9,  4, 8, 1, 8, 5, 3, 2, 7, 6, 10, 3, 10, 7, 8,  1, 8,  5, 9, 0, 9, 4, 6, 7, 2, 3, 11, 6, 11, 2, 5,  4, 1,  0, 8, 5, 8, 1, 7, 6, 3, 2, 10, 7, 10, 3, 8,  5, 8,  1, 9, 4, 9, 0, 4, 5,  0, 1,  5, 8,  1, 8,  7, 10, 3, 10, 6, 11, 2, 11, 5, 4,  1, 0,  4, 9,  0, 9,  6, 11, 2, 11, 7, 10, 3, 10, 6, 11, 2, 11, 7, 6, 3, 2, 5, 8, 1, 8, 4, 5,  0, 1,  7, 10, 3, 10, 6, 7, 2, 3, 4, 9, 0, 9, 5, 8,  1, 8,  8,  5, 8,  1, 5,  4, 1,  0, 11, 6, 11, 2, 10, 7, 10, 3, 9,  4, 9,  0, 4,  5, 0,  1, 10, 7, 10, 3, 11, 6, 11, 2, 8, 1, 8, 5, 1,  0, 5,  4, 11, 2, 11, 6, 2, 3, 6, 7, 9,  0, 9,  4, 0,  1, 4,  5, 10, 3, 10, 7, 11, 2, 11, 6, 2, 11, 6, 11, 3, 2, 7, 6, 1, 8, 5, 8, 0, 9,  4, 9,  3, 10, 7, 10, 2, 3, 6, 7, 0, 9, 4, 9, 1, 8,  5, 8,
                                3, 10, 7, 10, 2, 3, 6, 7, 0, 9, 4, 9, 1, 0,  5, 4,  2, 11, 6, 11, 3, 10, 7, 10, 1, 8,  5, 8,  0, 1,  4, 5,  9, 0, 9, 4, 8,  1, 8,  5, 10, 3, 10, 7, 3, 2, 7, 6, 8, 1, 8, 5, 9,  0, 9,  4, 11, 2, 11, 6, 2, 3, 6, 7, 9, 4, 9, 0, 4,  5, 0,  1, 10, 7, 10, 3, 7, 6, 3, 2, 8, 5, 8, 1, 9,  4, 9,  0, 11, 6, 11, 2, 6, 7, 2, 3, 7, 10, 3, 10, 6, 11, 2, 11, 4, 9,  0, 9,  5, 4,  1, 0,  6, 11, 2, 11, 7, 10, 3, 10, 5, 8,  1, 8,  4, 5,  0, 1,  5, 4,  1, 0,  4, 9, 0, 9, 6, 7, 2, 3, 7, 10, 3, 10, 4, 9,  0, 9,  5, 8, 1, 8, 7, 6, 3, 2, 6, 11, 2, 11, 11, 6, 11, 2, 10, 7, 10, 3, 4,  5, 0,  1, 9,  4, 9,  0, 10, 7, 10, 3, 11, 6, 11, 2, 5,  4, 1,  0, 8,  5, 8,  1, 3, 2, 7, 6, 10, 3, 10, 7, 0,  1, 4,  5, 9, 0, 9, 4, 10, 3, 10, 7, 11, 2, 11, 6, 1,  0, 5,  4, 8,  1, 8,  5, 1, 8,  5, 8,  0, 9, 4, 9, 2, 3, 6, 7, 3, 10, 7, 10, 0, 9,  4, 9,  1, 8, 5, 8, 3, 2, 7, 6, 2, 11, 6, 11};
#endif

int get_address_slice(uint64_t address)
{
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
        int sequence_offset = ((uint64_t)address % (seq_len * 64)) >> 6;
        slice = base_sequence[sequence_offset ^ slice];
    }

    return slice;
}

#define GET_LLC_SET_BITS(addr) ((((uintptr_t)(addr)) >> 6) & (1024 - 1))

static size_t found = 0;
static size_t errors = 0;
static size_t duplicate = 0;
uint8_t *check_set = 0;

int CheckResult()
{
    void *x, *head;
    int cnt = 0;

    if (!check_set)
        check_set = calloc(SetsLLC, sizeof(uint8_t));

    for (int i = 0; i < MappingIdx; i++)
    {
        for (int j = 0; j < EvictionSetSize[i]; j++)
        {
            LNEXT(Mapping[i][j]) = Mapping[i][(j + 1) % EvictionSetSize[i]]; // cyclic list of lines.
        }
        for (int j = 0; j < EvictionSetSize[i]; j++)
        {
            LNEXT(OFFSET(Mapping[i][j], sizeof(void *))) = Mapping[i][(j + EvictionSetSize[i] - 1) % EvictionSetSize[i]]; // Reverse cyclic list of lines.
        }

        uint64_t paddr = virtual_to_physical((uint64_t)Mapping[i][0]);
        int test_slice = get_address_slice(paddr);
        int test_llc_set = GET_LLC_SET_BITS(paddr);

        int idx = (test_slice * (SetsLLC / NUM_SLICES)) + test_llc_set;

        found++;
        if (check_set[idx] == 2)
        {
            duplicate++;
            printf("Duplicate set %4d | Set: %4d | Slice: %d\n", idx, test_llc_set, test_slice);
        }
        else
        {
            printf("Found set %4d     | Set: %4d | Slice: %d\n", idx, test_llc_set, test_slice);
            check_set[idx]++;
        }
        for (int j = 1; j < EvictionSetSize[i]; j++)
        {
            paddr = virtual_to_physical((uint64_t)Mapping[i][j]);
            if (test_slice != get_address_slice(paddr) || test_llc_set != GET_LLC_SET_BITS(paddr))
            {
                errors++;
            }
        }

        head = Mapping[i][0];
        x = LNEXT(NEXTPTR(head));

        if (!checkEviction(head, x, x))
        {
            cnt++;
        }
    }
    printf("Found: %ld\n", found);
    printf("Duplicate: %ld\n", duplicate);
    printf("Total: %ld\n", SetsLLC / 64);
    printf("Errors: %ld\n", errors);

    return cnt;
}

Struct InitData(int N_c, int N_R, uint64_t offset)
{
    // Init data structure
    Struct addresses;
    void **candidates = (void **)malloc(N_c * sizeof(void *));
    void **Representatives = (void **)malloc(N_R * sizeof(void *));
    addresses.N_c = N_c;
    addresses.N_R = N_R;

    // Collect addresses.
    for (int i = 0; i < N_c; i++)
        candidates[i] = CandAddressesPool + i * Stride + offset;
    for (int i = 0; i < N_R; i++)
        Representatives[i] = RepAddressesPool + i * Stride + offset;

    // Cyclic lists.
    for (int i = 0; i < N_c; i++)
    {
        LNEXT(candidates[i]) = candidates[(i + 1) % N_c];
        LNEXT(OFFSET(candidates[i], sizeof(void *))) = candidates[(i + N_c - 1) % N_c];
        if (i < N_R)
        {
            LNEXT(Representatives[i]) = Representatives[(i + 1) % N_R];
            LNEXT(OFFSET(Representatives[i], sizeof(void *))) = Representatives[(i + N_R - 1) % N_R];
        }
    }

    addresses.candidates = candidates[0];
    addresses.Representatives = Representatives[0];
    // flush(addresses.candidates);
    // flush(addresses.Representatives);
    free(candidates);
    free(Representatives);
    return addresses;
}

void collectEvictionSet(Struct addresses)
{
    void *p;
    Mapping[MappingIdx] = (void **)malloc((addresses.N_c + addresses.N_R) * sizeof(void *));
    EvictionSetSize[MappingIdx] = addresses.N_c + addresses.N_R;
    p = addresses.candidates;
    for (int i = 0; i < addresses.N_c; i++)
    {
        Mapping[MappingIdx][i] = p;
        p = LNEXT(p);
    }
    p = addresses.Representatives;
    for (int i = 0; i < addresses.N_R; i++)
    {
        Mapping[MappingIdx][i + addresses.N_c] = p;
        p = LNEXT(p);
    }
    // for(int i=0;i<EvictionSetSize[MappingIdx];i++) clflush(Mapping[MappingIdx][i]);
    MappingIdx++;
}

int checkEviction(void *head, void *x, void *pp)
{ // PP-> if checkResult pp=x, if reduction pp=head
    void *p;
    Prime(head, FW);
    flush(head);
    memaccesstime((void *)x);
    memaccesstime((void *)x);
    memaccesstime((void *)x);
    for (int i = 0; i < 3; i++)
    {
        p = head;
        do
        {
            gaurd();
            p = LNEXT(p);
        } while (p != (void *)pp);
    }
    if (memaccesstime((void *)x) > THRESHOLD)
    {
        // flush(head);
        return 1;
    }
    else
    {
        // flush(head);
        return 0;
    }
}

void printMapping()
{
    for (int set = 0; set < MappingIdx; set++)
    {
        printf("Eviction set %d:\n", set);
        for (int i = 0; i < EvictionSetSize[set]; i++)
            printf("%d) Add: %p  set: %ld\n", i, Mapping[set][i], (intptr_t)LNEXT(OFFSET(Mapping[set][i], 3 * sizeof(void *))));
    }
}

void *getMappingHead()
{
    if (MappingIdx == 0)
        return NULL;
    void **EvictionSets = (void **)malloc(MappingIdx * W * sizeof(void *));
    void *tmp;
    for (int set = 0; set < MappingIdx; set++)
        for (int line = 0; line < W; line++)
            EvictionSets[set * W + line] = Mapping[set][line];
    for (int j = 0; j < MappingIdx * W; j++)
        LNEXT(EvictionSets[j]) = EvictionSets[(j + 1) % (MappingIdx * W)]; // cyclic list of lines.
    for (int j = 0; j < MappingIdx * W; j++)
        LNEXT(OFFSET(EvictionSets[j], sizeof(void *))) = EvictionSets[(j + (MappingIdx * W) - 1) % (MappingIdx * W)]; // Reverse cyclic list of lines.
    tmp = EvictionSets[0];
    // flush(tmp);
    free(EvictionSets);
    return tmp;
}

Probe_Args remove_congrunt_addresses(void *head, int size)
{
    Probe_Args probe_args;
    void *tail, *MappingHead = getMappingHead();
    char *MissHit = (char *)calloc(size, sizeof(char));
    int NumExp = 4;

    tail = LNEXT(NEXTPTR(head));
    PruneInfo(head, tail, MissHit, NumExp, size, MappingHead);
    probe_args = probe(head, size, MissHit);
    printf("Removed %d addresses\n", probe_args.N2);
    free(MissHit);
    return probe_args;
}

Struct prepareForMapping(uint64_t offset)
{
    Struct addresses;
    srand(time(NULL));
    float p = 1 / (float)S;
    int N_c = S;
    int N_R = ceil(log(0.01) / log(1 - p)); // 0.01 => tolerance/100
    while (probabilityToBeEvictionSet(p, N_c) < 0.99)
        N_c = N_c + 4 * W; // 0.99 => LLC_Cover/100

    // 3 Times LLC size
    N_c = 3 * S * W;

    printf("The size of the candidate set is: %.5f times the size of the LLC (S*w)\n", N_c / (float)(S * W));
    printf("The size of the Representatives set is: %.5f  times the number of sets (S)\n", N_R / (float)S);

    Mapping = (void ***)malloc(S * sizeof(void **));
    EvictionSetSize = (int *)malloc(S * sizeof(int));
    MappingIdx = 0;

    // Collect pool of addresses
    CandAddressesPool = mmap(NULL, N_c * Stride, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0); // Maybe use (void*)(intptr_t)rand() instead of NULL
    RepAddressesPool = mmap(NULL, N_R * Stride, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);  // Maybe use (void*)(intptr_t)rand() instead of NULL
    addresses = InitData(N_c, N_R, offset);

    GarbageCands = (void **)malloc(addresses.N_c * sizeof(void *));
    GarbageReps = (void **)malloc(addresses.N_R * sizeof(void *));

    return addresses;
}

void statistics(int NumExp, int AVGmappingSize, double AVGtime)
{
    double AVGtime_array[BufferSize] = {0}, AVGmappingSize_array[BufferSize] = {0};

    for (int k = 0; k < BufferSize; k++)
    {
        AVGtime_array[k] = (double)time_array[k] / (CLOCKS_PER_SEC * NumExp);
        AVGmappingSize_array[k] = (double)mappingSize_array[k] / NumExp;
        printf("%d). time: %f  mapping size: %f\n", k, AVGtime_array[k], AVGmappingSize_array[k]);
    }

    AVGmappingSize /= NumExp;
    AVGtime /= NumExp;
    printf("AVG mapping size: %d\n", AVGmappingSize);
    printf("AVG mapping time: %f\n", AVGtime);
}
