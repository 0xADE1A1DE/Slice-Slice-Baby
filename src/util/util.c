#include "util/util.h"
#include "evsets/evsets.h"
#include "slice_partitioning/slicing.h"

#ifndef L2_EVSET_ALGORITHM
#define L2_EVSET_ALGORITHM GROUP_TESTING
#endif

char *get_address_state_string(address_state state)
{
    switch (state)
    {
    case L1:
        return "L1";
        break;
    case L2:
        return "L2";
        break;
    case LLC:
        return "LLC";
        break;
    case RAM:
        return "RAM";
        break;
    case RAM_SMALLPAGE:
        return "RAM_SMALLPAGE";
        break;
    default:
        return "UNKNOWN_CACHE";
        break;
    }
}

int compare(const void *a, const void *b)
{
    uint32_t *x = (uint32_t *)a;
    uint32_t *y = (uint32_t *)b;
    return (int)(*x - *y);
}

int compare_double(const void *a, const void *b)
{
    return (*(double *)a > *(double *)b) - (*(double *)a < *(double *)b);
}

double calculate_mean(uint32_t *data, uint32_t len)
{
    double sum = 0.0;
    for (uint32_t i = 0; i < len; ++i)
    {
        sum += data[i];
    }
    return (sum / (double)len);
}

double calculate_stddev(uint32_t *data, uint32_t len, double mean)
{
    double stddev = 0.0;
    for (uint32_t i = 0; i < len; ++i)
    {
        stddev += pow(data[i] - mean, 2);
    }
    return sqrt(stddev / len);
}

double calculate_zscore(double datum, double mean, double stddev)
{
    return ((datum - mean) / stddev);
}

void *initialise_memory(size_t size, size_t extra_flags, size_t page_size)
{
    size_t flags = MAP_PRIVATE | MAP_ANONYMOUS;
    switch (page_size)
    {
    case SMALLPAGE:
        break;
    case HUGEPAGE:
        flags |= MAP_HUGETLB | MAP_HUGE_2MB;
        break;
    case 1 * GB:
        flags |= MAP_HUGETLB | MAP_HUGE_1GB;
        break;
    default:
        printf("Error: initialise_memory(), please use page size of either 4KB, 2MB or 1GB.\n");
        exit(1);
    }

    void *memory = mmap(NULL, size, PROT_READ | PROT_WRITE, flags | extra_flags, -1, 0);

    // Check for errors using MAP_FAILED
    if (memory == MAP_FAILED)
    {
        perror("Error: mmap");
        exit(1);
    }

    memset(memory, 0x0, size);

    return memory;
}

void print_addr_info(uintptr_t address)
{
    uintptr_t paddress = virtual_to_physical(address);
    printf("0x%lx | L1: %2ld | L2: %4ld | LLC: %4ld | Slice: %d | CSlice: %d\n", paddress, GET_L1D_SET_BITS(paddress), GET_L2_SET_BITS(paddress), GET_LLC_SET_BITS(paddress), get_address_slice(paddress), ((elem_t *)address)->cslice);
}

static uint8_t *evict = NULL;
static cache_evsets_t *l2_evsets;

void reset_set_addr_state_l2_evset()
{
    printf("reset_set_addr_state_evset(): resetting L2 eviction sets...\n");
    do
    {
        l2_evsets = evsets_create(L2, L2_EVSET_ALGORITHM, L2_EVSET_FLAGS, 0, L2_CANDIDATE_SET_SIZE, L2_REDUCTIONS, NULL, 0);
        if (l2_evsets->evsets_count[0] < l2_evsets->evsets_per_offset)
            evsets_release(l2_evsets);
        else
            break;
    } while (1);
    printf("reset_set_addr_state_evset(): resetting L2 eviction sets succeeded\n");
}

