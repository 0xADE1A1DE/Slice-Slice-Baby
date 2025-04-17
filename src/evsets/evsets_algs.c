#include "evsets/evsets_algs.h"
#include "util/util.h"

// This is only for testing
int l2_cheat(cache_evsets_t *cache_evsets, void *victim, evset_t *evset)
{
    int ret = 0;

    elem_t **cheat_start = calloc(cache_evsets->target_size, sizeof(elem_t *));
    size_t count = 0;

    for (size_t i = 0; i < evset->size; i++)
    {
        if (GET_L2_SET_BITS(virtual_to_physical((uint64_t)victim)) == GET_L2_SET_BITS(virtual_to_physical((uint64_t)evset->cs[i])))
        {
            if (count < cache_evsets->target_size)
            {
                cheat_start[count] = evset->cs[i];
                count++;
            }
        }
    }

    evset->cs = cheat_start;
    evset->size = count;
    return ret;
}

// This is only for testing
int llc_cheat(cache_evsets_t *cache_evsets, void *victim, evset_t *evset)
{
    int ret = 0;
    int victim_slice = get_address_slice(virtual_to_physical((uint64_t)victim));
    int victim_llc_set = GET_LLC_SET_BITS(virtual_to_physical((uint64_t)victim));

    elem_t **cheat_start = calloc(cache_evsets->target_size, sizeof(elem_t *));
    size_t count = 0;

    for (size_t i = 0; i < evset->size; i++)
    {
        if (evset->cs[i]->slice == -1)
        {
            evset->cs[i]->slice = get_address_slice(virtual_to_physical((uint64_t)evset->cs[i]));
        }

        if ((size_t)victim_llc_set == GET_LLC_SET_BITS(virtual_to_physical((uint64_t)evset->cs[i])) && victim_slice == evset->cs[i]->slice)
        {
            if (count < cache_evsets->target_size)
            {
                cheat_start[count] = evset->cs[i];
                count++;
            }
        }
        else
        {
            evset->cs[i]->set = 0;
        }
    }

    evset->cs = cheat_start;
    evset->size = count;
    return ret;
}

#define MAX_BACKTRACKS_GROUP_TEST 20
int alg_group_test(cache_evsets_t *cache_evsets, void *victim, evset_t *evset, int early_terminate)
{
    int ret = 0;
    elem_t **cands = evset->cs;
    size_t n_cands = evset->size;

    uint32_t n_grps = cache_evsets->target_size + 1;
    double sz_ratio = (double)cache_evsets->target_size / n_cands;
    double rate = (double)cache_evsets->target_size / n_grps; // from vila et al.
    int64_t n_backup = (size_t)(1.5 * log(sz_ratio) / log(rate));

    if (n_backup <= 0)
    {
        return 1;
    }

    size_t *sz_backup = calloc(n_backup, sizeof(*sz_backup));
    size_t bt_idx = 0, n_bctr = 0, max_backtrack = MAX_BACKTRACKS_GROUP_TEST;
    if (!sz_backup)
    {
        return 1;
    }

    int ooh = 0; // out of history
    while (n_cands > cache_evsets->target_size && n_bctr < max_backtrack && !ooh)
    {
        // printf("Target: %d | n_cands: %d | evsetsz: %d\n", cache_evsets->target_size, n_cands, evset->size);
        size_t _b_grpsz = n_cands / n_grps, rnd = n_cands % n_grps;
        size_t start_idx = 0, num_tests = _b_grpsz ? n_grps : n_cands;
        int has_remove = 0;
        for (uint32_t t = 0; t < num_tests; t++)
        {
            size_t grp_sz = _b_grpsz + (t < rnd);
            int is_lst_grp = t == num_tests - 1;

            // move the removed group to the back
            if (!is_lst_grp)
            {
                for (uint32_t i = 0; i < grp_sz; i++)
                {
                    _swap(cands[start_idx + i], cands[n_cands - 1 - i]);
                }
            }

            evset->size -= grp_sz;
            int res;
            if ((int64_t)evset->size <= 0)
            {
                res = 0;
            }
            else
            {
                int evicted = test_victim_eviction_avg(cache_evsets, victim, evset->cs, evset->size);
                res = evicted >= TEST_EVICTIONS;
            }

            if (res > 0)
            {
                // positive
                n_cands -= grp_sz;
                has_remove = 1;

                // backup states
                sz_backup[bt_idx] = grp_sz;
                bt_idx = (bt_idx + 1) % n_backup;

                if (early_terminate)
                {
                    break;
                }
            }
            else
            {
                evset->size += grp_sz;
                // erroneous state, backtrack!
                if (is_lst_grp && !has_remove)
                {
                    n_bctr += 1;
                    bt_idx = (bt_idx + n_backup - 1) % n_backup;
                    if (sz_backup[bt_idx] == 0)
                    {
                        ooh = 1;
                    }
                    else
                    {
                        n_cands += sz_backup[bt_idx];
                        sz_backup[bt_idx] = 0;
                    }
                    break;
                }

                // restore the pruned group
                if (!is_lst_grp)
                {
                    for (uint32_t i = 0; i < grp_sz; i++)
                    {
                        _swap(cands[start_idx + i], cands[n_cands - 1 - i]);
                    }
                }
                start_idx += grp_sz;
            }
        }
    }

    if (evset->size > cache_evsets->target_size)
        ret = 1;

    // evsz = _min(evset_capacity, n_cands);
    // memcpy(evset->addrs, cands, sizeof(*cands) * evsz);
    free(sz_backup);
    return ret;
}

