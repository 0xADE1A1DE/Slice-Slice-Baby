#include "util.h"

int open_physical_address_ptr(ADDRESS_PTR_T *address_ptr)
{
    long page_size = sysconf(_SC_PAGESIZE);
    off_t page_base = address_ptr->address & ~(page_size - 1);

    address_ptr->mem_fd = open("/dev/mem", O_RDWR);
    if (address_ptr->mem_fd == -1)
    {
        perror("open");
        return 1;
    }

    address_ptr->mem = mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_SHARED, address_ptr->mem_fd, page_base);
    if (address_ptr->mem == MAP_FAILED)
    {
        close(address_ptr->mem_fd);
        return 1;
    }

    address_ptr->ptr = address_ptr->mem + (address_ptr->address & (page_size - 1));

    return 0;
}

int close_physical_address_ptr(ADDRESS_PTR_T *address_ptr)
{
    long page_size = sysconf(_SC_PAGESIZE);
    munmap(address_ptr->mem, page_size);
    close(address_ptr->mem_fd);

    return 0;
}

void update_xor_mask(uint64_t **xor_mask, size_t *reduction_bits, int addr_bit, int slice)
{
    // Increase xor_mask array if needed by checking the maximum bit of the recovered slice for this bit
    size_t max_bit = find_set_bit((uint64_t)slice) + 1;
    if (*reduction_bits < max_bit)
    {
        *xor_mask = realloc(*xor_mask, (max_bit) * sizeof(uint64_t));
        for (int i = *reduction_bits; i < max_bit; ++i)
            *(*xor_mask + i) = 0;
        *reduction_bits = max_bit;
    }
    // Update xor_mask array
    for (int b = 0; b < *reduction_bits; b++)
    {
        if (is_bit_k_set(slice, b))
            *(*xor_mask + b) |= 1ULL << addr_bit;
    }
}

void print_xor_mask(uint64_t *xor_mask, int reduction_bits)
{
    for (int i = 0; i < reduction_bits; i++)
    {
        if (i < reduction_bits - 1)
            printf("%lx, ", xor_mask[i]);
        else
            printf("%lx\n", xor_mask[i]);
    }
}

void print_base_sequence(int *base_sequence, size_t seq_len)
{
    if (seq_len > 1)
    {
        for (int i = 0; i < seq_len; i++)
        {
            if (i < seq_len - 1)
                printf("%d, ", base_sequence[i]);
            else
                printf("%d\n", base_sequence[i]);
        }
    }
}

// int get_address_slice(uncore_perfmon_t *u, uint64_t address)
// {
// 	int64_t samples = UNCORE_PERFMON_SAMPLES;
// 	ADDRESS_PTR_T address_ptr = {address, NULL, NULL, -1};
// 	uint8_t num_cbos = uncore_get_num_cbo(AFFINITY);
// 	double *data = calloc(num_cbos, sizeof(double));
// 	int slice = -2;
// 	int slice_found = 0;
// 	int wrong_slice_found = 0;

// 	//-1 means we have hit the memory limit for this processor.
// 	if(open_physical_address_ptr(&address_ptr))
// 	{
// 		slice = -1;
// 	}
// 	else
// 	{
// 		while(slice == -2)
// 		{
// 			uncore_perfmon_monitor(u, clflush, address_ptr.ptr, NULL);

// 			for (int s = 0; s < u->num_cbo_ctrs; ++s)
// 				data[s] = (double)u->results[s].total/(double)samples;
// 			double mean = calculate_mean(data, num_cbos);
// 			double stddev = calculate_stddev(data, num_cbos, mean);

// 			for (int s = 0; s < u->num_cbo_ctrs; ++s)
// 			{
// 				double zscore = calculate_zscore(data[s], mean, stddev);
// 				if(zscore > 2.0 && data[s] > 1.0)
// 				{
// 					slice = s;
// 				}
// 				if(data[s] > 1.0)
// 				{
// 					slice_found++;
// 				}
// 				if(zscore < 0.0)
// 				{
// 					wrong_slice_found++;
// 				}
// 			}
// 			if(slice_found != 1 || wrong_slice_found != num_cbos-1)
// 			{
// 				samples += UNCORE_PERFMON_SAMPLES;
// 				uncore_perfmon_change_samples(u, samples);
// 				slice = -2;
// 				slice_found = 0;
// 				wrong_slice_found = 0;
// 			}
// 		}
// 	}
// 	free(data);
// 	close_physical_address_ptr(&address_ptr);
// 	return slice;
// }

