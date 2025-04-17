#include <stdint.h>

#include <assemblyline.h>

#include "evsets/evsets.h"
#include "slice_partitioning/comparator_gate.h"
#include "slice_partitioning/decision_tree.h"
#include "slice_partitioning/slicing.h"

assemblyline_t al;

int (*comparator_weird_gate)(void *, void *, void *);
elem_t **measuring_sticks;
elem_t **signals;

// From https://arxiv.org/pdf/2303.00122.pdf
// Exploits the return stack buffer which holds return target predictions.
// By modifying the current value pointed to by RSP, we can overwrite our original return by
// creating a data dependency and therefore execute instructions we shouldn't.
// This gadget shows that it works by accessing a memory address in the speculated branch.
char *create_comparator_code()
{
    // Dynamically allocate memory for the instruction chain
    size_t instruction_chain_size = 16384;
    char *instruction_chain = malloc(instruction_chain_size);
    if (!instruction_chain)
    {
        perror("Failed to allocate memory for instruction_chain");
        exit(EXIT_FAILURE);
    }
    memset(instruction_chain, 0, instruction_chain_size);

    for (int i = 0; i < CG_BALANCER_DELAY; i++)
    {
        strncat(instruction_chain, "    add r10, rsi\n", instruction_chain_size - strlen(instruction_chain) - 1);
    }

    // Dynamically allocate memory for the comparator code string
    size_t comparator_code_size = 16384;
    char *comparator_code_str = malloc(comparator_code_size);
    if (!comparator_code_str)
    {
        perror("Failed to allocate memory for comparator_code_str");
        free(instruction_chain);
        exit(EXIT_FAILURE);
    }

    // Prepare the comparator code string with the instruction chain
    snprintf(comparator_code_str, comparator_code_size,
             "    xor rdi, 0x800\n"
             "    mov rax, [rdi]\n" // Make sure page for *input is in TLB
             "    xor rdi, 0x800\n"
             "    xor rsi, 0x800\n"
             "    mov rax, [rsi]\n" // Make sure page for *compare is in TLB
             "    xor rsi, 0x800\n"
             "    mov r10, rdx\n" // Get C/RDX into R10 for use later
             "    mfence\nlfence\n"
             "    call long 0x%x\n"
             "    mov rsi, [rsi]\n" // Access *compare now has 0 in it
             "%s"                   // Insert the instruction chain here
             "    mov r10, [r10]\n" // Access *signal
             "    lfence\n"         // Stops speculation here

             "    mov rax, 0x%x\n"
             "    add rax, [rdi]\n"
             "    add [rsp], rax\n"
             "    ret\n"

             "    lfence\n"
             "    xor r10, 0x800\n"
             "    mov rax, [r10]\n" // Make sure page for address *signal is in TLB
             "    xor r10, 0x800\n"
             "%s"
             "    mfence\nlfence\n"
             "    rdtscp\n"
             "    mov r9, rax\n"
             "    add r10, [r10]\n" // Access *signal held in RDX earlier
             "    rdtscp\n"
             "    sub rax, r9\n"
             "    ret\n",
             0x9 + (CG_BALANCER_DELAY * 0x3), instruction_chain, 0x16 + (CG_BALANCER_DELAY * 0x3),
             ""); // Any additional string can go here

    char *comparator_gate_str;
    if (CG_SIGNAL_EVICT == LLC)
    {
        comparator_gate_str = malloc(strlen(comparator_code_str) + 1);
        if (!comparator_gate_str)
        {
            perror("Failed to allocate memory for comparator_gate_str");
            free(comparator_code_str);
            free(instruction_chain);
            exit(EXIT_FAILURE);
        }
        snprintf(comparator_gate_str, strlen(comparator_code_str) + 1, "%s", comparator_code_str);
    }
    else if (CG_SIGNAL_EVICT == RAM)
    {
        char *loop_str = "    mov rcx, 1000\n"
                         "LOOP_START1:\n"
                         "    dec rcx\n"
                         "    jnz LOOP_START1\n";

        comparator_gate_str = malloc(strlen(comparator_code_str) + strlen(loop_str) + 1);
        if (!comparator_gate_str)
        {
            perror("Failed to allocate memory for comparator_gate_str");
            free(comparator_code_str);
            free(instruction_chain);
            exit(EXIT_FAILURE);
        }
        snprintf(comparator_gate_str, strlen(comparator_code_str) + strlen(loop_str) + 1, comparator_code_str, CG_BALANCER_DELAY, loop_str);
    }
    else
    {
        printf("create_comparator_code(): Please select CG_SIGNAL_EVICT as LLC or RAM\n");
        free(comparator_code_str);
        free(instruction_chain);
        exit(1);
    }

    // Free dynamically allocated strings after use
    free(comparator_code_str);
    free(instruction_chain);

    return comparator_gate_str;
}