size_t cache_uncertainty(address_state cache)
{
    size_t uncertainty = 0;
    size_t set_bits_under_ctrl = PAGE_BITS - CACHELINE_BITS;

    if (cache == L2)
    {
        if (set_bits_under_ctrl >= INDEX_OF_SET_BIT(L2_SETS))
            uncertainty = 0;
        else
            uncertainty = (1ULL << (INDEX_OF_SET_BIT(L2_SETS) - set_bits_under_ctrl)) * LLC_SLICES;
    }
    else if (cache == LLC)
    {
        if (set_bits_under_ctrl >= INDEX_OF_SET_BIT(LLC_SETS))
            uncertainty = LLC_SLICES;
        else
            uncertainty = (1ULL << (INDEX_OF_SET_BIT(LLC_SETS) - set_bits_under_ctrl)) * LLC_SLICES;
    }
    return uncertainty;
}

void shuffle_elements(elem_t **cs, size_t len)
{
    // Initialize random number generator
    srand(time(NULL));

    // Traverse the array and shuffle elements
    for (size_t i = len - 1; i > 0; --i)
    {
        size_t j = rand() % (i + 1); // Generate a random index in [0, i]
        if (i != j)
        {
            _swap(cs[i], cs[j]); // Swap elements at index i and j
        }
    }
}

