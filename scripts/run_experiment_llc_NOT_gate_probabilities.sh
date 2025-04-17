#!/bin/bash

replace_config () {
	cp $1 ../include/config.h
}

#CPU L1 Info
L1D_SIZE=$(taskset -c 0 getconf -a | grep L1_DCACHE_SIZE | awk '{print $2}')
L1D_ASSOCIATIVITY=$(taskset -c 0 getconf -a | grep LEVEL1_DCACHE_ASSOC | awk '{print $2}')
L1_CACHELINE=$(taskset -c 0 getconf -a | grep LEVEL1_DCACHE_LINESIZE | awk '{print $2}')

#CPU L2 Info
L2_SIZE=$(taskset -c 0 getconf -a | grep L2_CACHE_SIZE | awk '{print $2}')
L2_ASSOCIATIVITY=$(taskset -c 0 getconf -a | grep LEVEL2_CACHE_ASSOC | awk '{print $2}')
L2_CACHELINE=$(taskset -c 0 getconf -a | grep LEVEL2_CACHE_LINESIZE | awk '{print $2}')

#CPU L3 Info
LLC_SIZE=$(taskset -c 0 getconf -a | grep LEVEL3_CACHE_SIZE | awk '{print $2}')
LLC_ASSOCIATIVITY=$(taskset -c 0 getconf -a | grep LEVEL3_CACHE_ASSOC | awk '{print $2}')
LLC_CACHELINE=$(taskset -c 0 getconf -a | grep LEVEL3_CACHE_LINESIZE | awk '{print $2}')

echo $L1D_ASSOCIATIVITY
echo $L2_ASSOCIATIVITY
echo $LLC_ASSOCIATIVITY

replace_config $1

make clean
make -j OPS="-DL1D_SIZE=$L1D_SIZE -DL1D_ASSOCIATIVITY=$L1D_ASSOCIATIVITY -DL1_CACHELINE=$L1_CACHELINE -DL2_SIZE=$L2_SIZE -DL2_ASSOCIATIVITY=$L2_ASSOCIATIVITY -DL2_CACHELINE=$L2_CACHELINE -DLLC_SIZE=$LLC_SIZE -DLLC_ASSOCIATIVITY=$LLC_ASSOCIATIVITY -DLLC_CACHELINE=$LLC_CACHELINE"

if [[ $? -gt 0 ]]; then
	exit 1
fi

THREAD_SIBLINGS=$(cat /sys/devices/system/cpu/cpu0/topology/thread_siblings_list)
IFS=',-' read -r -a THREAD_SIBLINGS_ARRAY <<< "$THREAD_SIBLINGS"
NUM_CPUS=$(nproc)
CPU_LIST=""
echo $THREAD_SIBLINGS_ARRAY
for (( i=0; i<$NUM_CPUS; i++ )); do
    # Skip the CPUs in the thread siblings list
    if [[ ! " ${THREAD_SIBLINGS_ARRAY[@]} " =~ " ${i} " ]]; then
        CPU_LIST+="$i,"
    fi
done
CPU_LIST=${CPU_LIST%,}
NUM_CPUS_TO_USE=$(($NUM_CPUS - ${#THREAD_SIBLINGS_ARRAY[@]}))
echo $NUM_CPUS_TO_USE
echo $CPU_LIST

echo 0 | sudo tee /proc/sys/vm/nr_hugepages
FOLDER_PATH="../experiments/llc_NOT_gate_probabilities/current"
if [ -d "$FOLDER_PATH" ]; then
	CURRENT_DATE=$(date +"%Y%m%d_%H%M%S")

    # Construct new folder name with the date
    NEW_NAME="${FOLDER_PATH}/../${CURRENT_DATE}"
    
    # Rename the existing folder
    mv "$FOLDER_PATH" "$NEW_NAME"
fi
mkdir -p "${FOLDER_PATH}"

for (( i = 0; i < 1; i++ )); do
	echo $i
	while true; do
      sudo ASAN_OPTIONS="detect_leaks=0" taskset -c ${THREAD_SIBLINGS_ARRAY[1]} ../build/llc_NOT_gate_probabilities >> $FOLDER_PATH/data.txt
	    RES=$?
	    if [[ $RES -eq 0 ]]; then
	        break
	    fi
	    if [[ $RES -eq 130 ]]; then
	        exit 1
	    fi
	    echo "Command failed with exit status $RES, retrying..."
	done
done

echo 0 | sudo tee /proc/sys/vm/nr_hugepages
