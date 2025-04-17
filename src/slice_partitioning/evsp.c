#include <stdbool.h>

#include "slice_partitioning/comparator_gate.h"
#include "slice_partitioning/decision_tree.h"
#include "slice_partitioning/evsp.h"
#include "slice_partitioning/kmeans_wrapper.h"
#include "slice_partitioning/slicing.h"

#include "evsets/evsets.h"

int predict_slice_index_rdtscp(double *centroids, double data)
{
    double min_dist = INFINITY;
    int closest_cluster = -1;

    for (int i = 0; i < LLC_SLICES; i++)
    {
        double dist = fabs(centroids[i] - (double)data);
        if (dist < min_dist)
        {
            min_dist = dist;
            closest_cluster = i;
        }
    }

    return closest_cluster;
}

void evsp_calibrate(evsp_config_t evsp_config)
{
    struct timeval timer = start_timer();
    if (CG_MEASUREMENT)
    {
#if VERBOSITY >= 1
        printf("Calibrating for Comparator Gate\n");
#endif
        calibration_data_t *cd = NULL;
        if (CG_GROUND_TRUTH_CALIBRATE)
        {
            cd = comparator_gate_get_calibration_data_cheat(evsp_config->mem, evsp_config->mem_len);
        }
        else
        {
            cd = comparator_gate_get_calibration_data(evsp_config->mem, evsp_config->mem_len);
        }
        evsp_config->centroids = get_centroids(cd->data, cd->len);
        free(cd->data);
        free(cd);
    }
    else
    {
#if VERBOSITY >= 1
        printf("Calibrating for RDTSCP\n");
#endif
        evsp_config->centroids = calloc(LLC_SLICES, sizeof(double));
        int *sample_count = calloc(LLC_SLICES, sizeof(double));
        for (size_t cl = 0; cl < CALIBRATION_SAMPLES; cl++)
        {
            void *input = (void *)(evsp_config->mem + (cl * CACHELINE));
            int real_slice = get_address_slice(virtual_to_physical((uint64_t)input));
            double access_time = 0.0;
            int error = 0;
            for (int i = 0; i < MEASURE_SAMPLES; ++i)
            {
                set_addr_state((uintptr_t)input, LLC);
                memaccess((void *)((uintptr_t)(input) ^ 0x800));
                uint32_t temp = memaccesstime(input);
                if (temp < LLC_BOUND_LOW || temp > LLC_BOUND_HIGH)
                {
                    if (error >= 5)
                    {
                        ((elem_t *)(evsp_config->mem + (cl * CACHELINE)))->l2_evset = NULL;
                        error = 0;
                    }
                    i--;
                    error++;
                }
                else
                {
                    access_time += (double)temp;
                }
            }
            evsp_config->centroids[real_slice] += access_time / (double)MEASURE_SAMPLES;
            sample_count[real_slice]++;
        }
        for (int i = 0; i < LLC_SLICES; ++i)
        {
            evsp_config->centroids[i] /= (double)sample_count[i];
            printf("evsp_config->centroids[%d] = %f\n", i, evsp_config->centroids[i]);
        }
        free(sample_count);
    }

    double total_time = stop_timer(timer);
    printf("evsp_calibrate(): Calibration Complete in %.3fms\n", total_time * 1000);
}

evsp_config_t evsp_configure(void *mem, size_t num_pages, int num_slices, evsp_flags flags)
{
    evsp_config_t evsp_config = calloc(1, sizeof(struct evsp_config_s));
    evsp_config->num_pages = num_pages;
    evsp_config->num_addresses = (num_pages * PAGE_SIZE) / CACHELINE;
    evsp_config->num_slices = num_slices;
    evsp_config->flags = flags;
    evsp_config->mem = mem;
    evsp_config->mem_len = num_pages * PAGE_SIZE;

    for (int32_t cl = 0; cl < evsp_config->num_addresses; cl++)
    {
        elem_t *new_elem = (elem_t *)(mem + (cl * CACHELINE));
        new_elem->slice = -1;
        new_elem->cslice = -1;
    }

    struct timeval timer = start_timer();
    assert(N_BEST_ELEMENTS <= CL_PER_PAGE);
    evsp_config->decision_tree = build_decision_tree(N_BEST_ELEMENTS);
    // print_tree(evsp_config->decision_tree);
    double time_taken = stop_timer(timer);
    printf("evsp_configure(): Decision tree built in %.3fms\n", time_taken * 1000);

    if (evsp_config->flags & EVSP_FLAG_SLICES_GROUND_TRUTH)
    {
        evsp_config->cslices = calloc(evsp_config->num_addresses, sizeof(uint8_t));
        printf("Setting ground truth slices...\n");
        evsp_config->slices = calloc(evsp_config->num_addresses, sizeof(uint8_t));
        for (int32_t i = 0; i < (evsp_config->num_addresses); ++i)
        {
            void *input = mem + (i * CACHELINE);
            uint64_t p_address = virtual_to_physical((uint64_t)input);
            evsp_config->slices[i] = get_address_slice(p_address);
            evsp_config->cslices[i] = get_address_slice(p_address);
        }
        printf("Done\n");
    }

    evsp_calibrate(evsp_config);

    return evsp_config;
}