// My attempt at implementing the binary search algorithm from https://github.com/zzrcxb/LLCFeasible
#define MAX_BACKTRACKS_BINARY_SEARCH 50
int alg_binary_search(cache_evsets_t *cache_evsets, void *target, evset_t *evset)
{
    size_t lower = evset->size;
    size_t upper = evset->size;

    size_t initial_size = evset->size;

    size_t w = cache_evsets->target_size;

    size_t uncertainty = cache_uncertainty(cache_evsets->cache);

    if (cache_evsets->flags & L2_CANDIDATE_SET_FILTERING || cache_evsets->flags & PERFECT_L2_CANDIDATE_SET_FILTERING)
    {
        uncertainty /= (L2_SETS / L1D_SETS);
    }
    if (cache_evsets->flags & SLICE_FILTERING || cache_evsets->flags & PERFECT_SLICE_FILTERING)
    {
        uncertainty /= LLC_SLICES;
    }

    size_t backtracks = 0;

    if (cache_evsets->cache == LLC && !IS_INCLUSIVE)
    {
        w++;
    }

    size_t i;
    for (i = 0; i < w;)
    {
        lower = i - 1;

        while ((upper - lower) > 1)
        {
            evset->size = (size_t)floor((double)(lower + upper) / 2.0);

            int evicted = test_victim_eviction_avg(cache_evsets, target, evset->cs, evset->size);
            int res = evicted >= TEST_EVICTIONS;

            if (res)
            {
                // printf("Target evicted     | n: %4ld | Lower: %4ld | Upper: %4ld\n", evset->size, lower, upper);
                upper = evset->size;
            }
            else
            {
                // printf("Target NOT evicted | n: %4ld | Lower: %4ld | Upper: %4ld\n", evset->size, lower, upper);
                lower = evset->size;
            }
        }
        size_t tau_i = upper - 1;
        // printf("Found congruent, swapping %ld and %ld\n", i, tau_i);

        _swap(evset->cs[i], evset->cs[tau_i]);
        // print_addr_info((uintptr_t)target);
        // print_addr_info((uintptr_t)evset->cs[i]);

        int evicted = test_victim_eviction_avg(cache_evsets, target, evset->cs, evset->size);
        int res = evicted >= TEST_EVICTIONS;
        if (!res)
        {
            // printf("AFTER Target not evicted\n");
            _swap(evset->cs[i], evset->cs[tau_i]);

            size_t step = 3 * uncertainty / 2;
            upper += step;
            if (upper > initial_size)
            {
                upper = initial_size;
            }

            backtracks++;
            if (backtracks > MAX_BACKTRACKS_BINARY_SEARCH)
            {
                evset->size = initial_size;
                return 1;
            }
            // printf("Lower: %4ld | Upper: %4ld\n", lower, upper);
        }
        else
        {
            i++;
        }
    }

    evset->size = i;

    if (evset->size >= cache_evsets->target_size)
    {
        evset->size = cache_evsets->target_size;
        shuffle_elements(evset->cs, evset->size);
    }
    else
    {
        return 1;
    }

    return 0;
}

int testev(void *target, elem_t **cands, size_t len, cache_evsets_t *cache_evsets)
{
    int evicted = test_victim_eviction_avg(cache_evsets, target, cands, len);
    return evicted >= TEST_EVICTIONS;
}

size_t prune_evcands(elem_t *target, elem_t **cands, size_t cnt, cache_evsets_t *cache_evsets)
{
    for (size_t i = 0; i < cnt;)
    {
        _swap(target, cands[i]);
        int tres = testev(target, cands, cnt, cache_evsets);
        _swap(target, cands[i]);
        if (tres == 0)
        {
            cnt -= 1;
            _swap(cands[i], cands[cnt]);
        }
        else
        {
            i += 1;
        }
    }
    return cnt;
}

