#include "evsets/evsets.h"
#include "slice_partitioning/slicing.h"

static evsp_config_t llc_evsp_config;

// Return a 1 or 0 depending on flags set and information about the address
int filter_element(cache_evsets_t *cache_evsets, elem_t *elem, int victim_slice, evset_t *victim_l2_evset)
{
    if (cache_evsets->flags & L2_CANDIDATE_SET_FILTERING || cache_evsets->flags & PERFECT_L2_CANDIDATE_SET_FILTERING)
    {
        if (elem->l2_evset == NULL)
            set_addr_state((uintptr_t)elem, LLC);
        if (victim_l2_evset != elem->l2_evset)
            return 0;
    }

    if (cache_evsets->flags & PERFECT_SLICE_FILTERING)
    {
        if (victim_slice != elem->slice)
            return 0;
    }
    else if (cache_evsets->flags & SLICE_FILTERING)
    {
        if (elem->cslice == -1)
            evsp_get_address_slice_decision_tree(llc_evsp_config, (void *)elem, 0);
        if (victim_slice != elem->cslice)
            return 0;
    }
    return 1;
}

// Function to create and filter a new array of pointers
evset_t *create_candidate_set(cache_evsets_t *cache_evsets, void *victim, size_t candidate_set_size)
{
    evset_t *evset = calloc(1, sizeof(evset_t));
    evset->cs = calloc(candidate_set_size, sizeof(elem_t *));
    size_t real_candidate_set_size = 0;

    void *mem = cache_evsets->evict_mem;
    size_t len = cache_evsets->evict_mem_len;

    int victim_slice = -1;
    evset_t *victim_l2_evset = NULL;

    if (cache_evsets->flags & SLICE_FILTERING)
    {
        victim_slice = ((elem_t *)victim)->cslice;
    }
    else if (cache_evsets->flags & PERFECT_SLICE_FILTERING)
    {
        victim_slice = ((elem_t *)victim)->slice;
    }

    if (cache_evsets->flags & L2_CANDIDATE_SET_FILTERING || cache_evsets->flags & PERFECT_L2_CANDIDATE_SET_FILTERING)
    {
        if (((elem_t *)victim)->l2_evset == NULL)
            set_addr_state((uintptr_t)victim, LLC);
        victim_l2_evset = ((elem_t *)victim)->l2_evset;
    }

    for (uintptr_t i = ((uintptr_t)victim % L1D_STRIDE); i < len; i += L1D_STRIDE)
    {
        if (real_candidate_set_size >= candidate_set_size)
            break;

        elem_t *new_elem = (elem_t *)((char *)mem + i);

        if (!new_elem->set)
        {
            if (filter_element(cache_evsets, new_elem, victim_slice, victim_l2_evset))
            {
                evset->cs[real_candidate_set_size++] = new_elem;
            }
        }
    }

    evset->size = real_candidate_set_size;
    return evset;
}

size_t reduction_failures = 0;