inline void set_addr_state(uintptr_t address, address_state to_state)
{
    size_t len = ((LLC_SETS / L1D_SETS) * LLC_ASSOCIATIVITY) * LLC_SLICES * PAGE_SIZE;
    if (evict == NULL)
    {
        printf("set_addr_state(): initialise_memory with len %lu\n", len);
        evict = (uint8_t *)initialise_memory(len, 0, PAGE_SIZE);
    }

    switch (to_state)
    {
    case L1:
        memaccess((void *)address);
        break;
    case LLC:
#if PAGE_SIZE == SMALLPAGE
        // Find L2 evset for address
        if (((elem_t *)address)->l2_evset == NULL)
        {
            if (l2_evsets == NULL)
            {
                do
                {
                    l2_evsets = evsets_create(L2, L2_EVSET_ALGORITHM, L2_EVSET_FLAGS, 0, L2_CANDIDATE_SET_SIZE, L2_REDUCTIONS, evict, len);
                    if (l2_evsets->evsets_count[0] < l2_evsets->evsets_per_offset)
                    {
                        fprintf(stderr, "set_addr_state(): Failed to build L2 eviction sets, exiting\n");
                        exit(1);
                    }
                    else
                        break;
                } while (1);
            }
            ((elem_t *)address)->l2_evset = evset_find_for_address(l2_evsets, (void *)address);
        }
        L2_TRAVERSE(((elem_t *)address)->l2_evset->cs, ((elem_t *)address)->l2_evset->size, L2_TRAVERSE_REPEATS);
#elif PAGE_SIZE == HUGEPAGE
        // for (int r = 0; r < 4; r++)
        // {
        //     for (int i = 0; i < L1D_ASSOCIATIVITY + L2_ASSOCIATIVITY + 20; i++)
        //     {
        //         memaccess((void *)(evict + (i * L2_STRIDE) + (address % L2_STRIDE)));
        //     }
        // }
        for (int r = 0; r < 4; r++)
        {
            for (int i = L2_ASSOCIATIVITY; i >= 0; i--)
            {
                memaccess((void *)(evict + (i * L2_STRIDE) + (address % L2_STRIDE)));
            }
            for (int i = L1D_ASSOCIATIVITY; i >= 0; i--)
            {
                memaccess((void *)(evict + (i * L1D_STRIDE) + (address % L1D_STRIDE)));
            }
        }
#else
        fprintf(stderr, "set_addr_state(): PAGE_SIZE not set correctly");
        exit(1);
#endif
        break;
    case RAM:
        clflush((void *)address);
        break;
    case RAM_SMALLPAGE:
        if (((elem_t *)address)->llc_evset == NULL)
        {
            uintptr_t address_offset = virtual_to_physical(address % L1D_STRIDE);
            int target_slice = get_address_slice(virtual_to_physical((uint64_t)address));
            int target_llc_set = GET_LLC_SET_BITS(virtual_to_physical((uint64_t)address));

#if IS_INCLUSIVE == 1
            size_t size = LLC_ASSOCIATIVITY * 2;
#else
            size_t size = (LLC_ASSOCIATIVITY + 1) * 6;
#endif

            evset_t *llc_evset = calloc(1, sizeof(evset_t));
            llc_evset->cs = calloc(size, sizeof(elem_t *));

            size_t c = 0;
            // Precompute the slices for each eviction address for quick lookup later without polluting cache
            for (size_t i = 0; c < size; ++i)
            {
                int slice = get_address_slice(virtual_to_physical((uint64_t)(evict + (i * L1D_STRIDE) + address_offset)));
                int llc_set = GET_LLC_SET_BITS(virtual_to_physical((uint64_t)(evict + (i * L1D_STRIDE) + address_offset)));
                if (slice == target_slice && llc_set == target_llc_set)
                {
                    llc_evset->cs[c] = (elem_t *)(evict + (i * L1D_STRIDE) + address_offset);
                    llc_evset->size++;
                    c++;
                }
            }

            ((elem_t *)address)->llc_evset = llc_evset;
        }
#if IS_INCLUSIVE == 1
        LLC_TRAVERSE(((elem_t *)address)->llc_evset->cs, ((elem_t *)address)->llc_evset->size, LLC_TRAVERSE_REPEATS);
#else
        LLC_TRAVERSE(((elem_t *)address)->llc_evset->cs, ((elem_t *)address)->llc_evset->size, ((elem_t *)address)->l2_evset);
#endif
        break;
    default:
        break;
    }
}

static int pagemap_fd = -1;
uint64_t virtual_to_physical(uint64_t vaddr)
{
    if (pagemap_fd == -1)
        pagemap_fd = open("/proc/self/pagemap", O_RDONLY);

    unsigned long paddr = -1;
    unsigned long index = (vaddr / SMALLPAGE) * sizeof(paddr);
    if (pread(pagemap_fd, &paddr, sizeof(paddr), index) != sizeof(paddr))
    {
        return -1;
    }
    paddr &= 0x7fffffffffffff;
    return (paddr << PAGE_BITS) | (vaddr & (SMALLPAGE - 1));
}

struct timeval start_timer()
{
    struct timeval timer;
    gettimeofday(&timer, NULL);
    return timer;
}

double stop_timer(struct timeval timer)
{
    struct timeval stop;
    gettimeofday(&stop, NULL);
    double res = (double)((stop.tv_sec - timer.tv_sec) * 1000000ULL + (stop.tv_usec - timer.tv_usec)) / 1000000.0;
    return res;
}