#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "slice_partitioning/decision_tree.h"
#include "slice_partitioning/slicing.h"
#include "util/util.h"

static DecisionTreeNode_t *decision_tree = NULL;

DecisionTreeNode_t *get_decision_tree()
{
    if (decision_tree == NULL)
    {
        decision_tree = build_decision_tree(N_BEST_ELEMENTS);
    }
    return decision_tree;
}

void get_page_permutation(uint64_t page, int *page_slices)
{
    for (int cl = 0; cl < CL_PER_PAGE; ++cl)
    {
        page_slices[cl] = get_address_slice((page * PAGE_SIZE) + (cl * CACHELINE));
    }
}

// Function to create a new node
DecisionTreeNode_t *create_node(int *elements, int num_elements, int **expected_slices, int *permutation, DecisionTreeNode_t *parent)
{
    // Allocate memory for the new node
    DecisionTreeNode_t *node = (DecisionTreeNode_t *)malloc(sizeof(DecisionTreeNode_t));
    if (node == NULL)
    {
        // Handle allocation failure
        fprintf(stderr, "DecisionTreeError: Failed to allocate memory for decision tree node\n");
        exit(1);
    }
    node->elements = elements;
    node->num_elements = num_elements;
    node->expected_slices = expected_slices;
    node->parent = parent;
    node->children = NULL;
    node->permutation = permutation;
    node->unique_pages = NULL;
    node->unique_pages_count = 0;
    return node;
}

// Recursive function to build the decision tree
DecisionTreeNode_t *build_decision_tree_recurse(int **permutations, int num_permutations, int *consumed_elements, int *remaining_elements, int num_remaining_elements, DecisionTreeNode_t *parent, int num_best_elements)
{
    // Base case: if there's only one permutation left, create a leaf node
    if (num_permutations == 1)
    {
        int *elements = calloc(1, sizeof(int));
        elements[0] = -1;
        return create_node(elements, 1, NULL, permutations[0], parent);
    }

    double element_entropies[CL_PER_PAGE];
    // Calculate entropy for each remaining element
    for (int i = 0; i < num_remaining_elements; ++i)
    {
        int element_slice = remaining_elements[i];
        int slice_counts[LLC_SLICES] = {0};
        // Count the occurrences of each value for the current element
        for (int j = 0; j < num_permutations; ++j)
        {
            slice_counts[permutations[j][element_slice]]++;
        }

        // Calculate entropy
        double entropy = 0.0;
        for (int j = 0; j < LLC_SLICES; ++j)
        {
            if (slice_counts[j] > 0)
            {
                double p = (double)slice_counts[j] / num_permutations;
                entropy -= p * log2(p);
            }
        }
        element_entropies[i] = entropy;
    }

    // Find the indices of the elements with the highest entropy that are separated by a certain distance
    int *best_elements = calloc(num_best_elements, sizeof(int));
    double *best_entropies = calloc(num_best_elements, sizeof(double));

    for (int i = 0; i < num_best_elements; ++i)
    {
        best_elements[i] = -1;
        best_entropies[i] = -1.0;
    }

    for (int i = 0; i < num_remaining_elements; ++i)
    {
        int valid = 1;
        // Ensure the selected element is at least min_distance away from previously selected elements
        for (int j = 0; j < num_best_elements && best_elements[j] >= 0; ++j)
        {
            if (abs(remaining_elements[i] - best_elements[j]) < MIN_DISTANCE)
            {
                valid = 0;
                break;
            }
        }

        // If the element is valid (meets the distance condition), check if its entropy is among the best
        if (valid)
        {
            for (int j = 0; j < num_best_elements; ++j)
            {
                if (element_entropies[i] > best_entropies[j])
                {
                    for (int k = num_best_elements - 1; k > j; --k)
                    {
                        best_elements[k] = best_elements[k - 1];
                        best_entropies[k] = best_entropies[k - 1];
                    }
                    best_elements[j] = remaining_elements[i];
                    best_entropies[j] = element_entropies[i];
                    break;
                }
            }
        }
    }

    // Partition permutations based on the best elements
    int **partitions[LLC_SLICES] = {NULL};
    int partition_sizes[LLC_SLICES] = {0};

    for (int i = 0; i < num_permutations; ++i)
    {
        if (best_elements[0] >= 0)
        {
            int best_slice = permutations[i][best_elements[0]];
            if (partition_sizes[best_slice] == 0)
            {
                partitions[best_slice] = (int **)malloc(num_permutations * sizeof(int *));
                if (partitions[best_slice] == NULL)
                {
                    // Handle allocation failure
                    fprintf(stderr, "DecisionTreeError: Failed to allocate memory for partitions[%d]\n", best_slice);
                    exit(1);
                }
            }
            partitions[best_slice][partition_sizes[best_slice]++] = permutations[i];
        }
    }

    int **expected_slices = calloc(LLC_SLICES, sizeof(int *));
    for (int i = 0; i < LLC_SLICES; ++i)
    {
        expected_slices[i] = calloc(num_best_elements, sizeof(int));
    }

    // Create the current node
    DecisionTreeNode_t *node = create_node(best_elements, num_best_elements, expected_slices, NULL, parent);
    node->children = (DecisionTreeNode_t **)malloc(LLC_SLICES * sizeof(DecisionTreeNode_t *));
    if (node->children == NULL)
    {
        // Handle allocation failure
        fprintf(stderr, "DecisionTreeError: Failed to allocate memory for node children\n");
        exit(1);
    }

    // Recursively build child nodes by splitting the unique permutations into partitions.
    // Partitions with at least one or more permutations become children.
    // When we only have one permutation, we make a leaf node and set the permutation of that leaf node up the top as the base case.
    for (int i = 0; i < LLC_SLICES; ++i)
    {
        if (partition_sizes[i] > 0)
        {
            for (int k = 0; k < num_best_elements; ++k)
            {
                expected_slices[i][k] = partitions[i][partition_sizes[i] - 1][best_elements[k]];
            }

            int *new_remaining_elements = (int *)malloc((num_remaining_elements) * sizeof(int));
            int *new_consumed_elements = calloc(((CL_PER_PAGE - num_remaining_elements) + num_best_elements), sizeof(int));
            int ydx = 0;
            for (ydx = 0; ydx < (CL_PER_PAGE - num_remaining_elements); ydx++)
            {
                new_consumed_elements[ydx] = consumed_elements[ydx];
            }
            for (int i = 0; i < num_best_elements; i++, ydx++)
            {
                new_consumed_elements[ydx] = best_elements[i];
            }

            if (new_remaining_elements == NULL)
            {
                fprintf(stderr, "DecisionTreeError: Failed to allocate memory for new remaining_elements, N_BEST_ELEMENTS too large and consuming entropy information (%d)\n", N_BEST_ELEMENTS);
                exit(1);
            }
            int idx = 0;
            for (int j = 0; j < num_remaining_elements; ++j)
            {
                int is_best = 0;
                for (int k = 0; k < num_best_elements; ++k)
                {
                    if (remaining_elements[j] == best_elements[k])
                    {
                        is_best = 1;
                        break;
                    }
                }
                if (!is_best)
                {
                    new_remaining_elements[idx++] = remaining_elements[j];
                }
            }
            node->children[i] = build_decision_tree_recurse(partitions[i], partition_sizes[i], new_consumed_elements, new_remaining_elements, num_remaining_elements - num_best_elements, node, num_best_elements);
            free(new_remaining_elements);
            free(new_consumed_elements);
        }
        else
        {
            node->children[i] = NULL;
        }

        if (partitions[i] != NULL)
        {
            free(partitions[i]);
        }
    }
    return node;
}