evset_t *evset_find(cache_evsets_t *cache_evsets, void *victim, void *evset_mem, size_t len)
{
    struct timeval timer = start_timer();
    double reduction_time = 0.0;
    evset_t *super_evset = calloc(1, sizeof(evset_t));
    size_t temp_candidate_set_size = cache_evsets->candidate_set_size;

    size_t max_iterations = (L2_SETS / L1D_SETS) * (LLC_SLICES * 2);

    if (cache_evsets->cache == L2)
    {
        max_iterations = 3;
    }

    if (cache_evsets->flags & L2_CANDIDATE_SET_FILTERING || cache_evsets->flags & PERFECT_L2_CANDIDATE_SET_FILTERING)
    {
        max_iterations /= (L2_SETS / L1D_SETS);
        temp_candidate_set_size /= (L2_SETS / L1D_SETS);

        if (!(cache_evsets->flags & SLICE_FILTERING))
        {
            temp_candidate_set_size *= 2;
        }
    }

    // Dynamically set the max iterations depending on how much uncertainty we remove.
    if (cache_evsets->flags & SLICE_FILTERING || cache_evsets->flags & PERFECT_SLICE_FILTERING)
    {
        max_iterations /= LLC_SLICES;
        temp_candidate_set_size /= LLC_SLICES;

        if (cache_evsets->flags & L2_CANDIDATE_SET_FILTERING)
        {
            temp_candidate_set_size *= (LLC_SLICES / 2);
        }
    }

    if (evset_mem == NULL || len == 0)
        return NULL;

    size_t iterations = 0;
    while (1)
    {
        if (iterations >= max_iterations)
        {
            free(super_evset->cs);
            free(super_evset);
            return NULL;
        }

        for (int r = 0; r < cache_evsets->reductions;)
        {
            iterations++;

            evset_t *evset = NULL;
            while (1)
            {
                evset = create_candidate_set(cache_evsets, victim, temp_candidate_set_size);
                if (evset->size < cache_evsets->target_size)
                {
                    temp_candidate_set_size += (cache_evsets->candidate_set_size / LLC_SLICES);
                    free(evset->cs);
                    free(evset);
                    if (iterations >= max_iterations)
                    {
                        free(super_evset->cs);
                        free(super_evset);
                        return NULL;
                    }
                }
                else
                {
                    break;
                }
                iterations++;
            }

            if (!evset)
            {
                printf("evset_find(): could not generate candidate set\n");
                exit(1);
            }

#if VERBOSITY >= 3
            printf("\n******************* ITERATION %ld REDUCING %4ld:%ld TO %ld *******************\n", iterations, temp_candidate_set_size, evset->size, cache_evsets->target_size);
            print_evset_info(victim, evset);
#endif
            struct timeval reduction_timer = start_timer();
            int ret = prune_candidate_set(cache_evsets, victim, evset);
            reduction_time += stop_timer(reduction_timer);

            // We encountered an error during reduction
            if (ret == 1)
            {
#if VERBOSITY >= 3
                printf("\n*******************   REDUCTION FAILURE!   *******************\n");
                printf("Before: %lu | %d + (%d) %f\n", temp_candidate_set_size, cache_evsets->candidate_set_size, ceil((double)cache_evsets->candidate_set_size / (double)LLC_SLICES));
                print_evset_info(victim, evset);
#endif
                reduction_failures++;

                if (cache_evsets->flags & SLICE_FILTERING)
                {
                    ((elem_t *)victim)->cslice = evsp_get_address_slice_decision_tree(llc_evsp_config, victim, 0);
                }
                else if (cache_evsets->flags & PERFECT_SLICE_FILTERING)
                {
                    ((elem_t *)victim)->slice = get_address_slice(virtual_to_physical((uint64_t)victim));
                }
                if (cache_evsets->flags & L2_CANDIDATE_SET_FILTERING)
                {
                    ((elem_t *)victim)->l2_evset = NULL;
                    set_addr_state((uintptr_t)victim, LLC);
                }

                temp_candidate_set_size += (cache_evsets->candidate_set_size / LLC_SLICES);
                if (iterations >= max_iterations)
                {
#if VERBOSITY >= 3
                    printf("evset_find(): failed to build eviction set, candidate_set_size increase too large, should have evictions by now. Trying further victims. Iterations: %lu\n", iterations);
#endif
                    free(super_evset->cs);
                    free(super_evset);
                    free(evset->cs);
                    free(evset);
                    return NULL;
                }
                free(evset->cs);
                free(evset);
            }
            else
            {
#if VERBOSITY >= 3
                printf("\n*******************   REDUCTION SUCCESS!   *******************\n");
                print_evset_info(victim, evset);
#endif

                // Set all good elements to 1
                for (size_t i = 0; i < evset->size; i++)
                {
                    evset->cs[i]->set = 1;
                }
                ((elem_t *)victim)->set = 1;

                // If we have a SF evset, append the victim to it to it (as the LLC evset creation routine only makes associativity and SF is LLC_ASSOC+1)
                // if (cache_evsets->cache == LLC && !IS_INCLUSIVE)
                // {
                //     evset->cs[evset->size] = (elem_t *)victim;
                //     evset->size++;
                // }

                // Join arrays
                size_t new_len = super_evset->size + evset->size;
                super_evset->cs = realloc(super_evset->cs, new_len * sizeof(elem_t *));
                memcpy(super_evset->cs + super_evset->size, evset->cs, evset->size * sizeof(elem_t *));
                super_evset->size = new_len;

                free(evset->cs);
                free(evset);
                r++;
            }
        }

        if (cache_evsets->algorithm == L2_CHEAT || cache_evsets->algorithm == LLC_CHEAT)
        {
            if (super_evset->size > cache_evsets->target_size)
            {
                super_evset->size = cache_evsets->target_size;
            }
        }
        // Test the found eviction set a fair bit!
        int evicted = 0;
        for (int i = 0; i < 5; ++i)
        {
            evicted += test_victim_eviction_avg(cache_evsets, victim, super_evset->cs, super_evset->size);
        }

        if (evicted < TEST_EVICTIONS * 5)
        {
#if VERBOSITY >= 3
            printf("\n*******************   FAILURE!   *******************\n");
            print_evset_info(victim, super_evset);
            printf("EVICTED: %d\n", evicted);
#endif
            super_evset->size = 0;
        }
        else
        {
#if VERBOSITY >= 3
            printf("\n******************* FINAL RESULT *******************\n");
            print_evset_info(victim, super_evset);
            printf("EVICTED: %d\n", evicted);
#endif
            temp_candidate_set_size = cache_evsets->candidate_set_size;
            break;
        }
    }

    int victim_l1_set = GET_L1D_SET_BITS(victim);

    double time = stop_timer(timer);

    if ((time * 1000) > 50)
    {
        fprintf(stderr, "evset_find(): Timeout, took >50 milliseconds for a single eviction set.\n");
        exit(1);
    }

#if VERBOSITY >= 2
    uint64_t victim_paddr = virtual_to_physical((uint64_t)victim);
    if (cache_evsets->cache == L2)
        printf("Added offset 0x%lx eviction set %4d/%4d with %2d/%2ld congruent for L2 set %4ld in %.3fms (%.3fms reduction) with %ld iterations\n", victim_l1_set * CACHELINE, cache_evsets->evsets_count[victim_l1_set] + 1, cache_evsets->evsets_per_offset, get_num_l2_same_set(victim, super_evset), super_evset->size, GET_L2_SET_BITS(victim_paddr), time * 1000, reduction_time * 1000, iterations);
    else if (cache_evsets->cache == LLC)
        printf("Added offset 0x%lx eviction set %4d/%4d with %2d/%2ld LLC congruent and %2d/%2ld L2_SS congruent for Slice %d LLC set %4ld (L2 set %4ld) in %.3fms (%.3fms reduction) with %ld iterations\n", victim_l1_set * CACHELINE, cache_evsets->evsets_count[victim_l1_set] + 1, cache_evsets->evsets_per_offset, get_num_llc_same_set(victim, super_evset), super_evset->size, get_num_l2_same_slice_and_set(victim, super_evset), super_evset->size, get_address_slice(victim_paddr), GET_LLC_SET_BITS(victim_paddr), GET_L2_SET_BITS(victim_paddr), time * 1000, reduction_time * 1000, iterations);
#elif VERBOSITY == 1
    printf("Added offset 0x%lx eviction set %4d/%4d in %.3fms (%.3fms reduction) with %ld iterations\n", victim_l1_set * CACHELINE, cache_evsets->evsets_count[victim_l1_set] + 1, cache_evsets->evsets_per_offset, time * 1000, reduction_time * 1000, iterations);
#endif

    cache_evsets->evsets_count[victim_l1_set]++;
    super_evset->victim = victim;

    return super_evset;
}