void init_comparator_gate()
{
#if VERBOSITY >= 0
    struct timeval timer = start_timer();
#endif
    al = asm_create_instance(NULL, 0);
    char *comparator_gate_str = create_comparator_code();
    asm_assemble_str(al, comparator_gate_str);
    comparator_weird_gate = (int (*)(void *, void *, void *))(asm_get_code(al));
    free(comparator_gate_str);
#if VERBOSITY >= 0
    printf("init_comparator_gate(): took %.3fms\n", stop_timer(timer) * 1000);
#endif
}

size_t init_measuring_sticks(void *mem, size_t len)
{
    if (len < PAGE_SIZE)
    {
        fprintf(stderr, "init_measuring_sticks(): length of buffer less than PAGE_SIZE\n");
        exit(1);
    }

#if VERBOSITY >= 1
    struct timeval start = start_timer();
#endif

    // This tells us how many pages we had to go into mem to find all slices. Some non-linear functions do not have every slice in a single 4KB page.
    // For linear functions we only need one page to find all slices.
    // We use this to inform the calibration how many addresses we have to run kmeans on, as this is how many we will know the slices for.
    size_t max_len = 0;

    if (measuring_sticks != NULL)
        free(measuring_sticks);
    if (signals != NULL)
        free(signals);

    measuring_sticks = calloc(LLC_SLICES, sizeof(elem_t *));
    signals = calloc(LLC_SLICES, sizeof(uint64_t *));

    set_addr_state((uintptr_t)mem, LLC);

    DecisionTreeNode_t *dt = get_decision_tree();

    // If we have a linear slice function, we can instantly select addresses which go to a different slice
    if (__builtin_popcountll(LLC_SLICES) == 1)
    {

        int count = 0;
        for (int cl = 0; cl < CL_PER_PAGE; ++cl)
        {
            int unique_count[LLC_SLICES] = {0};
            for (int i = 0; i < dt->unique_pages_count; ++i)
            {
                int first_slice = dt->unique_pages[i][cl];
                unique_count[first_slice]++;
            }

            int all_unique = 1;
            for (int s = 0; s < LLC_SLICES; ++s)
            {
                if (unique_count[s] != 1)
                {
                    all_unique = 0;
                }
            }
            if (all_unique == 1)
            {
                count++;
            }
        }

        if (count == CL_PER_PAGE)
        {
            for (int s = 0; s < LLC_SLICES; ++s)
            {
                for (int cl = 0; cl < CL_PER_PAGE; ++cl)
                {
                    if (s == 0)
                    {
                        ((elem_t *)((uintptr_t)mem + (cl * CACHELINE)))->cslice = dt->unique_pages[0][cl];
                    }
                    if (dt->unique_pages[0][cl] == s)
                    {
                        measuring_sticks[s] = (elem_t *)((uintptr_t)mem + (cl * CACHELINE));
                    }
                }
            }
        }
        max_len = PAGE_SIZE;
    }
    else
    {
        // Use the decision tree to choose the first element as literally the first element, then measure a different one.
        // Use the comparator gate to tell us when something is not the other as the comparator gate is a <= b logic.
        // Use this to narrow down the tree until we have only one permutation, apply to the entire page and then get the slices.
        // If we do not have enough slices then go to the next page.

        int8_t results[CL_PER_PAGE * CL_PER_PAGE] = {0};

        for (size_t i = 0; i < CL_PER_PAGE; i++)
        {
            elem_t *temp1 = ((elem_t *)((uintptr_t)mem + (i * CACHELINE)));
            for (size_t j = 0; j < CL_PER_PAGE; j++)
            {
                if (i == j)
                {
                    results[(i * CL_PER_PAGE) + j] = 1;
                    continue;
                }

                elem_t *temp2 = ((elem_t *)((uintptr_t)mem + (j * CACHELINE)));
                elem_t *signal = ((elem_t *)((uintptr_t)mem + (j * CACHELINE) + CACHELINE));

                int signal_accesses = 0;
                int error = 0;
                for (int s = 0; s < LLC_SLICES * 2; ++s)
                {
                    set_addr_state((uintptr_t)temp1, LLC);
                    set_addr_state((uintptr_t)temp2, LLC);
                    set_addr_state((uintptr_t)signal, LLC);

                    // If temp1 arrives before temp2, then we will see a higher signal_accesses count.
                    //  I.e, temp1 <= temp2.
                    int time = comparator_weird_gate((void *)temp1, (void *)temp2, (void *)signal);
                    if (time > RAM_BOUND_HIGH)
                    {
                        if (error >= 5)
                        {
                            temp1->l2_evset = NULL;
                            temp2->l2_evset = NULL;
                            signal->l2_evset = NULL;
                            error = 0;
                        }
                        s--;
                        error++;
                    }
                    else
                    {
                        if (time < LLC_BOUND_LOW)
                        {
                            signal_accesses++;
                        }
                    }
                }

                // If temp1 > temp2
                if (signal_accesses > (LLC_SLICES * 2 * 0.8))
                {
                    results[(i * CL_PER_PAGE) + j] = 1;
                }
                // If temp1 <= temp2
                else if (signal_accesses <= (LLC_SLICES * 2 * 0.3))
                {
                    results[(i * CL_PER_PAGE) + j] = -1;
                }
            }
        }

        int max_crosscheck = 0;
        int max_crosscheck_idx = 0;

        for (int s = 0; s < LLC_SLICES; s++)
        {
            for (int p = 0; p < dt->unique_pages_count; p++)
            {
                int crosscheck = 0;
                for (size_t i = 0; i < CL_PER_PAGE; i++)
                {
                    int first = dt->unique_pages[p][i] ^ s;
                    for (size_t j = 0; j < CL_PER_PAGE; j++)
                    {
                        int second = dt->unique_pages[p][j] ^ s;

                        if (first > second && results[(i * CL_PER_PAGE) + j] == 1)
                        {
                            crosscheck++;
                        }
                        else if (first <= second && results[(i * CL_PER_PAGE) + j] == -1)
                        {
                            crosscheck++;
                        }
                    }
                }
                if (crosscheck > max_crosscheck)
                {
                    max_crosscheck = crosscheck;
                    max_crosscheck_idx = p;
                }
            }
        }

#if VERBOSITY >= 1
        for (int cl = 0; cl < CL_PER_PAGE; ++cl)
        {
            elem_t *temp1 = ((elem_t *)((uintptr_t)mem + (cl * CACHELINE)));
            printf("%2d", get_address_slice(virtual_to_physical((uint64_t)temp1)));
        }
        putchar('\n');
        for (int cl = 0; cl < CL_PER_PAGE; ++cl)
        {
            printf("%2d", dt->unique_pages[max_crosscheck_idx][cl]);
        }
        putchar('\n');
#endif

        for (int s = 0; s < LLC_SLICES; ++s)
        {
            for (int cl = 0; cl < CL_PER_PAGE; ++cl)
            {
                if (dt->unique_pages[max_crosscheck_idx][cl] == s)
                {
                    measuring_sticks[s] = (elem_t *)((uintptr_t)mem + (cl * CACHELINE));
                }
                if (s == 0)
                {
                    ((elem_t *)((uintptr_t)mem + (cl * CACHELINE)))->cslice = dt->unique_pages[max_crosscheck_idx][cl];
                }
            }
        }

        max_len = PAGE_SIZE;
    }

    for (int s = 0; s < LLC_SLICES; s++)
    {
#if VERBOSITY >= 1
        print_addr_info((uintptr_t)measuring_sticks[s]);
#endif
        for (uintptr_t i = max_len; i < len; i += CACHELINE)
        {
            elem_t *new = (elem_t *)((uintptr_t)mem + i);
            if (new->l2_evset == NULL)
                set_addr_state((uintptr_t) new, LLC);
            if (new != measuring_sticks[s] && new->l2_evset == measuring_sticks[s]->l2_evset)
            {
                signals[s] = new;
                break;
            }
        }
    }

#if VERBOSITY >= 1
    printf("init_measuring_sticks(): Found sticks in %fms\n", stop_timer(start) * 1000);
#endif

    return max_len;
}

