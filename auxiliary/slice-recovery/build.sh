#!/bin/bash

#CPU L1 Info
L1D=$(getconf -a | grep L1_DCACHE_SIZE | awk '{print $2}')
L1_ASSOCIATIVITY=$(getconf -a | grep LEVEL1_DCACHE_ASSOC | awk '{print $2}')
L1_CACHELINE=$(getconf -a | grep LEVEL1_DCACHE_LINESIZE | awk '{print $2}')

#CPU L2 Info
L2=$(getconf -a | grep L2_CACHE_SIZE | awk '{print $2}')
L2_ASSOCIATIVITY=$(getconf -a | grep LEVEL2_CACHE_ASSOC | awk '{print $2}')
L2_CACHELINE=$(getconf -a | grep LEVEL2_CACHE_LINESIZE | awk '{print $2}')

#CPU L3 Info
L3_ASSOCIATIVITY=$(getconf -a | grep LEVEL3_CACHE_ASSOC | awk '{print $2}')
L3_CACHELINE=$(getconf -a | grep LEVEL3_CACHE_LINESIZE | awk '{print $2}')

sudo make clean
make get_num_slices
sudo make get_slice_mapping OPS="-DL1D=$L1D -DL1_ASSOCIATIVITY=$L1_ASSOCIATIVITY -DL1_CACHELINE=$L1_CACHELINE -DL2=$L2 -DL2_ASSOCIATIVITY=$L2_ASSOCIATIVITY -DL2_CACHELINE=$L2_CACHELINE -DL3_CACHELINE=$L3_CACHELINE"
sudo make example_hash_function_usage