// Check to see if we have any existing offsets for a given victim by checking all eviction sets at this victim.
int existing_evset(cache_evsets_t *cache_evsets, elem_t *victim)
{
    int ret = 0;
    int victim_l1_set = GET_L1D_SET_BITS(victim);
    int index = victim_l1_set * cache_evsets->evsets_per_offset;

    for (int evset = 0; evset < cache_evsets->evsets_count[victim_l1_set]; evset++)
    {
        if (cache_evsets->algorithm == L2_CHEAT)
        {
            if (get_num_l2_same_set(victim, cache_evsets->evsets[index + evset]) >= L2_ASSOCIATIVITY)
            {
                ret = 1;
                break;
            }
        }
        else if (cache_evsets->algorithm == LLC_CHEAT)
        {
            if (get_num_l2_same_slice_and_set(victim, cache_evsets->evsets[index + evset]) >= (int)cache_evsets->target_size)
            {
                ret = 1;
                break;
            }
        }
        else
        {
            // Filter out eviction sets to test to determine if we already have an eviction set for this address
            if ((cache_evsets->flags & L2_CANDIDATE_SET_FILTERING && cache_evsets->flags & SLICE_FILTERING) || (cache_evsets->flags & L2_CANDIDATE_SET_FILTERING && cache_evsets->flags & PERFECT_SLICE_FILTERING))
            {
                if (victim->l2_evset_index != cache_evsets->evsets[index + evset]->cs[0]->l2_evset_index)
                    continue;
                if (cache_evsets->flags & SLICE_FILTERING && !IS_POWER_OF_TWO(LLC_SLICES) && victim->cslice != cache_evsets->evsets[index + evset]->cs[0]->cslice)
                    continue;
                if (cache_evsets->flags & PERFECT_SLICE_FILTERING && !IS_POWER_OF_TWO(LLC_SLICES) && victim->slice != cache_evsets->evsets[index + evset]->cs[0]->slice)
                    continue;
            }

            // Filter out eviction sets to test to determine if we already have an eviction set for this address
            if (cache_evsets->flags & L2_CANDIDATE_SET_FILTERING)
            {
                if (victim->l2_evset_index != cache_evsets->evsets[index + evset]->cs[0]->l2_evset_index)
                    continue;
            }

            int evicted = 0;
            for (int i = 0; i < 5; ++i)
            {
                evicted += test_victim_eviction_avg(cache_evsets, victim, cache_evsets->evsets[index + evset]->cs, cache_evsets->evsets[index + evset]->size);
            }
            if (evicted >= TEST_EVICTIONS * 4)
            {
                ret = 1;
                break;
            }
            else
            {
                flush_array(cache_evsets->evsets[index + evset]->cs, cache_evsets->evsets[index + evset]->size);
            }
        }
    }
    return ret;
}

size_t evset_expansion_count = 0;

int found[PAGE_SIZE / CACHELINE][LLC_SLICES][L2_SETS / L1D_SETS] = {0};