// Function to build the decision tree
DecisionTreeNode_t *build_decision_tree_root(int **permutations, int num_permutations, int num_best_elements)
{
    int *remaining_elements = (int *)calloc(CL_PER_PAGE, sizeof(int));
    for (int i = 0; i < CL_PER_PAGE; ++i)
    {
        remaining_elements[i] = i;
    }
    DecisionTreeNode_t *root = build_decision_tree_recurse(permutations, num_permutations, NULL, remaining_elements, CL_PER_PAGE, NULL, num_best_elements);
    free(remaining_elements);
    return root;
}

// Function to print the decision tree (for debugging purposes)
void print_tree_recurse(DecisionTreeNode_t *node, int depth)
{
    if (node->permutation != NULL)
    {
        printf("%*sLeaf: ", depth * 2, "");
        for (int i = 0; i < CL_PER_PAGE; ++i)
        {
            printf("%d ", node->permutation[i]);
        }
        printf("\n");
    }
    else
    {
        printf("%*sNode: Measure element(s): ", depth * 2, "");
        for (int i = 0; i < node->num_elements; ++i)
        {
            printf("%d ", node->elements[i]);
        }
        putchar('\n');
        for (int i = 0; i < LLC_SLICES; ++i)
        {
            if (node->children[i] != NULL)
            {
                printf("%*sExpected Value(s): ", depth * 2, "");
                for (int j = 0; j < node->num_elements; ++j)
                {
                    printf("%d ", node->expected_slices[i][j]);
                }
                putchar('\n');
                print_tree_recurse(node->children[i], depth + 2);
            }
        }
    }
}

void print_tree(DecisionTreeNode_t *node)
{
    print_tree_recurse(node, 0);
}

// Function to free the memory allocated for the decision tree
void free_decision_tree(DecisionTreeNode_t *node)
{
    if (node == NULL)
        return;

    for (int i = 0; i < LLC_SLICES; ++i)
    {
        if (node->children)
        {
            if (node->children[i] != NULL)
            {
                free_decision_tree(node->children[i]);
            }
        }
    }

    free(node->elements);

    if (node->expected_slices != NULL)
    {
        for (int i = 0; i < LLC_SLICES; ++i)
        {
            if (node->expected_slices[i])
                free(node->expected_slices[i]);
        }
        free(node->expected_slices);
    }
    if (node->children)
        free(node->children);
    if (node->unique_pages != NULL)
        free(node->unique_pages);
    free(node);
}

