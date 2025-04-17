#include "evsets/evsets_helpers.h"

uint32_t test_victim_eviction_L2(void *victim, elem_t **cs, size_t len)
{
    uint32_t time = 0;
    if (cs == NULL)
        return -1;

    for (int i = 0; i < 1; ++i)
    {
        time = 0;
        memaccess(victim);
        memaccess(victim);
        memaccess(victim);
        memaccess(victim);

        L2_TRAVERSE(cs, len, L2_TRAVERSE_REPEATS);

        // Page walk i.e. put address of page into TLB
        memaccess((void *)((uintptr_t)victim ^ 0x800));

        uint32_t temp = memaccesstime(victim);
        // Throwaway garbage result over LLC high bound cycles
        if (temp > LLC_BOUND_HIGH)
        {
            i--;
        }
        else
            time += temp;
    }
    return time;
}

uint32_t test_victim_eviction_LLC(void *victim, elem_t **cs, size_t len)
{
    uint32_t time = 0;
    if (cs == NULL)
        return -1;

    for (int i = 0; i < 1; ++i)
    {
        time = 0;
        memaccess(victim);
        memaccess(victim);
        memaccess(victim);
        memaccess(victim);

        LLC_TRAVERSE(cs, len, LLC_TRAVERSE_REPEATS);

        // Page walk i.e. put address of page into TLB
        memaccess((void *)((uintptr_t)victim ^ 0x800));

        uint32_t temp = memaccesstime(victim);
        // Throwaway garbage result over 1000 cycles
        if (temp > RAM_BOUND_HIGH)
        {
            i--;
        }
        else
            time += temp;
    }
    return time;
}

// I couldn't really get this working well on 12th-14th Gen.
uint32_t test_victim_eviction_LLC_non_inclusive(void *victim, elem_t **cs, size_t len, evset_t *l2_evset)
{
    uint32_t time = 0;
    if (cs == NULL)
        return -1;

    for (int i = 0; i < 1; ++i)
    {
        time = 0;
        memaccess(victim);
        memaccess(victim);
        memaccess(victim);
        memaccess(victim);

        skx_sf_cands_traverse_st(cs, len, l2_evset);
        // traverse_array_with_l2_occupy_set(cs, len, LLC_TRAVERSE_REPEATS, l2_evset);
        // LLC_TRAVERSE(cs, len, LLC_TRAVERSE_REPEATS);

        // Page walk i.e. put address of page into TLB
        memaccess((void *)((uintptr_t)victim ^ 0x800));

        uint32_t temp = memaccesstime(victim);
        // Throwaway garbage result over 1000 cycles
        if (temp > RAM_BOUND_HIGH)
        {
            i--;
        }
        else
            time += temp;
    }
    return time;
}

// Modified from https://github.com/zzrcxb/LLCFeasible
void generic_cands_traverse(elem_t **cands, size_t cnt, size_t ev_repeat)
{
    // traverse backwards to prevent speculative execution to overshoot
    for (size_t r = 0; r < ev_repeat; r++)
    {
        access_array_bwd(cands, cnt);
    }
}

// Modified from https://github.com/zzrcxb/LLCFeasible
uint32_t generic_test_eviction(elem_t *target, elem_t **cands, size_t cnt, evset_t *l2_evset, int sf)
{
    elem_t *tlb_target = tlb_warmup_ptr(target);
    uint32_t otc = 0, aux_before, aux_after;

    uint32_t trials = 4;
    if (sf)
        trials = 5;

    // hardcoded values for now.
    uint32_t lat_thresh = 145;
    uint32_t low_bnd = 2;
    uint32_t upp_bnd = 2;
    if (sf)
    {
        lat_thresh = 63;
        low_bnd = 2;
        upp_bnd = 3;
    }

    int flush_cands = 0;
    if (sf)
        flush_cands = 1;

    uint32_t unsure_retry = 3;
    if (sf)
        unsure_retry = 5;
    uint32_t access_cnt = 1;

    int lower_ev = 1;
    if (sf)
        lower_ev = 0;

    for (uint32_t r = 0; r < unsure_retry; r++)
    {
        otc = 0;
        for (uint32_t i = 0; i < trials;)
        {
            _rdtscp_aux(&aux_before);

            clflush(target); // flush it so it gets an insertion age
            if (flush_cands)
            {
                flush_array(cands, cnt);
            }
            lfence();
            // load the target line
            for (uint32_t j = 0; j < access_cnt; j++)
            {
                // may use lower ev to causing repeated access to upper levels
                if (lower_ev)
                {
                    generic_cands_traverse(l2_evset->cs, l2_evset->size, L2_TRAVERSE_REPEATS);
                }
                lfence();
                memaccess(target);
                // if (tconf->need_helper)
                // {
                //     helper_thread_read_single(target, tconf->hctrl);
                //     memaccess(target);
                // }
            }

            // traverse candidates
            lfence();
            // if (tconf->foreign_evictor)
            // {
            //     helper_thread_traverse_cands(cands, cnt, tconf);
            // }
            // else
            {
                if (sf)
                    generic_cands_traverse(cands, cnt, 10);
                else
                    skx_sf_cands_traverse_st(cands, cnt, l2_evset);
            }
            lfence();

            // warmup TLB then time target
            memaccess(tlb_target);
            uint64_t UNUSED _tmp;
            uint64_t lat = _time_maccess_aux(target, _tmp, aux_after);
            if (aux_before == aux_after && lat < 940)
            // if (aux_before == aux_after && lat < detected_cache_lats.interrupt_thresh)
            {
                otc += (lat >= lat_thresh);
                i += 1;
                if (otc > upp_bnd)
                {
                    return 1;
                }
            }
        }

        if (otc > upp_bnd)
        {
            return 1;
        }
        else if (otc < low_bnd)
        {
            return 0;
        }
    }

    if (otc >= (low_bnd + upp_bnd) / 2)
    {
        return 0;
    }
    else
    {
        return 0;
    }
}