// Copy mirrors of each found evset to the rest of the page offsets for L2 cache only, as LLC slicing breaks this trick.
void evset_copy_for_page_offset(cache_evsets_t *cache_evsets, size_t offset)
{
    // For L2 and linear LLCs
    if (cache_evsets->cache == L2 || (cache_evsets->cache == LLC && IS_POWER_OF_TWO(LLC_SLICES)))
    {
        int l1d_set = offset / CACHELINE;
        int evsets_offset = l1d_set * cache_evsets->evsets_per_offset;
        for (int e = 0; e < cache_evsets->evsets_count[0]; ++e)
        {
            // Create new evset if it doesn't exist
            if (cache_evsets->evsets[evsets_offset + e] == NULL)
            {
                // Allocate new evset and copy elements from the original
                cache_evsets->evsets[evsets_offset + e] = calloc(1, sizeof(evset_t));
                cache_evsets->evsets[evsets_offset + e]->cs = calloc(cache_evsets->evsets[e]->size, sizeof(elem_t *));

                // Copy elements from the original evset with offset adjustment
                for (size_t i = 0; i < cache_evsets->evsets[e]->size; ++i)
                {
                    elem_t *original_elem = cache_evsets->evsets[e]->cs[i];
                    elem_t *mirror_elem = (elem_t *)((uintptr_t)original_elem + offset);
                    mirror_elem->set = 1;
                    cache_evsets->evsets[evsets_offset + e]->cs[i] = mirror_elem;
                }

                cache_evsets->evsets[evsets_offset + e]->size = cache_evsets->evsets[e]->size;
                cache_evsets->evsets[evsets_offset + e]->mirror = 1;
                cache_evsets->evsets_count[l1d_set]++;
                evset_expansion_count++;
            }
        }
    }
    // For non-linear LLCs
    else if (cache_evsets->cache == LLC && cache_evsets->flags & SLICE_FILTERING && !IS_POWER_OF_TWO(LLC_SLICES))
    {
        if (offset < CACHELINE)
            return;

        int prior_l1d_set = (offset - CACHELINE) / CACHELINE;

        // Go through each page offset
        // Then go through each newly discovered evset
        // See if any are mirrors exist where we have a target_size congruency where all address map to the same slice in the mirror
        for (size_t p_offset = offset / CACHELINE; p_offset < (PAGE_SIZE / CACHELINE); p_offset++)
        {
            size_t current_count = cache_evsets->evsets_count[prior_l1d_set];

            // If we already have enough evsets for this offset then skip
            if (cache_evsets->evsets_count[p_offset] >= cache_evsets->evsets_per_offset)
                continue;

            for (size_t e = 0; e < current_count; e++)
            {
                size_t idx = (prior_l1d_set * cache_evsets->evsets_per_offset) + e;
                evset_t *current = cache_evsets->evsets[idx];

                if (current == NULL)
                    break;

                elem_t *mirror_first_elem = (elem_t *)((uintptr_t)current->cs[0] + ((p_offset - prior_l1d_set) * CACHELINE));

                if (found[p_offset][mirror_first_elem->cslice][mirror_first_elem->l2_evset_index] >= MAX_EVSET_PER_SET)
                    continue;

                size_t slice_congruent = 1;
                for (size_t i = 1; i < current->size; i++)
                {
                    elem_t *mirror_elem = (elem_t *)((uintptr_t)current->cs[i] + ((p_offset - prior_l1d_set) * CACHELINE));
                    if (mirror_elem->cslice == mirror_first_elem->cslice)
                    {
                        slice_congruent++;
                    }
                }

                if (slice_congruent == cache_evsets->target_size)
                {
                    // If we have some evsets for this offset already
                    if (cache_evsets->evsets_count[p_offset] > 0)
                    {
                        // And we can already evict the mirror element, then don't add
                        if (existing_evset(cache_evsets, mirror_first_elem))
                        {
                            continue;
                        }
                    }

                    // Add new evset
                    size_t new_idx = (p_offset * cache_evsets->evsets_per_offset) + cache_evsets->evsets_count[p_offset];
                    cache_evsets->evsets[new_idx] = calloc(1, sizeof(evset_t));
                    cache_evsets->evsets[new_idx]->cs = calloc(slice_congruent, sizeof(elem_t *));
                    for (size_t i = 0; i < slice_congruent; ++i)
                    {
                        cache_evsets->evsets[new_idx]->cs[i] = (elem_t *)((uintptr_t)current->cs[i] + ((p_offset - prior_l1d_set) * CACHELINE));
                        cache_evsets->evsets[new_idx]->cs[i]->set = 1;
                    }
                    cache_evsets->evsets[new_idx]->size = slice_congruent;
                    cache_evsets->evsets[new_idx]->victim = cache_evsets->evsets[new_idx]->cs[0];
                    cache_evsets->evsets[new_idx]->mirror = 1;
                    cache_evsets->evsets_count[p_offset]++;
                    evset_expansion_count++;

                    found[p_offset][mirror_first_elem->cslice][mirror_first_elem->l2_evset_index]++;

#if VERBOSITY >= 2
                    uint64_t victim_paddr = virtual_to_physical((uint64_t)cache_evsets->evsets[new_idx]->victim);
                    if (cache_evsets->cache == L2)
                        printf("Evset Expansion: added offset 0x%03lx eviction set %4d/%4d with %2d/%2ld congruent for L2 set %4ld\n", p_offset * CACHELINE, cache_evsets->evsets_count[p_offset], cache_evsets->evsets_per_offset, get_num_l2_same_set(cache_evsets->evsets[new_idx]->victim, cache_evsets->evsets[new_idx]), cache_evsets->evsets[new_idx]->size, GET_L2_SET_BITS(victim_paddr));
                    else if (cache_evsets->cache == LLC)
                        printf("Evset Expansion: added offset 0x%03lx eviction set %4d/%4d with %2d/%2ld LLC congruent and %2d/%2ld L2_SS congruent for Slice %d LLC set %4ld (L2 set %4ld)\n", p_offset * CACHELINE, cache_evsets->evsets_count[p_offset], cache_evsets->evsets_per_offset, get_num_llc_same_set(cache_evsets->evsets[new_idx]->victim, cache_evsets->evsets[new_idx]), cache_evsets->evsets[new_idx]->size, get_num_l2_same_slice_and_set(cache_evsets->evsets[new_idx]->victim, cache_evsets->evsets[new_idx]), cache_evsets->evsets[new_idx]->size, get_address_slice(victim_paddr), GET_LLC_SET_BITS(victim_paddr), GET_L2_SET_BITS(victim_paddr));
#elif VERBOSITY == 1
                    printf("Evset Expansion: added offset 0x%03lx eviction set %4d/%4d\n", p_offset * CACHELINE, cache_evsets->evsets_count[p_offset], cache_evsets->evsets_per_offset);
#endif
                }
            }
        }
    }
    return;
}