size_t bayesian_cl_when_found = 0;
void evsp_run(evsp_config_t evsp_config, void *mem)
{
    printf("evsp_run(): Finding slices for %d addresses\n", evsp_config->num_addresses);
    struct timeval timer = start_timer();

    // If we calibrated without ground truth then we can skip the first page as this was determined in the calibration step.
    int32_t start_cl = 0;
    if (!CG_GROUND_TRUTH_CALIBRATE)
    {
        start_cl = 64;
    }

    for (int32_t cl = start_cl; cl < evsp_config->num_addresses;)
    {
        if (cl % 10000 == 0)
        {
            printf("evsp_run(): Progress: %.1f%%\r", ((double)cl / (double)evsp_config->num_addresses) * 100.0);
        }

        void *address = (void *)(mem + (cl * CACHELINE));

        if (evsp_config->flags & EVSP_FLAG_BAYESIAN_PROPAGATION)
        {
            ((elem_t *)address)->cslice = evsp_get_address_slice_bayesian_inference(evsp_config, address, 1);
            cl += CL_PER_PAGE;
        }
        else if (evsp_config->flags & EVSP_FLAG_DECISION_TREE_PROPAGATION)
        {
            ((elem_t *)address)->cslice = evsp_get_address_slice_decision_tree(evsp_config, address, 1);
            cl += CL_PER_PAGE;
        }
        else if (evsp_config->flags & EVSP_FLAG_CLOSEST_MATCH_PROPAGATION)
        {
            ((elem_t *)address)->cslice = evsp_get_address_slice_closest_match(evsp_config, address, 1);
            cl += CL_PER_PAGE;
        }
        else
        {
            ((elem_t *)address)->cslice = evsp_get_address_slice_raw(evsp_config, address);
            cl++;
        }
    }
    putchar('\n');

    double total_time = stop_timer(timer);

    if (evsp_config->flags & EVSP_FLAG_BAYESIAN_PROPAGATION)
    {
        printf("evsp_run(): Average cachelines measured: %.3f\n", (double)bayesian_cl_when_found / ((double)evsp_config->num_addresses / (double)CL_PER_PAGE));
    }
    printf("evsp_run(): Addresses Partitioned: %d\n", evsp_config->num_addresses);
    printf("evsp_run(): Total Time: %fms\n", total_time * 1000);
    printf("evsp_run(): Addresses / second: %f\n", (double)evsp_config->num_addresses / (double)total_time);
}