int get_address_slice_sim2(uint64_t address)
{
    int seq_len = 512;
    int reduction_bits = 9;
    int addr_bits = 39;
    uint64_t xor_mask[9] = {0x52c6a38000ULL, 0x30342b8000ULL, 0x547f480000ULL, 0x3d47f08000ULL, 0x1c5e518000ULL, 0x38bca30000ULL, 0x23bfe18000ULL, 0x0000000000ULL, 0x7368d80000ULL};
    int base_sequence[512] = {0, 1,  4, 5,  1, 8, 5, 8, 3, 2, 7, 6, 2, 11, 6, 11, 1, 0,  5, 4,  0, 9,  4, 9,  2, 11, 6, 11, 3, 10, 7, 10, 2, 3, 6, 7, 11, 2, 11, 6, 9,  0, 9,  4, 8, 1, 8, 5, 3, 2, 7, 6, 10, 3, 10, 7, 8,  1, 8,  5, 9, 0, 9, 4, 6, 7, 2, 3, 11, 6, 11, 2, 5,  4, 1,  0, 8, 5, 8, 1, 7, 6, 3, 2, 10, 7, 10, 3, 8,  5, 8,  1, 9, 4, 9, 0, 4, 5,  0, 1,  5, 8,  1, 8,  7, 10, 3, 10, 6, 11, 2, 11, 5, 4,  1, 0,  4, 9,  0, 9,  6, 11, 2, 11, 7, 10, 3, 10, 6, 11, 2, 11, 7, 6, 3, 2, 5, 8, 1, 8, 4, 5,  0, 1,  7, 10, 3, 10, 6, 7, 2, 3, 4, 9, 0, 9, 5, 8,  1, 8,  8,  5, 8,  1, 5,  4, 1,  0, 11, 6, 11, 2, 10, 7, 10, 3, 9,  4, 9,  0, 4,  5, 0,  1, 10, 7, 10, 3, 11, 6, 11, 2, 8, 1, 8, 5, 1,  0, 5,  4, 11, 2, 11, 6, 2, 3, 6, 7, 9,  0, 9,  4, 0,  1, 4,  5, 10, 3, 10, 7, 11, 2, 11, 6, 2, 11, 6, 11, 3, 2, 7, 6, 1, 8, 5, 8, 0, 9,  4, 9,  3, 10, 7, 10, 2, 3, 6, 7, 0, 9, 4, 9, 1, 8,  5, 8,
                              3, 10, 7, 10, 2, 3, 6, 7, 0, 9, 4, 9, 1, 0,  5, 4,  2, 11, 6, 11, 3, 10, 7, 10, 1, 8,  5, 8,  0, 1,  4, 5,  9, 0, 9, 4, 8,  1, 8,  5, 10, 3, 10, 7, 3, 2, 7, 6, 8, 1, 8, 5, 9,  0, 9,  4, 11, 2, 11, 6, 2, 3, 6, 7, 9, 4, 9, 0, 4,  5, 0,  1, 10, 7, 10, 3, 7, 6, 3, 2, 8, 5, 8, 1, 9,  4, 9,  0, 11, 6, 11, 2, 6, 7, 2, 3, 7, 10, 3, 10, 6, 11, 2, 11, 4, 9,  0, 9,  5, 4,  1, 0,  6, 11, 2, 11, 7, 10, 3, 10, 5, 8,  1, 8,  4, 5,  0, 1,  5, 4,  1, 0,  4, 9, 0, 9, 6, 7, 2, 3, 7, 10, 3, 10, 4, 9,  0, 9,  5, 8, 1, 8, 7, 6, 3, 2, 6, 11, 2, 11, 11, 6, 11, 2, 10, 7, 10, 3, 4,  5, 0,  1, 9,  4, 9,  0, 10, 7, 10, 3, 11, 6, 11, 2, 5,  4, 1,  0, 8,  5, 8,  1, 3, 2, 7, 6, 10, 3, 10, 7, 0,  1, 4,  5, 9, 0, 9, 4, 10, 3, 10, 7, 11, 2, 11, 6, 1,  0, 5,  4, 8,  1, 8,  5, 1, 8,  5, 8,  0, 9, 4, 9, 2, 3, 6, 7, 3, 10, 7, 10, 0, 9,  4, 9,  1, 8, 5, 8, 3, 2, 7, 6, 2, 11, 6, 11};

    if ((address >> (addr_bits + 1)) > 0)
    {
        return -1;
    }

    int slice = 0;
    for (int b = 0; b < reduction_bits; ++b)
    {
        slice |= ((uint64_t)(count_set_bits(xor_mask[b] & address) % 2) << b);
    }

    if (seq_len > 1)
    {
        int sequence_offset = ((uint64_t)address % (seq_len * 64)) >> 6;
        slice = base_sequence[sequence_offset ^ slice];
    }

    return slice;
}