int test_victim_eviction_avg(cache_evsets_t *cache_evsets, void *victim, elem_t **cs, size_t len)
{
    int tries = 0;
    for (int i = 0; i < TEST_EVICTIONS; ++i)
    {
        uint32_t time = 0;
        if (cache_evsets->cache == L2)
        {
            time = test_victim_eviction_L2(victim, cs, len);
        }
        else if (cache_evsets->cache == LLC && IS_INCLUSIVE)
        {
            time = test_victim_eviction_LLC(victim, cs, len);
        }
        else if (cache_evsets->cache == LLC && !IS_INCLUSIVE)
        {
            // time = test_victim_eviction_LLC_non_inclusive(victim, cs, len, ((elem_t *)victim)->l2_evset);
            int res = generic_test_eviction(victim, cs, len, ((elem_t *)victim)->l2_evset, 0);
            // This is so hacky but it is just for massaging for now.
            if (res)
                time = cache_evsets->measure_low + 1;
        }
        else if (cache_evsets->cache == SF && !IS_INCLUSIVE)
        {
            int res = generic_test_eviction(victim, cs, len, ((elem_t *)victim)->l2_evset, 1);
            // This is so hacky but it is just for massaging for now.
            if (res)
                time = cache_evsets->measure_low + 1;
        }

        if (time > cache_evsets->measure_low && time < cache_evsets->measure_high)
            tries++;
    }
    return tries;
}

void delete_element(evset_t *evset, size_t index)
{
    if (index >= evset->size)
    {
        return; // Index out of bounds
    }

    elem_t *elem = evset->cs[index];
    elem->set = 0;

    // Swap with last element and reduce size.
    _swap(elem, evset->cs[evset->size - 1]);

    evset->size--;
}

void clear_array(evset_t *evset)
{
    if (evset->cs)
    {
        for (size_t i = 0; i < evset->size; i++)
        {
            evset->cs[i]->set = 0;
        }
    }
}

int get_num_l1_same_set(void *victim, evset_t *evset)
{
    int count = 0;
    for (size_t i = 0; i < evset->size; i++)
    {
        uint32_t elem_l2_set = GET_L1D_SET_BITS(virtual_to_physical((uint64_t)evset->cs[i]));
        if (elem_l2_set == GET_L1D_SET_BITS(virtual_to_physical((uint64_t)victim)))
            count++;
    }
    return count;
}

// Cheat mode activate
int get_num_l2_same_set(void *victim, evset_t *evset)
{
    int count = 0;
    uint32_t victim_l2_set = GET_L2_SET_BITS(virtual_to_physical((uint64_t)victim));
    for (size_t i = 0; i < evset->size; i++)
    {
        uint32_t elem_l2_set = GET_L2_SET_BITS(virtual_to_physical((uint64_t)evset->cs[i]));
        if (elem_l2_set == victim_l2_set)
            count++;
    }
    return count;
}

int get_num_llc_same_slice(void *victim, evset_t *evset)
{
    int count = 0;
    for (size_t i = 0; i < evset->size; i++)
    {
        int elem_llc_slice = get_address_slice(virtual_to_physical((uint64_t)evset->cs[i]));
        if (elem_llc_slice == get_address_slice(virtual_to_physical((uint64_t)victim)))
            count++;
    }
    return count;
}

int get_num_llc_same_set(void *victim, evset_t *evset)
{
    int count = 0;
    for (size_t i = 0; i < evset->size; i++)
    {
        uint32_t elem_llc_set = GET_LLC_SET_BITS(virtual_to_physical((uint64_t)evset->cs[i]));
        if (elem_llc_set == GET_LLC_SET_BITS(virtual_to_physical((uint64_t)victim)))
            count++;
    }
    return count;
}