int evsp_get_address_slice_bayesian_inference(evsp_config_t evsp_config, void *address, int update_cslices)
{
    uintptr_t page_offset = ((uintptr_t)address % PAGE_SIZE);
    void *page_address = (void *)((uintptr_t)address - page_offset);

    DecisionTreeNode_t *node = evsp_config->decision_tree;
    size_t cslices_offset = (size_t)(page_address - evsp_config->mem) / CACHELINE;

    void *addresses[CL_PER_PAGE] = {0};
    int slice_results[CL_PER_PAGE] = {0};
    double page_probabilities[node->unique_pages_count]; // Probabilities for each unique page
    double confidence_threshold = 0.90;                  // Confidence threshold to stop early

    // Initialise probabilities equally across all unique pages
    for (int i = 0; i < node->unique_pages_count; ++i)
    {
        page_probabilities[i] = 1.0 / node->unique_pages_count;
    }

    bool confident_match_found = false;
    int best_unique_page_index = -1;

    int i = 0;
    for (i = 0; i < CL_PER_PAGE && !confident_match_found; i += MIN_DISTANCE)
    {
        addresses[i] = (void *)(page_address + ((i % CL_PER_PAGE) * CACHELINE));
        slice_results[i] = evsp_get_address_slice_raw(evsp_config, addresses[i]);

        // Update probabilities for each unique page based on current slice result
        double total_probability = 0.0;
        for (int j = 0; j < node->unique_pages_count; ++j)
        {
            int match = (node->unique_pages[j][i] == slice_results[i]) ? 1 : 0;

            // Update the probability using Bayes' rule:
            // P(Page_j | Match) ∝ P(Match | Page_j) * P(Page_j)
            page_probabilities[j] *= (match) ? 0.95 : 0.05;
            total_probability += page_probabilities[j];
        }

        // Normalise probabilities after update
        for (int j = 0; j < node->unique_pages_count; ++j)
        {
            page_probabilities[j] /= total_probability;
            if (page_probabilities[j] >= confidence_threshold)
            {
                confident_match_found = true;
                best_unique_page_index = j;
                break;
            }
        }
    }

    // If no confident match found, choose the page with the highest probability
    if (!confident_match_found)
    {
        double max_probability = 0.0;
        for (int i = 0; i < node->unique_pages_count; ++i)
        {
            if (page_probabilities[i] > max_probability)
            {
                max_probability = page_probabilities[i];
                best_unique_page_index = i;
            }
        }
    }

    bayesian_cl_when_found += i;

    // Update cslices based on the best match found
    for (int p = 0; p < CL_PER_PAGE; p++)
    {
        if (update_cslices && evsp_config->flags & EVSP_FLAG_SLICES_GROUND_TRUTH)
            evsp_config->cslices[cslices_offset + p] = node->unique_pages[best_unique_page_index][p];
        ((elem_t *)(page_address + (p * CACHELINE)))->cslice = node->unique_pages[best_unique_page_index][p];
    }

    return ((elem_t *)(address))->cslice;
}

int evsp_get_address_slice_closest_match(evsp_config_t evsp_config, void *address, int update_cslices)
{
    uintptr_t page_offset = ((uintptr_t)address % PAGE_SIZE);
    void *page_address = (void *)((uintptr_t)address - page_offset);

    DecisionTreeNode_t *node = evsp_config->decision_tree;

    size_t cslices_offset = (size_t)(page_address - evsp_config->mem) / CACHELINE;

    void *addresses[CL_PER_PAGE] = {0};
    int slice_results[CL_PER_PAGE] = {0};

    for (int i = 0; i < CL_PER_PAGE; i++)
    {
        addresses[i] = (void *)(page_address + (i * CACHELINE));
        slice_results[i] = evsp_get_address_slice_raw(evsp_config, addresses[i]);
    }

    // Find the unique_page that matches most with slice_results
    int max_matches = 0;
    int best_unique_page_index = -1;

    for (int i = 0; i < node->unique_pages_count; ++i)
    {
        int current_matches = 0;

        for (int j = 0; j < CL_PER_PAGE; j++)
        {
            // Check if the expected slice at j for the current unique_page matches slice_results[j]
            if (node->unique_pages[i][j] == slice_results[j])
            {
                current_matches++;
            }
        }

        // Update the best match if the current page has more matches
        if (current_matches > max_matches)
        {
            max_matches = current_matches;
            best_unique_page_index = i;
        }
    }

    for (int p = 0; p < CL_PER_PAGE; p++)
    {
        if (update_cslices && evsp_config->flags & EVSP_FLAG_SLICES_GROUND_TRUTH)
            evsp_config->cslices[cslices_offset + p] = node->unique_pages[best_unique_page_index][p];
        ((elem_t *)(page_address + (p * CACHELINE)))->cslice = node->unique_pages[best_unique_page_index][p];
    }

    return ((elem_t *)(address))->cslice;
}

