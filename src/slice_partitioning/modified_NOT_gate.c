#include <stdint.h>

#include <assemblyline.h>

#include "evsets/evsets_defs.h"
#include "slice_partitioning/modified_NOT_gate.h"

assemblyline_t *al_NOT;
uint32_t (**NOT_weird_gate)(void *, void *);

assemblyline_t al_determine;
int (*determination_code)(uint32_t, int *);

char operations[18][64] = {"    nop\n", "    times 2 nop\n", "    movd ecx, [rsi]\n", "    test rdx, rax\n", "    or rax, rdx\n", "    add rsi, rax\n", "    xor rsi, rax\n", "    shl rsi, 0\n", "    imul rsi, rax\n", "    sfence\n", "    lfence\n", "    mfence\n", "    mov rdx, rax\n", "    sub rdx, rax\n", "    pxor mm0, mm1\n", "    addpd xmm0, xmm1\n", "    sqrtsd xmm0, xmm0\n", "    div rdx\n"};

char *create_NOT_gate_code(int chain_len)
{
    // Prepare the assembly start code
    const char *asm_start_temp = "    xor rdi, 0x800\n"
                                 "    mov rax, [rdi]\n" // Make sure page for *input is in TLB
                                 "    xor rdi, 0x800\n"
                                 "    xor rsi, 0x800\n"
                                 "    mov rax, [rsi]\n" // Make sure page for *output is in TLB
                                 "    xor rsi, 0x800\n"
                                 "    mov rax, 0x0\n" // this is here for some instruction chains which use RAX and need it to be 0
                                 "    mfence\nlfence\n"
                                 "    call long 0x%x\n"; // 0x6 goes in the %x

    size_t asm_start_len = snprintf(NULL, 0, asm_start_temp, 0x6 + (chain_len * 0x3)) + 1;
    char *asm_start = malloc(asm_start_len);
    if (!asm_start)
    {
        perror("Failed to allocate memory for asm_start");
        exit(EXIT_FAILURE);
    }

    snprintf(asm_start, asm_start_len, asm_start_temp, 0x6 + (chain_len * 0x3));

    // Create the instruction chain
    size_t instruction_chain_len = 16384;
    char *instruction_chain = malloc(instruction_chain_len);
    if (!instruction_chain)
    {
        perror("Failed to allocate memory for instruction_chain");
        free(asm_start);
        exit(EXIT_FAILURE);
    }
    memset(instruction_chain, 0, instruction_chain_len);
    for (int i = 0; i < chain_len; i++)
    {
        strncat(instruction_chain, operations[5], instruction_chain_len - strlen(instruction_chain) - 1);
    }

    // Prepare the assembly end code
    const char *asm_end_temp = "    add rax, [rsi]\n"
                               "    lfence\n" // stops speculation

                               "    mov r11, 0x%x\n"
                               "    add r11, [rdi]\n"
                               "    add [rsp], r11\n"
                               "    ret\n"

                               "    lfence\n" // stops speculation
                               "    xor rsi, 0x800\n"
                               "    mov rax, [rsi]\n" // Make sure page for address *output is in TLB
                               "    xor rsi, 0x800\n"
                               "    %s\n"
                               "    rdtscp\n"
                               "    mov r9, rax\n"
                               "    mov r11, [rsi]\n"
                               "    rdtscp\n"
                               "    sub rax, r9\n"
                               "    ret\n";

    size_t asm_end_len = snprintf(NULL, 0, asm_end_temp, 0x14 + (chain_len * 0x3), "mfence\nlfence\n") + 1;
    char *asm_end = malloc(asm_end_len);
    if (!asm_end)
    {
        perror("Failed to allocate memory for asm_end");
        free(asm_start);
        free(instruction_chain);
        exit(EXIT_FAILURE);
    }
    snprintf(asm_end, asm_end_len, asm_end_temp, 0x14 + (chain_len * 0x3), "mfence\nlfence\n");

    // Calculate total length and allocate final string
    size_t total_len = strlen(asm_start) + strlen(instruction_chain) + strlen(asm_end) + 1;
    char *asm_tmp = calloc(total_len, sizeof(char));
    if (!asm_tmp)
    {
        perror("Error: asm_tmp calloc()");
        free(asm_start);
        free(instruction_chain);
        free(asm_end);
        exit(EXIT_FAILURE);
    }

    strcat(asm_tmp, asm_start);
    strcat(asm_tmp, instruction_chain);
    strcat(asm_tmp, asm_end);

    // Clean up allocated memory
    free(asm_start);
    free(instruction_chain);
    free(asm_end);

    return asm_tmp;
}

inline void set_measure_states(void *input, void *output)
{
    set_addr_state((uintptr_t)input, LLC);
    set_addr_state((uintptr_t)output, LLC);
}