cache_evsets_t *evsets_create(address_state cache, evset_algorithm algorithm, evset_flags flags, evsp_flags eflags, int candidate_set_size, int reductions, void *mem, size_t mem_len)
{
    if (cache == LLC || cache == SF)
    {
        DecisionTreeNode_t *dt = get_decision_tree();
    }

    struct timeval timer = start_timer();
    double config_time_taken = 0.0;
    double time_taken = 0.0;
    size_t total_count = 0;

    size_t len;
    if (cache == L2)
    {
        len = ((L2_SETS / L1D_SETS) * L2_ASSOCIATIVITY) * 4 * PAGE_SIZE;
    }
    else if (cache == LLC)
    {
        len = ((LLC_SETS / L1D_SETS) * LLC_ASSOCIATIVITY) * LLC_SLICES * 3 * PAGE_SIZE;
    }
    else
    {
        len = 256 * MB;
    }

    uint8_t *evict_mem = (uint8_t *)mem;

    if (evict_mem == NULL)
    {
        evict_mem = (uint8_t *)initialise_memory(len, 0, PAGE_SIZE);
    }
    else
    {
        len = mem_len;
    }

    cache_evsets_t *cache_evsets = calloc(1, sizeof(cache_evsets_t));
    cache_evsets->evict_mem = evict_mem;
    cache_evsets->evict_mem_len = len;
    cache_evsets->cache = cache;
    cache_evsets->algorithm = algorithm;
    cache_evsets->flags = flags;
    cache_evsets->candidate_set_size = candidate_set_size;
    cache_evsets->reductions = reductions;

    cache_evsets->evsets_count = calloc(L1D_SETS, sizeof(int));

    if (cache == L1)
    {
        cache_evsets->target_size = L1D_ASSOCIATIVITY + 4;
        cache_evsets->evsets = calloc(L1D_SETS, sizeof(evset_t));
        cache_evsets->evsets_per_offset = 1;

        for (size_t i = 0; i < L1D_SETS; ++i)
        {
            cache_evsets->evsets[i] = create_candidate_set(cache_evsets, (void *)(evict_mem + (i * L1D_SETS)), cache_evsets->target_size);
        }

        return cache_evsets;
    }
    else if (cache == L2)
    {
        // Only allocate the number of evictions sets we need just for page offset 0x0
        cache_evsets->target_size = L2_CANDIDATE_SIZE_TARGET;
        cache_evsets->evsets = calloc(L2_SETS, sizeof(evset_t));

        cache_evsets->evsets_per_offset = L2_SETS / L1D_SETS;
        cache_evsets->measure_low = LLC_BOUND_LOW;
        cache_evsets->measure_high = RAM_BOUND_HIGH;

        // As the L2 mapping is linear, we can expand eviction sets out from only building for the initial page offset.
        cache_evsets->page_offset_limit = 1;
    }
    else if (cache == LLC)
    {
        cache_evsets->target_size = LLC_CANDIDATE_SIZE_TARGET;
        if (IS_INCLUSIVE)
        {
            cache_evsets->evsets_per_offset = (LLC_SETS / L1D_SETS) * LLC_SLICES;
        }
        else
        {
            cache_evsets->evsets_per_offset = (SF_SETS / L1D_SETS * LLC_SLICES);
        }

        cache_evsets->evsets = calloc(cache_evsets->evsets_per_offset * (PAGE_SIZE / CACHELINE), sizeof(evset_t *));

        // Due to the pattern of the linear slice functions, we can expand eviction sets out from only building for the initial page offset.
        if (IS_POWER_OF_TWO(LLC_SLICES))
        {
            cache_evsets->page_offset_limit = 1;
        }
        // This is not true for non-linear slice functions - we need to build for every page offset.
        else
        {
            cache_evsets->page_offset_limit = PAGE_SIZE / CACHELINE;
        }

        cache_evsets->measure_low = LLC_BOUND_HIGH;
        cache_evsets->measure_high = RAM_BOUND_HIGH;

        struct timeval setup_timer = start_timer();

        if (cache_evsets->flags & PERFECT_SLICE_FILTERING)
        {
            printf("Flag PERFECT_SLICE_FILTERING enabled, setting real slices...\n");
            for (uintptr_t i = 0; i < len; i += CACHELINE)
            {
                elem_t *new_elem = (elem_t *)((char *)mem + i);
                new_elem->slice = get_address_slice(virtual_to_physical((uint64_t)new_elem));
            }
            printf("Done\n");
        }
        else if (cache_evsets->flags & SLICE_FILTERING)
        {
            // Configure EVSP
            llc_evsp_config = evsp_configure(mem, len / PAGE_SIZE, LLC_SLICES, eflags);
            evsp_run(llc_evsp_config, mem);
            // evsp_verify(llc_evsp_config);
        }
        if (cache_evsets->flags & PERFECT_L2_CANDIDATE_SET_FILTERING)
        {
            printf("Flag PERFECT_L2_CANDIDATE_SET_FILTERING enabled, getting L2 set bits...\n");
            for (uintptr_t i = 0; i < len; i += CACHELINE)
            {
                elem_t *new_elem = (elem_t *)((char *)mem + i);
                new_elem->l2_evset = (evset_t *)GET_L2_SET_BITS(virtual_to_physical((uint64_t)new_elem));
            }
        }
        else if (cache_evsets->flags & L2_CANDIDATE_SET_FILTERING && !(cache_evsets->flags & SLICE_FILTERING))
        {
            printf("Flag L2_CANDIDATE_SET_FILTERING enabled with SLICE_FILTERING disabled, getting L2 eviction sets...\n");
            for (uintptr_t i = 0; i < len; i += CACHELINE)
            {
                elem_t *new_elem = (elem_t *)((char *)mem + i);
                set_addr_state((uintptr_t)new_elem, LLC);
            }
        }
        config_time_taken = stop_timer(setup_timer);
    }
    else
    {
        printf("ERROR: Desired cache eviction set invalid\n");
        exit(1);
    }

#if VERBOSITY >= 1
    printf("Page Offsets to build: %lu\n", cache_evsets->page_offset_limit);
#endif

    evset_expansion_count = 0;
    reduction_failures = 0;
    for (size_t p_offset = 0; p_offset < cache_evsets->page_offset_limit; p_offset++)
    {
        size_t vic_count = 0;

        size_t page = 0;
        for (page = 0; page < len / PAGE_SIZE; page++)
        {
            elem_t *victim = (elem_t *)(cache_evsets->evict_mem + (page * L1D_STRIDE) + (p_offset * CACHELINE));

            if (victim->set == 1)
            {
                continue;
            }

            if (cache_evsets->flags & SLICE_FILTERING && !IS_POWER_OF_TWO(LLC_SLICES) && cache_evsets->cache == LLC && found[p_offset][victim->cslice][victim->l2_evset_index] >= MAX_EVSET_PER_SET)
            {
                continue;
            }

            int victim_l1_set = GET_L1D_SET_BITS(victim);
            // Used as the index into our array of eviction sets
            int index = victim_l1_set * cache_evsets->evsets_per_offset;

            if (cache_evsets->evsets_count[victim_l1_set] >= cache_evsets->evsets_per_offset)
            {
                if (cache_evsets->cache == L2)
                {
                    break;
                }
                else if (cache_evsets->cache == LLC)
                {
                    break;
                }
                else
                {
                    printf("Desired cache eviction sets invalid\n");
                    exit(1);
                }
            }

            // We had no evictions from any available eviction sets then create a new one
            if (!existing_evset(cache_evsets, victim))
            {
                int new_evset_index = index + cache_evsets->evsets_count[victim_l1_set];
                static struct timeval between;
                if (total_count)
                {
                    double timer = stop_timer(between) * 1000;
                    if (timer >= 1000)
                    {
                        fprintf(stderr, "evset_create(): Timeout, took longer than 1s for a single evset\n");
                        exit(1);
                    }
                }
                evset_t *temp = evset_find(cache_evsets, victim, evict_mem, len);

                if (temp)
                {
                    cache_evsets->evsets[new_evset_index] = temp;
                    total_count++;

                    found[p_offset][temp->cs[0]->cslice][temp->cs[0]->l2_evset_index]++;
                    between = start_timer();
                }
                vic_count = 0;
            }
            vic_count++;

            // If we haven't found a new victim after a given amount, then move to next page offset, no point lagging here.
            if (cache_evsets->cache == LLC && vic_count > (L2_SETS / L1D_SETS) * (LLC_SLICES))
                break;
        }

        // If we have L2 eviction set creation or LLC with a linear slice function
        if (cache_evsets->cache == L2 || (cache_evsets->cache == LLC && IS_POWER_OF_TWO(LLC_SLICES) && cache_evsets->flags & FULL_SYSTEM))
        {
            for (size_t offset = CACHELINE; offset < SMALLPAGE; offset += CACHELINE)
            {
                evset_copy_for_page_offset(cache_evsets, offset);
            }
        }
        // If we have a LLC eviction set with a non-linear slice function AND slice filtering enabled
        else if (cache_evsets->cache == LLC && cache_evsets->flags & SLICE_FILTERING && cache_evsets->flags & FULL_SYSTEM)
        {
            evset_copy_for_page_offset(cache_evsets, (p_offset + 1) * CACHELINE);
        }

        if ((cache_evsets->evsets_count[p_offset] < cache_evsets->evsets_per_offset && cache_evsets->flags & FULL_SYSTEM) || cache_evsets->evsets_count[p_offset] < cache_evsets->evsets_per_offset && cache_evsets->cache == L2)
        {
            if (cache_evsets->cache == L2)
            {
                printf("Error: did not find all desired evsets for L2 offset 0x%lx\n", p_offset * CACHELINE);
                exit(1);
            }
            else if (cache_evsets->cache == LLC)
            {
#if VERBOSITY >= 0
                printf("evsets_create(%s): Error: did not find all desired evsets for LLC offset 0x%lx\r", get_address_state_string(cache_evsets->cache), p_offset * CACHELINE);
#endif
            }
            else
            {
                printf("Desired cache eviction sets invalid\n");
                exit(1);
            }
        }
        else
        {
#if VERBOSITY >= 0
            printf("evsets_create(%s): Found all desired evsets for offset 0x%03lx at page %ld                         \r", get_address_state_string(cache_evsets->cache), p_offset * CACHELINE, page);
#endif
        }

        if (!(cache_evsets->flags & FULL_SYSTEM))
        {
            break;
        }
    }

    time_taken = stop_timer(timer);
    printf("\nevsets_create(%s): Evset expansion count: %5lu\n", get_address_state_string(cache_evsets->cache), evset_expansion_count);
    printf("evsets_create(%s): Reduction failures:     %5lu\n", get_address_state_string(cache_evsets->cache), reduction_failures);
    printf("evsets_create(%s): Setup took: %.3fms\n", get_address_state_string(cache_evsets->cache), config_time_taken * 1000);
    printf("evsets_create(%s): Build took: %.3fms\n", get_address_state_string(cache_evsets->cache), (time_taken - config_time_taken) * 1000);
    printf("evsets_create(%s): Avg time:   %.3fms\n", get_address_state_string(cache_evsets->cache), ((time_taken - config_time_taken) / (double)total_count) * 1000);
    printf("evsets_create(%s): Total time: %.3fms\n", get_address_state_string(cache_evsets->cache), time_taken * 1000);
    return cache_evsets;
}

