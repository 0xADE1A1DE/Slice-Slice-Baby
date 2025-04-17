#ifndef __COMPARATOR_GATE__
#define __COMPARATOR_GATE__

#include "../config.h"
#include "../evsets/evsets_defs.h"
#include "../util/util.h"

#define CALIBRATION_SAMPLES (1000 * LLC_SLICES)

typedef struct calibration_data_s
{
    int *data;
    int len;
} calibration_data_t;

size_t init_measuring_sticks(void *mem, size_t len);
void init_measuring_sticks_cheat();
elem_t **get_measuring_sticks();

calibration_data_t *comparator_gate_get_calibration_data(void *mem, size_t len);
calibration_data_t *comparator_gate_get_calibration_data_cheat(void *mem, size_t len);
void comparator_gate(void *input, int *output);

#endif //__COMPARATOR_GATE__