int modified_NOT_gate(void *input, void *output)
{
    if (al_NOT == NULL)
    {
        al_NOT = calloc(ROB_SIZE + 1, sizeof(assemblyline_t));
        NOT_weird_gate = (uint32_t(**)(void *, void *))calloc(ROB_SIZE + 1, sizeof(uint32_t(*)(void *, void *)));
    }

    if (determination_code == NULL)
    {
        al_determine = asm_create_instance(NULL, 0);
        char *determination_asm = calloc(512, sizeof(char)); // Adjust the size as needed

        // Use snprintf to generate assembly with variables
        snprintf(determination_asm, 512,
                 "    mov rcx, rdi\n"
                 "    cmp rcx, %u\n" // Compare to measure_high, out of bounds
                 "    jl  0x5\n"     // jump to cmp
                 "    inc qword [rsi]\n"
                 "    jmp 0x9\n"     // jmp to ret
                 "    cmp rcx, %u\n" // If greater than measure_low, increment no_output_accesses
                 "    jle 0x3\n"     // jmp to ret
                 "    inc qword [rsi]\n"
                 "    ret\n",
                 LLC_BOUND_HIGH, LLC_BOUND_LOW);

        asm_assemble_str(al_determine, determination_asm);
        determination_code = (int (*)(uint32_t, int *))(asm_get_code(al_determine));
    }

    int low = 0;
    int high = ROB_SIZE;
    int chain_len = ((low + high) / 2) % ROB_SIZE + 1;
    int found_chain_len = 0;
    int tries = 0;

    while (low < high)
    {
        if (al_NOT[chain_len] == NULL)
        {
            al_NOT[chain_len] = asm_create_instance(NULL, 0);
            char *NOT_gate_asm = create_NOT_gate_code(chain_len);
            asm_assemble_str(al_NOT[chain_len], NOT_gate_asm);
            NOT_weird_gate[chain_len] = (uint32_t(*)(void *, void *))(asm_get_code(al_NOT[chain_len]));
            free(NOT_gate_asm);
        }

        int no_output_access = 0;
        uint32_t time = 0;

        for (volatile int i = 0; i < 1; ++i)
        {
            set_measure_states(input, output);
            time = (*NOT_weird_gate[chain_len])(input, output);
            determination_code(time, &no_output_access);
        }

        if (chain_len == 0 || no_output_access < ceil((double)1 * 0.5))
        {
            low = chain_len + 1;
            if (low > LLC_BOUND_HIGH)
                low = LLC_BOUND_HIGH;
        }
        else
        {
            found_chain_len = chain_len;
            high = chain_len - 2;
            if (high < LLC_BOUND_LOW)
                high = LLC_BOUND_LOW;
        }

        chain_len = ((low + high) / 2) % ROB_SIZE + 1;
        if (chain_len < 0)
        {
            chain_len = 0;
        }
        tries++;
        if (tries > 20 && PAGE_SIZE == SMALLPAGE)
        {
            elem_t *victim = (elem_t *)input;
            victim->l2_evset = NULL;
            elem_t *victim2 = (elem_t *)output;
            victim2->l2_evset = NULL;
            tries = 0;
        }
    }

    return found_chain_len;
}

int modified_NOT_gate_raw(void *input, void *output, int chain_len)
{
    if (al_NOT == NULL)
    {
        al_NOT = calloc(ROB_SIZE + 1, sizeof(assemblyline_t));
        NOT_weird_gate = (uint32_t(**)(void *, void *))calloc(ROB_SIZE + 1, sizeof(uint32_t(*)(void *, void *)));
    }

    if (determination_code == NULL)
    {
        al_determine = asm_create_instance(NULL, 0);
        char *determination_asm = calloc(512, sizeof(char)); // Adjust the size as needed

        // Use snprintf to generate assembly with variables
        snprintf(determination_asm, 512,
                 "    mov rcx, rdi\n"
                 "    cmp rcx, %u\n" // Compare to measure_high, out of bounds
                 "    jl  0x5\n"     // jump to cmp
                 "    inc qword [rsi]\n"
                 "    jmp 0x9\n"     // jmp to ret
                 "    cmp rcx, %u\n" // If greater than measure_low, increment no_output_accesses
                 "    jle 0x3\n"     // jmp to ret
                 "    inc qword [rsi]\n"
                 "    ret\n",
                 LLC_BOUND_HIGH, LLC_BOUND_LOW);

        asm_assemble_str(al_determine, determination_asm);
        determination_code = (int (*)(uint32_t, int *))(asm_get_code(al_determine));
    }

    if (al_NOT[chain_len] == NULL)
    {
        char *NOT_gate_asm = create_NOT_gate_code(chain_len);

        al_NOT[chain_len] = asm_create_instance(NULL, 0);
        asm_assemble_str(al_NOT[chain_len], NOT_gate_asm);
        NOT_weird_gate[chain_len] = (uint32_t(*)(void *, void *))(asm_get_code(al_NOT[chain_len]));

        free(NOT_gate_asm);
    }

    int no_output_access = 0; // i.e. chain was longer than the compared memory access
    uint32_t time = 0;

    for (volatile int i = 0; i < 1; ++i)
    {
        set_measure_states(input, output);
        time = (*NOT_weird_gate[chain_len])(input, output);
        determination_code(time, &no_output_access);
    }

    return no_output_access;
}