void init_measuring_sticks_cheat()
{
    size_t len = 1 * MB;
    uint8_t *temp = (uint8_t *)initialise_memory(len, 0, PAGE_SIZE);
    measuring_sticks = calloc(LLC_SLICES, sizeof(uint64_t *));
    signals = calloc(LLC_SLICES, sizeof(uint64_t *));

    for (int s = 0; s < LLC_SLICES; s++)
    {
        for (uintptr_t i = 0; i < len; i += CACHELINE)
        {
            elem_t *new = (elem_t *)(temp + i);
            if (new->l2_evset == NULL)
                set_addr_state((uintptr_t) new, LLC);
            if (get_address_slice(virtual_to_physical((uint64_t) new)) == s)
            {
                measuring_sticks[s] = new;
                measuring_sticks[s]->cslice = s;
#if VERBOSITY >= 1
                print_addr_info((uintptr_t)measuring_sticks[s]);
#endif
                break;
            }
        }
    }

    for (int s = 0; s < LLC_SLICES; s++)
    {
        for (uintptr_t i = 0; i < len; i += CACHELINE)
        {
            elem_t *new = (elem_t *)((uintptr_t)temp + i);
            if (new->l2_evset == NULL)
                set_addr_state((uintptr_t) new, LLC);
            if (new != measuring_sticks[s] && new->l2_evset == measuring_sticks[s]->l2_evset)
            {
                signals[s] = new;
                signals[s]->cslice = s;
                break;
            }
        }
    }
}