evset_t *evset_find_for_address(cache_evsets_t *cache_evsets, void *victim)
{
    int victim_l1_set = GET_L1D_SET_BITS(victim);
    int offset = victim_l1_set * cache_evsets->evsets_per_offset;
    // Test to see if any known eviction sets for this address work
    int max_evset = 0;
    int max_evicted = 0;

    for (int evset = 0; evset < cache_evsets->evsets_count[victim_l1_set]; evset++)
    {
        int evicted = 0;
        for (int i = 0; i < 1; ++i)
        {
            evicted += test_victim_eviction_avg(cache_evsets, victim, cache_evsets->evsets[offset + evset]->cs, cache_evsets->evsets[offset + evset]->size);
        }
        if (evicted >= max_evicted)
        {
            max_evicted = evicted;
            max_evset = evset;
        }
    }

    if (cache_evsets->cache == L2)
    {
        // Apply L2 evset to entire page
        for (size_t offset = 0; offset < SMALLPAGE; offset += CACHELINE)
        {
            elem_t *cl = (elem_t *)(victim - ((uintptr_t)victim % SMALLPAGE) + offset);
            int cl_l1_set = GET_L1D_SET_BITS(cl);
            int cl_offset = cl_l1_set * cache_evsets->evsets_per_offset;
            cl->l2_evset = cache_evsets->evsets[cl_offset + max_evset];
            cl->l2_evset_index = max_evset;
        }
    }

    return cache_evsets->evsets[offset + max_evset];
}

