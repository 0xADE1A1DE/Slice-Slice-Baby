#ifndef __EVSP_H__
#define __EVSP_H__

#include "../config.h"
#include "../util/util.h"
#include "slice_partitioning/decision_tree.h"

typedef enum
{
    EVSP_FLAG_SLICES_GROUND_TRUTH = (1 << 0),
    EVSP_FLAG_CLOSEST_MATCH_PROPAGATION = (1 << 1),
    EVSP_FLAG_BAYESIAN_PROPAGATION = (1 << 2),
    EVSP_FLAG_DECISION_TREE_PROPAGATION = (1 << 3),
} evsp_flags;

struct evsp_config_s
{
    int32_t num_pages;
    int32_t num_addresses;

    int num_slices;
    double *centroids;

    evsp_flags flags;

    uint8_t *cslices;
    uint8_t *slices;

    void *victim;
    uint8_t victim_slice;

    void *mem;
    size_t mem_len;

    DecisionTreeNode_t *decision_tree;
};

typedef struct evsp_config_s *evsp_config_t;

evsp_config_t evsp_configure(void *mem, size_t num_pages, int num_slices, evsp_flags flags);
void evsp_run(evsp_config_t evsp_config, void *mem);

int evsp_get_address_slice_raw(evsp_config_t evsp_config, void *address);
int evsp_get_address_slice_bayesian_inference(evsp_config_t evsp_config, void *address, int update_cslices);
int evsp_get_address_slice_decision_tree(evsp_config_t evsp_config, void *address, int update_cslices);
int evsp_get_address_slice_closest_match(evsp_config_t evsp_config, void *address, int update_cslices);

void evsp_verify_csv(evsp_config_t evsp_config, FILE *fp);
void evsp_verify(evsp_config_t evsp_config);
void evsp_release(evsp_config_t evsp_config);

#endif //__EVSP_H__
