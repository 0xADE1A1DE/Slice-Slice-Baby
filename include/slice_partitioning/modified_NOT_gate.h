#ifndef __MODIFIED_NOT_GATE_H__
#define __MODIFIED_NOT_GATE_H__

#include "../config.h"
#include "../util/util.h"

int modified_NOT_gate(void *input, void *output);
int modified_NOT_gate_raw(void *input, void *output, int chain_len);

#endif //__MODIFIED_NOT_GATE_H__