int evsets_test(cache_evsets_t *cache_evsets)
{
    if (cache_evsets->cache == L2)
    {
        int err = 0;
        size_t len = 128 * SMALLPAGE;
        uint8_t *test = (uint8_t *)initialise_memory(len, 0, SMALLPAGE);

        // Test evictions for 512 pages at offset 0x0
        for (size_t i = 0; i < len; i += SMALLPAGE)
        {
            void *victim = test + i;
            int victim_l1_set = GET_L1D_SET_BITS(victim);
            int offset = victim_l1_set * cache_evsets->evsets_per_offset;

            int success = 0;
            for (int evset = 0; evset < cache_evsets->evsets_count[victim_l1_set]; ++evset)
            {
                int evicted = 0;
                for (int i = 0; i < 5; ++i)
                {
                    evicted += test_victim_eviction_avg(cache_evsets, victim, cache_evsets->evsets[offset + evset]->cs, cache_evsets->evsets[offset + evset]->size);
                }

                if (evicted >= TEST_EVICTIONS * 4)
                {
                    success = 1;
                    break;
                }
            }

            if (!success)
            {
                printf("Failed eviction for L1:L2 set %ld:%ld once\n", GET_L1D_SET_BITS(virtual_to_physical((uint64_t)victim)), GET_L2_SET_BITS(virtual_to_physical((uint64_t)victim)));
                err++;
            }
        }

        munmap(test, len);
        if (err > 10)
            return 1;
        return 0;
    }
    else if (cache_evsets->cache == LLC)
    {
        int llc_sets;
        if (IS_INCLUSIVE)
            llc_sets = LLC_SETS;
        else
            llc_sets = SF_SETS;

        int *llc_set_count = calloc(llc_sets * LLC_SLICES, sizeof(int));
        size_t successful = 0;

        for (size_t o = 0; o < (PAGE_SIZE / CACHELINE); o++)
        {
            for (int i = 0; i < cache_evsets->evsets_per_offset; i++)
            {
                size_t index = o * (cache_evsets->evsets_per_offset) + i;
                if (cache_evsets->evsets[index] != NULL)
                {
                    uint64_t paddr = virtual_to_physical((uint64_t)cache_evsets->evsets[index]->cs[0]);
                    int llc_set;
                    if (IS_INCLUSIVE)
                        llc_set = GET_LLC_SET_BITS(paddr);
                    else
                        llc_set = GET_L2_SET_BITS(paddr);
                    int slice = get_address_slice(paddr);
                    int result_index = (slice * llc_sets) + llc_set;
                    int num_l2_same_slice_and_set = get_num_l2_same_slice_and_set(cache_evsets->evsets[index]->cs[0], cache_evsets->evsets[index]);
                    llc_set_count[result_index]++;
                    if ((double)num_l2_same_slice_and_set / (double)cache_evsets->evsets[index]->size == 1.0 && cache_evsets->evsets[index]->size == LLC_CANDIDATE_SIZE_TARGET)
                    {
                        successful++;
                    }
                }
            }
        }

        size_t total = 0;
        size_t duplicates = 0;
        size_t missing = 0;

        for (size_t s = 0; s < LLC_SLICES; s++)
        {
            size_t limit = llc_sets / MAX_EVSET_PER_SET;

            for (size_t i = 0; i < limit; i++)
            {
                int temp = 0;
                size_t index = (s * llc_sets) + i;

                temp += llc_set_count[index];

                if (MAX_EVSET_PER_SET > 1)
                {
                    size_t index2 = (s * llc_sets) + i;
                    index2 ^= L2_SETS;

                    temp += llc_set_count[index2];
                }

                if (temp > MAX_EVSET_PER_SET)
                {
                    duplicates += temp - MAX_EVSET_PER_SET;
                }
                else if (temp < MAX_EVSET_PER_SET)
                {
                    missing += MAX_EVSET_PER_SET - temp;
                }
                total += temp;

                if (!(cache_evsets->flags & FULL_SYSTEM))
                {
                    i += (CL_PER_PAGE - 1);
                }
            }
        }
        size_t total_sets = llc_sets * LLC_SLICES;

        if (!(cache_evsets->flags & FULL_SYSTEM))
        {
            total_sets /= CL_PER_PAGE;
        }

        if (cache_evsets->flags & SLICE_FILTERING)
            evsp_verify(llc_evsp_config);

        char *state_string = get_address_state_string(cache_evsets->cache);

        // Print statements with color codes
        printf("evsets_test(%s): \033[0mTotal           : %5ld / %5ld [%.2f%%]\033[0m\n", state_string, total, total_sets, (double)total / (double)(total_sets) * 100);
        printf("evsets_test(%s): \033[1;32mSuccessful      : %5ld / %5ld [%.2f%%]\033[0m\n", state_string, successful, total, (double)successful / (double)(total) * 100);
        printf("evsets_test(%s): \033[1;33mDuplicates      : %5ld / %5ld [%.2f%%]\033[0m\n", state_string, duplicates, total, (double)duplicates / (double)total * 100);
        printf("evsets_test(%s): \033[1;31mMissing         : %5ld / %5ld [%.2f%%]\033[0m\n", state_string, missing, total_sets, (double)missing / (double)(total_sets) * 100);
        free(llc_set_count);
    }
    return 0;
}