int get_address_slice(uncore_perfmon_t *u, uint64_t address)
{
    ADDRESS_PTR_T address_ptr = {address, NULL, NULL, -1};
    uint8_t num_cbos = uncore_get_num_cbo(AFFINITY);
    int slice = -2;
    int slice_found = 0;

    //-1 means we have hit the memory limit for this processor.
    if (open_physical_address_ptr(&address_ptr))
    {
        return -1;
    }

    uncore_perfmon_monitor(u, clflush, address_ptr.ptr, NULL);

    uint64_t max = 0;
    for (int s = 0; s < u->num_cbo_ctrs; ++s)
    {
        if ((double)u->results[s].total / (double)UNCORE_PERFMON_SAMPLES >= 1.0)
        {
            slice = s;
            slice_found++;
            if (slice_found > 1)
            {
                slice = -2;
                break;
            }
        }
    }
    close_physical_address_ptr(&address_ptr);

    return slice;
}

int get_address_slice_sim(uint64_t address, uint64_t *xor_mask, int reduction_bits, int addr_bits, int *base_sequence, size_t seq_len)
{
    if ((address >> addr_bits + 1) > 0)
    {
        return -1;
    }

    int slice = 0;
    for (int b = 0; b < reduction_bits; ++b)
    {
        slice |= ((uint64_t)(count_set_bits(xor_mask[b] & address) % 2) << b);
    }

    if (seq_len > 1)
    {
        int sequence_offset = ((uint64_t)address % (seq_len * L3_CACHELINE)) >> 6;
        slice = base_sequence[sequence_offset ^ slice];
    }

    return slice;
}

int get_sim_params(char const *argv[], size_t *num_cbos, uint64_t **xor_mask, size_t *reduction_bits, size_t *addr_bits, int **base_sequence, size_t *seq_len)
{
    FILE *sim_file;
    char *line = NULL;
    size_t line_length = 0;
    size_t line_count = 0;

    sim_file = fopen(argv[2], "r");

    if (sim_file == NULL)
    {
        fprintf(stderr, "Failed to open the file.\n");
        return 1;
    }

    fprintf(stderr, "Simulating using: %s\n", argv[2]);

    char *token;
    char delimiter[] = ",";
    while (getline(&line, &line_length, sim_file) != -1)
    {
        switch (line_count)
        {
        case 0:
            *num_cbos = atoi(line);
            break;
        case 1:
            *addr_bits = atoi(line);
            break;
        case 2:
            token = strtok(line, delimiter);
            while (token != NULL)
            {
                (*reduction_bits)++;
                *xor_mask = realloc(*xor_mask, *reduction_bits * sizeof(uint64_t));
                *(*xor_mask + (*reduction_bits - 1)) = strtoll(token, NULL, 16);
                token = strtok(NULL, delimiter);
            }
            break;
        case 3:
            if (!is_power_of_two(*num_cbos))
            {
                token = strtok(line, delimiter);
                while (token != NULL)
                {
                    (*seq_len)++;
                    *base_sequence = realloc(*base_sequence, *seq_len * sizeof(int));
                    *(*base_sequence + (*seq_len - 1)) = strtoll(token, NULL, 10);
                    token = strtok(NULL, delimiter);
                }
            }
            break;
        default:
            break;
        }
        line_count++;
    }
    free(line);
    fclose(sim_file);

    fprintf(stderr, "sim_num_cbos: %ld\n", *num_cbos);
    fprintf(stderr, "sim_addr_bits: %ld\n", *addr_bits);
    fprintf(stderr, "sim_xor_mask: ");
    for (int i = 0; i < *reduction_bits; ++i)
        fprintf(stderr, "%lx, ", *(*xor_mask + i));
    fprintf(stderr, "\n");

    if (seq_len > 0)
    {
        fprintf(stderr, "sim_base sequence: ");
        for (int i = 0; i < *seq_len; ++i)
            fprintf(stderr, "%d, ", *(*base_sequence + i));
    }

    fprintf(stderr, "\n\n");
    return 0;
}

static struct timeval stop, start;
void start_timer()
{
    gettimeofday(&start, NULL);
}

double stop_timer()
{
    gettimeofday(&stop, NULL);
    return ((stop.tv_sec - start.tv_sec) * 1000000ULL + stop.tv_usec - start.tv_usec) / (double)1000000ULL;
}
