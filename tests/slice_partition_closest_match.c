#include <stdio.h>
#include <stdlib.h>

#include "../include/evsets/evsets.h"
#include "../include/slice_partitioning/evsp.h"

int main()
{
    size_t len = LLC_SIZE * 3;
    uint8_t *mem = (uint8_t *)initialise_memory(len, 0, PAGE_SIZE);
    evsp_flags flags = 0;
    flags |= EVSP_FLAG_CLOSEST_MATCH_PROPAGATION;
    evsp_config_t evsp_config = evsp_configure(mem, ((len) / PAGE_SIZE), LLC_SLICES, flags);

    evsp_run(evsp_config, mem);

    evsp_verify(evsp_config);

    // Release allocated resources
    evsp_release(evsp_config);
}