void evsets_release(cache_evsets_t *cache_evsets)
{
    for (size_t addr = 0; addr < cache_evsets->evict_mem_len; addr += CACHELINE)
    {
        elem_t *temp = (elem_t *)(cache_evsets->evict_mem + addr);
        temp->next = NULL;
        temp->prev = NULL;
        temp->set = 0;
    }

    if (cache_evsets->cache == L1)
    {
        for (size_t i = 0; i < L1D_SETS; ++i)
        {
            if (cache_evsets->evsets[i] != NULL)
            {
                free(cache_evsets->evsets[i]);
            }
        }
    }
    else if (cache_evsets->cache == L2)
    {
        for (size_t i = 0; i < L2_SETS; ++i)
        {
            if (cache_evsets->evsets[i] != NULL)
            {
                free(cache_evsets->evsets[i]);
            }
        }
    }
    else if (cache_evsets->cache == LLC)
    {
        for (size_t i = 0; i < cache_evsets->evsets_per_offset * (PAGE_SIZE / CACHELINE); i++)
        {
            if (cache_evsets->evsets[i] != NULL)
            {
                free(cache_evsets->evsets[i]->cs);
                free(cache_evsets->evsets[i]);
            }
        }
    }
    else
    {
        printf("evsets_release(): Not implemented for desired cache\n");
        exit(1);
    }

    evsp_release(llc_evsp_config);

    free(cache_evsets->evsets);
    free(cache_evsets->evsets_count);
    free(cache_evsets);
}