int get_num_same_slice(void *victim, evset_t *evset)
{
    int count = 0;
    for (size_t i = 0; i < evset->size; i++)
    {
        int elem_llc_slice = get_address_slice(virtual_to_physical((uint64_t)evset->cs[i]));
        if (elem_llc_slice == get_address_slice(virtual_to_physical((uint64_t)victim)))
            count++;
    }
    return count;
}

int get_num_llc_same_slice_and_set(void *victim, evset_t *evset)
{
    int count = 0;
    for (size_t i = 0; i < evset->size; i++)
    {
        int elem_llc_slice = get_address_slice(virtual_to_physical((uint64_t)evset->cs[i]));
        uint32_t elem_llc_set = GET_LLC_SET_BITS(virtual_to_physical((uint64_t)evset->cs[i]));
        if (elem_llc_slice == get_address_slice(virtual_to_physical((uint64_t)victim)) && elem_llc_set == GET_LLC_SET_BITS(virtual_to_physical((uint64_t)victim)))
            count++;
    }
    return count;
}

int get_num_l2_same_slice_and_set(void *victim, evset_t *evset)
{
    int count = 0;
    for (size_t i = 0; i < evset->size; i++)
    {
        int elem_llc_slice = get_address_slice(virtual_to_physical((uint64_t)evset->cs[i]));
        uint32_t elem_l2_set = GET_L2_SET_BITS(virtual_to_physical((uint64_t)evset->cs[i]));
        if (elem_llc_slice == get_address_slice(virtual_to_physical((uint64_t)victim)) && elem_l2_set == GET_L2_SET_BITS(virtual_to_physical((uint64_t)victim)))
            count++;
    }
    return count;
}

int get_num_l2_same_slice_and_set_user(void *victim, evset_t *evset)
{
    int count = 0;
    for (size_t i = 0; i < evset->size; i++)
    {
        int elem_llc_slice = evset->cs[i]->cslice;
        evset_t *elem_l2_evset = evset->cs[i]->l2_evset;
        if (elem_llc_slice == ((elem_t *)victim)->cslice && elem_l2_evset == ((elem_t *)victim)->l2_evset)
            count++;
    }
    return count;
}

void print_evset_info(void *victim, evset_t *evset)
{
    uintptr_t victim_paddr = virtual_to_physical((uint64_t)victim);
    uintptr_t head_paddr = virtual_to_physical((uint64_t)evset->cs[0]);
    printf("Victim 0x%010lx | Px%010lx | L1D_SET: %2ld | L2_SET: %4ld | LLC_SET: %4ld | SLICE: %d | CSLICE: %d\n", (uintptr_t)victim, victim_paddr, GET_L1D_SET_BITS(victim_paddr), GET_L2_SET_BITS(victim_paddr), GET_LLC_SET_BITS(victim_paddr), get_address_slice(victim_paddr), ((elem_t *)victim)->cslice);
    printf("Head   0x%010lx | Px%010lx | L1D_SET: %2ld | L2_SET: %4ld | LLC_SET: %4ld | SLICE: %d | CSLICE: %d\n", (uintptr_t)evset->cs[0], head_paddr, GET_L1D_SET_BITS(head_paddr), GET_L2_SET_BITS(head_paddr), GET_LLC_SET_BITS(head_paddr), get_address_slice(head_paddr), ((elem_t *)evset->cs[0])->cslice);
    printf("Evset_len: %5ld | L1D_SS: %3d  |  L2_SS: %3d | SSl: %3d | LLC_SS: %3d | L2_SS_SSl: %3d | LLC_SS_SSl: %2d\n\n", evset->size, get_num_l1_same_set(victim, evset), get_num_l2_same_set(victim, evset), get_num_same_slice(victim, evset), get_num_llc_same_set(victim, evset), get_num_l2_same_slice_and_set(victim, evset), get_num_llc_same_slice_and_set(victim, evset));
}

void print_evset(evset_t *evset)
{
    for (size_t i = 0; i < evset->size; ++i)
    {
        print_addr_info((uintptr_t)evset->cs[i]);
    }
}

void print_evset_mirror(evset_t *evset, int page_offset)
{
    for (size_t i = 0; i < evset->size; ++i)
    {
        elem_t *mirror_elem = (elem_t *)((uintptr_t)evset->cs[i] + (page_offset * CACHELINE));
        print_addr_info((uintptr_t)mirror_elem);
    }
}

char *evset_alg_to_str(evset_algorithm alg)
{
    switch (alg)
    {
    case L2_CHEAT:
        return "L2_CHEAT";
        break;
    case LLC_CHEAT:
        return "LLC_CHEAT";
        break;
    case GROUP_TESTING_NEW:
        return "GROUP_TESTING_NEW";
    case GROUP_TESTING_OPTIMISED_NEW:
        return "GROUP_TESTING_OPTIMISED_NEW";
        break;
    default:
        return "UNKNOWN";
    }
}