int overall_error = 0;
int evsp_get_address_slice_decision_tree(evsp_config_t evsp_config, void *address, int update_cslices)
{
    uintptr_t page_offset = ((uintptr_t)address % PAGE_SIZE);
    void *page_address = (void *)((uintptr_t)address - page_offset);

    size_t cslices_offset = (size_t)(page_address - evsp_config->mem) / CACHELINE;

    DecisionTreeNode_t *node = evsp_config->decision_tree;
    void *addresses[N_BEST_ELEMENTS];

    int slice_results[N_BEST_ELEMENTS] = {0};

    int minor_error = 0;
    size_t iterations = 0;

    double slice_averages[CL_PER_PAGE] = {0};
    int slice_counts[CL_PER_PAGE] = {0};

    while (1)
    {
        if (overall_error >= evsp_config->num_pages * 5)
        {
            fprintf(stderr, "evsp_get_address_slice_decision_tree(): Too many errors, better luck next time...\n", overall_error);
            exit(1);
        }

        if (iterations >= 50)
        {
            return evsp_get_address_slice_closest_match(evsp_config, address, update_cslices);
        }

        for (int i = 0; i < N_BEST_ELEMENTS; ++i)
        {
            addresses[i] = (void *)(page_address + ((node->elements[i]) * CACHELINE));
            slice_results[i] = evsp_get_address_slice_raw(evsp_config, addresses[i]);
            slice_averages[i] += slice_results[i];
            slice_counts[i]++;
        }

        int max_match = -1;
        int slice_index = -1;
        for (int i = 0; i < LLC_SLICES; ++i)
        {
            int match = 0;
            for (int e = 0; e < N_BEST_ELEMENTS; ++e)
            {
                if (node->expected_slices[i][e] == slice_results[e])
                {
                    match++;
                }
            }
            if (match > max_match)
            {
                max_match = match;
                slice_index = i;
            }
        }

        // If we don't get any matches at all, then repeat and add to errors.
        if (max_match == 0)
        {
            if (minor_error >= 2)
            {
                for (int i = 0; i < N_BEST_ELEMENTS; ++i)
                {
                    ((elem_t *)addresses[i])->l2_evset = NULL;
                }
                if (node->parent != NULL)
                {
                    node = node->parent;
                    if (node->parent != NULL)
                        node = node->parent;
                }
                overall_error++;
            }
            minor_error++;
            continue;
        }

        // If the slice at node->element is what we expect
        if (node->children[slice_index] != NULL)
        {
            if (node->children[slice_index]->permutation != NULL)
            {
                for (int p = 0; p < CL_PER_PAGE; ++p)
                {
                    if (update_cslices && evsp_config->flags & EVSP_FLAG_SLICES_GROUND_TRUTH)
                        evsp_config->cslices[cslices_offset + p] = node->children[slice_index]->permutation[p];
                    ((elem_t *)(page_address + (p * CACHELINE)))->cslice = node->children[slice_index]->permutation[p];
                }
                minor_error = 0;
                break;
            }
            // If we have a branch, go to this branch and repeat
            else
            {
                node = node->children[slice_index];
                minor_error = 0;
            }
        }
        // Branch error.
        else
        {
            if (minor_error >= 2)
            {
                for (int i = 0; i < N_BEST_ELEMENTS; ++i)
                {
                    ((elem_t *)addresses[i])->l2_evset = NULL;
                }
                if (node->parent != NULL)
                {
                    node = node->parent;
                    if (node->parent != NULL)
                        node = node->parent;
                }
                overall_error++;
            }
            minor_error++;
        }
        iterations++;
    }
    // printf("Iterations: %4d | Minor Error: %4d | Overall Error: %4d\n", iterations, minor_error, overall_error);
    return ((elem_t *)(address))->cslice;
}

int evsp_get_address_slice_raw(evsp_config_t evsp_config, void *address)
{
    int slice = -1;
    if (CG_MEASUREMENT)
    {
        int gate_output[LLC_SLICES] = {0};
        comparator_gate(address, (int *)gate_output);
        slice = predict_slice_index(evsp_config->centroids, (int *)gate_output);
    }
    else
    {
        double access_time = 0.0;
        int error = 0;
        for (int i = 0; i < MEASURE_SAMPLES; ++i)
        {
            set_addr_state((uintptr_t)address, LLC);
            uint32_t temp = memaccesstime(address);
            if (temp < LLC_BOUND_LOW || temp > LLC_BOUND_HIGH)
            {
                if (error >= 5)
                {
                    ((elem_t *)(address))->l2_evset = NULL;
                    error = 0;
                }
                i--;
                error++;
            }
            else
            {
                access_time += (double)temp;
            }
        }
        access_time /= (double)MEASURE_SAMPLES;
        slice = predict_slice_index_rdtscp(evsp_config->centroids, access_time);
    }
    return slice;
}