elem_t **get_measuring_sticks()
{
    return measuring_sticks;
}

calibration_data_t *comparator_gate_get_calibration_data_cheat(void *mem, size_t len)
{
    if (al == NULL)
    {
        init_comparator_gate();
    }
    init_measuring_sticks_cheat();

    fprintf(stderr, "\033[1;31mWARNING: cheating to get calibration data!\033[0m\n");

    calibration_data_t *cd = calloc(1, sizeof(calibration_data_t));
    cd->len = (LLC_SLICES + 1) * CALIBRATION_SAMPLES;
    cd->data = calloc(cd->len, sizeof(int));
    for (size_t cl = 0; cl < CALIBRATION_SAMPLES; cl++)
    {
        if (cl * CACHELINE > len)
        {
            fprintf(stderr, "comparator_gate_get_calibration_data_cheat(): memory buffer too small to calibrate!\n");
            exit(1);
        }
        void *input = (void *)(mem + (cl * CACHELINE));
        int *real_slice = (int *)(cd->data + (cl * (LLC_SLICES + 1)));
        int *gate_output = (int *)(cd->data + (cl * (LLC_SLICES + 1)) + 1);
        *real_slice = get_address_slice(virtual_to_physical((uint64_t)input));
        comparator_gate(input, gate_output);
    }
    return cd;
}

#define CALIBRATION_MULTIPLIER 5
calibration_data_t *comparator_gate_get_calibration_data(void *mem, size_t len)
{
    if (al == NULL)
    {
        init_comparator_gate();
    }
    size_t max_len = init_measuring_sticks(mem, len);
    calibration_data_t *cd = calloc(1, sizeof(calibration_data_t));
    cd->len = (LLC_SLICES + 1) * (max_len / CACHELINE) * CALIBRATION_MULTIPLIER;
    cd->data = calloc(cd->len, sizeof(int));

    for (int p = 0; p < CALIBRATION_MULTIPLIER; p++)
    {
        for (size_t cl = 0; cl < max_len / CACHELINE; cl++)
        {
            if (cl * CACHELINE > len)
            {
                fprintf(stderr, "comparator_gate_get_calibration_data(): memory buffer too small to calibrate!\n");
                exit(1);
            }
            elem_t *input = (elem_t *)(mem + (cl * CACHELINE));
            int *cslice = (int *)(cd->data + (cl * (LLC_SLICES + 1)) + (p * (LLC_SLICES + 1) * (max_len / CACHELINE)));
            int *gate_output = (int *)(cd->data + (cl * (LLC_SLICES + 1)) + (p * (LLC_SLICES + 1) * (max_len / CACHELINE)) + 1);
            *cslice = input->cslice;
            comparator_gate((void *)input, gate_output);
        }
    }
    return cd;
}

inline void set_measure_states(void *input, void *compare, void *signal)
{
    set_addr_state((uintptr_t)input, LLC);
    memaccess(compare);
    memaccess(signal);
    //*compare and *signal are in the same L2 cache set to optimise the eviction process :)
    // set_addr_state((uintptr_t)compare, LLC);
    set_addr_state((uintptr_t)signal, CG_SIGNAL_EVICT);
}

void comparator_gate(void *input, int *output)
{
    if (al == NULL)
    {
        init_comparator_gate();
    }
    if (measuring_sticks == NULL)
    {
        fprintf(stderr, "comparator_gate(): Please call init_measuring_sticks() first.\n");
        exit(1);
    }

    for (int s = 0; s < LLC_SLICES; s++)
    {
        int signal_accesses = 0;
        int error = 0;
        for (int i = 0; i < MEASURE_SAMPLES; ++i)
        {
            set_measure_states(input, measuring_sticks[s], signals[s]);

            int time = comparator_weird_gate(input, measuring_sticks[s], signals[s]);
            if (time > RAM_BOUND_HIGH)
            {
                if (error >= 5)
                {
                    ((elem_t *)input)->l2_evset = NULL;
                    measuring_sticks[s]->l2_evset = NULL;
                    signals[s]->l2_evset = NULL;
                    error = 0;
                }
                i--;
                error++;
            }
            else
            {
                if (time < LLC_BOUND_LOW)
                {
                    signal_accesses++;
                }
            }
        }

        if (CG_FILTER_ENABLE && (double)signal_accesses > (double)(MEASURE_SAMPLES * CG_FILTER_LOWER) && (double)signal_accesses < (double)(MEASURE_SAMPLES * CG_FILTER_UPPER))
        {
            s--;
        }
        else
        {
            output[s] = signal_accesses;
        }
    }
}