// Modified from https://github.com/zzrcxb/LLCFeasible
int alg_binary_search_original(cache_evsets_t *cache_evsets, void *target, evset_t *evset)
{
    elem_t **cands = evset->cs;
    int64_t n_cands = evset->size, evsz = 0;

    int64_t n_ways = 0;
    uint32_t extra_cong = 0;
    uint32_t slack = 1;
    uint32_t capacity = 0;

    if (cache_evsets->cache == L2)
    {
        n_ways = L2_ASSOCIATIVITY;
        capacity = 2 * n_ways;
    }
    else if (cache_evsets->cache == LLC)
    {
        if (!IS_INCLUSIVE)
        {
            extra_cong = 1;
            slack = 2;
        }

        n_ways = LLC_ASSOCIATIVITY;
        capacity = 2 * n_ways;
    }

    uint32_t exp_evsz = n_ways + extra_cong;

    if (n_cands <= 1)
    {
        return 1;
    }

    size_t uncertainty = cache_uncertainty(cache_evsets->cache);

    if (cache_evsets->flags & L2_CANDIDATE_SET_FILTERING || cache_evsets->flags & PERFECT_L2_CANDIDATE_SET_FILTERING)
    {
        uncertainty /= (L2_SETS / L1D_SETS);
    }
    if (cache_evsets->flags & SLICE_FILTERING || cache_evsets->flags & PERFECT_SLICE_FILTERING)
    {
        uncertainty /= LLC_SLICES;
    }

    uint64_t migrated = n_cands - 1;
    int64_t num_carried_cong = n_ways - slack;
    int64_t lower = 0, upper = n_cands, cnt, n_bctr = 0, iters = 0;
    int is_reset = 0;
    while (evsz < capacity && n_bctr < (int64_t)MAX_BACKTRACKS_BINARY_SEARCH)
    {
        uint32_t offset = 0;
        if (slack && evsz > num_carried_cong)
        {
            offset = evsz - num_carried_cong;
        }

        if (evsz > 0 && !is_reset && evsz < n_ways)
        {
            uint32_t rem = n_ways - evsz; // rem > 0
            cnt = (upper * rem + lower) / (rem + 1);
            if (cnt == upper)
            {
                assert(cnt > 0);
                cnt -= 1;
            }
        }
        else
        {
            cnt = (upper + lower) / 2;
        }

        is_reset = 0;
        elem_t **cands_o = cands + offset;
        int has_pos = 0;
        while (upper - lower > 1)
        {
            int res = testev(target, cands_o, cnt - offset, cache_evsets);
            if (res > 0)
            {
                upper = cnt;
                has_pos = 1;
            }
            else
            {
                lower = cnt;
            }
            cnt = (upper + lower) / 2;
        }

        // lower is the largest number of candidates that CANNOT evict target;
        // while upper is the smallest # candidates that can; therefore,
        // upper-th element (cands[upper - 1]) is a congruent address
        iters += 1;

        // this is for error detection & backtracking, and handle a corner
        // case that the upper is a congruent line.
        // Note that if we have no pos, then upper is the old upper before
        // the binary search, that's why we use "upper - offset" here

        if (!has_pos && testev(target, cands_o, upper - offset, cache_evsets) < 0)
        {
            n_bctr += 1;
            is_reset = 1;
        }
        else
        {
            _swap(cands[evsz], cands[upper - 1]);
            evsz += 1;
        }

        if (evsz >= exp_evsz && testev(target, cands, evsz, cache_evsets) == 1)
        {
            evsz = prune_evcands((elem_t *)target, cands, evsz, cache_evsets);
            if (evsz >= exp_evsz)
            {
                break;
            }
            n_bctr += 1;
        }

        lower = evsz;

        if (is_reset || (slack && evsz > num_carried_cong))
        {
            if (upper >= (int64_t)migrated)
            {
                migrated = n_cands - 1;
            }

            size_t step = 3 * uncertainty / 2;
            for (size_t i = 0; i < step && upper < (int64_t)migrated; upper++, migrated--, i++)
            {
                _swap(cands[upper], cands[migrated]);
            }
        }

        if (upper <= lower)
        {
            // this can happen if upper == evsz + 1 but evsz cannot evict target
            // which is a result of flaky replacement policy
            upper = lower + 1;
            if (upper > (int64_t)n_cands)
            {
                break;
            }
        }
    }

    evset->size = evsz;

    if (evset->size >= cache_evsets->target_size)
    {
        evset->size = cache_evsets->target_size;
    }
    else
    {
        return 1;
    }

    memcpy(evset->cs, cands, sizeof(*cands) * evsz);
    return 0;
}

// Returns 0 for success, 1 for error
int prune_candidate_set(cache_evsets_t *cache_evsets, void *victim, evset_t *evset)
{
    int ret = 0;

    if (evset->size < cache_evsets->target_size)
        return 1;

    switch (cache_evsets->algorithm)
    {
    case L2_CHEAT:
        ret = l2_cheat(cache_evsets, victim, evset);
        break;
    case LLC_CHEAT:
        ret = llc_cheat(cache_evsets, victim, evset);
        break;
    case GROUP_TESTING_NEW:
        ret = alg_group_test(cache_evsets, victim, evset, 1);
        break;
    case GROUP_TESTING_OPTIMISED_NEW:
        ret = alg_group_test(cache_evsets, victim, evset, 0);
        break;
    case BINARY_SEARCH_BRADM:
        ret = alg_binary_search(cache_evsets, victim, evset);
        break;
    case BINARY_SEARCH_ORIGINAL:
        ret = alg_binary_search_original(cache_evsets, victim, evset);
        break;
    default:
        printf("prune_candidate_set(): algorithm not defined\n");
        exit(1);
    }

    if (evset->size != cache_evsets->target_size)
        return 1;

    return ret;
}