void evsp_verify(evsp_config_t evsp_config)
{
    if (!(evsp_config->flags & EVSP_FLAG_SLICES_GROUND_TRUTH))
    {
        printf("Setting ground truth slices...\n");
        evsp_config->slices = calloc(evsp_config->num_addresses, sizeof(uint8_t));
        for (int32_t i = 0; i < (evsp_config->num_addresses); ++i)
        {
            void *input = evsp_config->mem + (i * CACHELINE);
            uint64_t p_address = virtual_to_physical((uint64_t)input);
            evsp_config->slices[i] = get_address_slice(p_address);
        }
        printf("Done\n");
    }
    printf("Errors: %d\n", overall_error);
    // Initialize variables for counting
    size_t *bin_counts = calloc(evsp_config->num_slices, sizeof(size_t));
    size_t *slice_bin_counts = calloc(evsp_config->num_slices * evsp_config->num_slices, sizeof(size_t));

    elem_t **measuring_sticks = get_measuring_sticks();
    int measuring_stick_slices[LLC_SLICES] = {0};

    for (int s = 0; s < LLC_SLICES; ++s)
    {
        uint64_t p_address = virtual_to_physical((uint64_t)measuring_sticks[s]);
        measuring_stick_slices[s] = get_address_slice(p_address);
    }

    // Count occurrences of slices and slice pairs
    for (int i = 0; i < evsp_config->num_addresses; ++i)
    {
        elem_t *temp = (elem_t *)((uintptr_t)evsp_config->mem + (i * CACHELINE));
        int count = 0;
        for (int s = 0; s < LLC_SLICES; ++s)
        {
            if (measuring_sticks[s]->cslice == temp->cslice)
            {
                bin_counts[measuring_stick_slices[s]]++;
                slice_bin_counts[(measuring_stick_slices[s] * evsp_config->num_slices) + evsp_config->slices[i]]++;
                count++;
            }
        }
        if (count > 1)
        {
            for (int l = 0; l < LLC_SLICES; ++l)
            {
                printf("%d %d %d %d\n", l, temp->cslice, measuring_sticks[l]->cslice, measuring_stick_slices[l]);
            }
            exit(1);
        }
    }

    // Analyze and print results
    double total_accuracy = 0;

    for (int s = 0; s < evsp_config->num_slices; ++s)
    {
        double max_accuracy = 0;

        // Find the maximum accuracy within the slice
        for (int i = 0; i < evsp_config->num_slices; ++i)
        {
            double accuracy = ((double)slice_bin_counts[(s * evsp_config->num_slices) + i] / (double)bin_counts[s]) * 100;
            if (max_accuracy < accuracy)
                max_accuracy = accuracy;
        }

        total_accuracy += max_accuracy;

        // Print bin counts and accuracy details
        printf("Bin %d: %ld (%5.2f%%)\t", s, bin_counts[s], max_accuracy);

        for (int i = 0; i < evsp_config->num_slices; ++i)
        {
            printf("%6ld", slice_bin_counts[(s * evsp_config->num_slices) + i]);
            if (i < evsp_config->num_slices - 1)
                putchar(',');
            else
                printf("\t\t");
        }

        for (int i = 0; i < evsp_config->num_slices; ++i)
        {
            double per_bin_slice_count = 0;

            // Calculate per_bin_slice_count
            for (int b = 0; b < evsp_config->num_slices; ++b)
            {
                per_bin_slice_count += slice_bin_counts[(b * evsp_config->num_slices) + s];
            }

            // Calculate accuracy and print
            double accuracy = ((double)slice_bin_counts[(s * evsp_config->num_slices) + i] / (double)per_bin_slice_count) * 100;
            printf("%6.2f%%", accuracy);

            if (i < evsp_config->num_slices - 1)
                putchar(',');
            else
                putchar('\n');
        }
    }

    // Print average accuracy
    printf("Average Accuracy: %f%%\n", total_accuracy / evsp_config->num_slices);

    // Initialize net counts
    size_t net_true_positive = 0;
    size_t net_false_positive = 0;
    size_t net_false_negative = 0;
    size_t net_true_negative = 0;

    double total_precision = 0;
    double total_recall = 0;
    double total_f1_score = 0;

    // Calculate and print one-vs-all confusion matrix, precision, recall, and F1-score
    printf("\nClass\tTP\tFP\tFN\tTN\tPrecision\tRecall\t\tF1-score\n");

    for (int s = 0; s < evsp_config->num_slices; ++s)
    {
        size_t true_positive = slice_bin_counts[s * evsp_config->num_slices + s];
        size_t false_positive = 0;
        size_t false_negative = 0;
        size_t true_negative = 0;

        for (int i = 0; i < evsp_config->num_slices; ++i)
        {
            if (i != s)
            {
                false_positive += slice_bin_counts[s * evsp_config->num_slices + i];
                false_negative += slice_bin_counts[i * evsp_config->num_slices + s];
            }
        }

        true_negative = evsp_config->num_addresses - (true_positive + false_positive + false_negative);

        double precision = (true_positive + false_positive) ? (double)true_positive / (true_positive + false_positive) : 0;
        double recall = (true_positive + false_negative) ? (double)true_positive / (true_positive + false_negative) : 0;
        double f1_score = (precision + recall) ? 2 * (precision * recall) / (precision + recall) : 0;

        printf("%d\t%ld\t%ld\t%ld\t%ld\t%8.2f%%\t%8.2f%%\t%8.2f%%\n", s, true_positive, false_positive, false_negative, true_negative, precision * 100, recall * 100, f1_score * 100);

        net_true_positive += true_positive;
        net_false_positive += false_positive;
        net_false_negative += false_negative;
        net_true_negative += true_negative;

        total_precision += precision;
        total_recall += recall;
        total_f1_score += f1_score;
    }

    double macro_precision = total_precision / evsp_config->num_slices;
    double macro_recall = total_recall / evsp_config->num_slices;
    double macro_f1_score = total_f1_score / evsp_config->num_slices;

    printf("\nNet\tTP\tFP\tFN\tTN\tPrecision\tRecall\t\tF1-score\n");
    printf("Net\t%ld\t%ld\t%ld\t%ld\n", net_true_positive, net_false_positive, net_false_negative, net_true_negative);
    printf("Macro\t\t\t\t\t%8.2f%%\t%8.2f%%\t%8.2f%%\n", macro_precision * 100, macro_recall * 100, macro_f1_score * 100);

    free(bin_counts);
    free(slice_bin_counts);
}

