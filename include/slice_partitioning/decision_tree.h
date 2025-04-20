// This method is equivalent to the ID3 algorithm by J. R. Quinlan (https://en.wikipedia.org/wiki/ID3_algorithm).

#ifndef __DECISION_TREE__
#define __DECISION_TREE__

#include "slicing.h"

#ifndef MIN_DISTANCE
#define MIN_DISTANCE 1
#endif

#ifndef N_BEST_ELEMENTS
#define N_BEST_ELEMENTS 1
#endif

#define CL_PER_PAGE ((int)(PAGE_SIZE / CACHELINE)) // Calculate cache lines per page

// Define the structure for a node in the decision tree
typedef struct DecisionTreeNode_s
{
    int *elements;                        // Array of best elements to measure stored in the node
    int num_elements;                     // Number of best elements
    int **expected_slices;                // Double Array of expected slices for each element. Its size will be num_elements * num_children
    struct DecisionTreeNode_s *parent;    // Pointer to the parent node
    struct DecisionTreeNode_s **children; // Array of pointers to the child nodes
    int *permutation;                     // Pointer to the permutation array. Will be NULL if not a leaf node.
    int **unique_pages;
    int unique_pages_count;
    double *slice_likelihoods;
} DecisionTreeNode_t;

DecisionTreeNode_t *get_decision_tree();
DecisionTreeNode_t *build_decision_tree(int n_best_elements);
void print_tree(DecisionTreeNode_t *node);
void free_decision_tree(DecisionTreeNode_t *node);

#endif //__DECISION_TREE__