void find_min_max_depth(DecisionTreeNode_t *node, int depth, int *min_depth, int *max_depth)
{
    if (node->permutation != NULL)
    {
        if (depth < *min_depth)
        {
            *min_depth = depth;
        }
        if (depth > *max_depth)
        {
            *max_depth = depth;
        }
    }
    else
    {
        for (int i = 0; i < LLC_SLICES; ++i)
        {
            if (node->children[i] != NULL)
            {
                find_min_max_depth(node->children[i], depth + 1, min_depth, max_depth);
            }
        }
    }
}

DecisionTreeNode_t *build_decision_tree(int n_best_elements)
{
    if (decision_tree != NULL)
        return decision_tree;

#if VERBOSITY >= 1
    printf("Building Decision Tree\n");
    printf("Generating pages to determine unique permutations...\n");
#endif

    int num_pages = (1 * GB) / PAGE_SIZE;
    int **pages = (int **)malloc(num_pages * sizeof(int *));
    for (int i = 0; i < num_pages; ++i)
    {
        if (i % 128 == 0)
        {
#if VERBOSITY >= 1
            printf("%d / %d\r", i, num_pages);
#endif
        }
        pages[i] = (int *)calloc(CL_PER_PAGE, sizeof(int));
        get_page_permutation(i, pages[i]);
    }

#if VERBOSITY >= 1
    printf("\nDone\nFinding unique pages...\n");
#endif

    int max_permutations = 0;
    int unique_pages_count = 0;
    int **unique_pages = (int **)calloc(num_pages, sizeof(int *));
    int *occurrences = (int *)calloc(num_pages, sizeof(int)); // To count occurrences

    for (int i = 0; i < num_pages; ++i)
    {
        int is_unique = 1;
        for (int j = 0; j < unique_pages_count; ++j)
        {
            if (memcmp(pages[i], unique_pages[j], CL_PER_PAGE * sizeof(int)) == 0)
            {
                is_unique = 0;
                occurrences[j]++; // Increment occurrence count
                break;
            }
        }
        if (is_unique)
        {
            unique_pages[unique_pages_count] = (int *)calloc(CL_PER_PAGE, sizeof(int));
            memcpy(unique_pages[unique_pages_count], pages[i], CL_PER_PAGE * sizeof(int));
            occurrences[unique_pages_count] = 1; // Initialize count to 1
            unique_pages_count++;
        }
    }

    max_permutations = unique_pages_count;

#if VERBOSITY >= 1
    printf("Number of Unique Page Permutations: %d\n", max_permutations);
#endif

#if VERBOSITY >= 2
    // Print first 8 elements of each unique permutation and their occurrences
    for (int i = 0; i < unique_pages_count; ++i)
    {
        printf("Unique permutation %d: ", i + 1);
        for (int k = 0; k < 8 && k < CL_PER_PAGE; ++k)
        { // Print first 8 elements
            printf("%d ", unique_pages[i][k]);
        }
        printf(" | Occurs: %d\n", occurrences[i]);
    }
#endif

    decision_tree = build_decision_tree_root(unique_pages, max_permutations, n_best_elements);
    decision_tree->unique_pages = unique_pages;
    decision_tree->unique_pages_count = unique_pages_count;

    int slice_counts[LLC_SLICES] = {0};
    decision_tree->slice_likelihoods = calloc(LLC_SLICES, sizeof(double));

    for (int i = 0; i < num_pages; ++i)
    {
        // Count slices for the current page
        for (int cl = 0; cl < CL_PER_PAGE; ++cl)
        {
            int slice = pages[i][cl];
            if (slice >= 0 && slice < LLC_SLICES)
            {
                slice_counts[slice]++;
            }
        }
    }

    // Calculate and print the likelihood for each slice
    int total_counts = num_pages * CL_PER_PAGE;
    for (int s = 0; s < LLC_SLICES; ++s)
    {
        double likelihood = (double)slice_counts[s] / total_counts;
        decision_tree->slice_likelihoods[s] = likelihood;
#if VERBOSITY >= 1
        printf("Slice %2d Likelihood: %.4f\n", s, likelihood);
#endif
    }

    int min_depth = INT_MAX;
    int max_depth = INT_MIN;
    find_min_max_depth(decision_tree, 0, &min_depth, &max_depth);

    for (int i = 0; i < num_pages; ++i)
    {
        free(pages[i]);
    }
    free(pages);
    free(occurrences); // Free occurrences array

#if VERBOSITY >= 1
    printf("Decision Tree complete\n");
    printf("\tMinimum leaf depth: %d\n", min_depth);
    printf("\tMaximum leaf depth: %d\n", max_depth);
#endif

    return decision_tree;
}