void evsp_verify_csv(evsp_config_t evsp_config, FILE *fp)
{
    // Initialize variables for counting
    size_t *bin_counts = calloc(evsp_config->num_slices, sizeof(size_t));
    size_t *slice_bin_counts = calloc(evsp_config->num_slices * evsp_config->num_slices, sizeof(size_t));

    // Count occurrences of slices and slice pairs
    for (int i = 0; i < evsp_config->num_addresses; ++i)
    {
        bin_counts[evsp_config->cslices[i]]++;
        slice_bin_counts[(evsp_config->cslices[i] * evsp_config->num_slices) + evsp_config->slices[i]]++;
    }

    // Analyze and print results
    double total_accuracy = 0;

    for (int s = 0; s < evsp_config->num_slices; ++s)
    {
        double max_accuracy = 0;

        // Find the maximum accuracy within the slice
        for (int i = 0; i < evsp_config->num_slices; ++i)
        {
            double accuracy = ((double)slice_bin_counts[(s * evsp_config->num_slices) + i] / (double)bin_counts[s]) * 100;
            if (max_accuracy < accuracy)
                max_accuracy = accuracy;
        }

        total_accuracy += max_accuracy;

        // Print bin counts and accuracy details
        fprintf(fp, "Bin %d,", s);

        for (int i = 0; i < evsp_config->num_slices; ++i)
        {
            fprintf(fp, "%6ld", slice_bin_counts[(s * evsp_config->num_slices) + i]);
            if (i < evsp_config->num_slices - 1)
                fputc(',', fp);
            else
                fprintf(fp, "\n");
        }
    }

    free(bin_counts);
    free(slice_bin_counts);
}

void evsp_release(evsp_config_t evsp_config)
{
    if (evsp_config != NULL)
    {
        if (evsp_config->flags & EVSP_FLAG_SLICES_GROUND_TRUTH)
            free(evsp_config->slices);
        free(evsp_config->cslices);
        free(evsp_config->centroids);
        free_decision_tree(evsp_config->decision_tree);
        free(evsp_config);
    }
}