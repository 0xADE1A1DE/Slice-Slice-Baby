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

# Get the thread siblings for the first CPU
THREAD_SIBLINGS=$(cat /sys/devices/system/cpu/cpu0/topology/thread_siblings_list)
IFS=',-' read -r -a THREAD_SIBLINGS_ARRAY <<< "$THREAD_SIBLINGS"
NUM_CPUS=$(grep -c ^processor /proc/cpuinfo)
echo "Thread Siblings Array: ${THREAD_SIBLINGS_ARRAY[@]}"
PHYSICAL_CORES=()
for (( i=0; i<$NUM_CPUS; i++ )); do
    if [[ ! " ${THREAD_SIBLINGS_ARRAY[@]} " =~ " ${i} " ]]; then
        PHYSICAL_CORES+=("$i")
    fi
done
echo "Number of Physical Cores to Use: ${#PHYSICAL_CORES[@]}"
echo "Physical Cores List: ${PHYSICAL_CORES[@]}"

# Initialize a new array for the result
CORES=()

# Extract the first and second elements
first=${THREAD_SIBLINGS_ARRAY[0]}
second=${THREAD_SIBLINGS_ARRAY[1]}

# Loop until the second element is less than NUM_CPUS - 1
while [ "$second" -lt $((NUM_CPUS)) ]; do
    # Append the first element to the result array
    CORES+=("$first")

    # Increment both values
    first=$((first + 1))
    second=$((second + 1))
done

FNAME=$(basename "$1" .h)

if [[ "$FNAME" == "config_13700h" ]]; then
    CORES=(0 2 4 6 8 10)
fi

if [[ "$FNAME" == "config_12900k" || "$FNAME" == "config_14900k" ]]; then
    CORES=(0 2 4 6 8 10 12 14)
fi

# Print the resulting array
echo "${CORES[@]}"

# #Directory for experiments
FOLDER_PATH="../experiments/per_core_llc_bayesian_inference_predictions/current"

if [[ "$FNAME" != "config_13700h" && "$FNAME" != "config_12900k" && "$FNAME" != "config_14900k" ]]; then
    if [ -d "$FOLDER_PATH" ]; then
    	CURRENT_DATE=$(date +"%Y%m%d_%H%M%S")

        # Construct new folder name with the date
        NEW_NAME="${FOLDER_PATH}/../${CURRENT_DATE}"
        
        # Rename the existing folder
        mv "$FOLDER_PATH" "$NEW_NAME"
    fi
fi
mkdir -p "${FOLDER_PATH}"

sudo rm -f $FOLDER_PATH/data_${FNAME}_*

echo 0 | sudo tee /proc/sys/vm/nr_hugepages
for (( i = 0; i < 100; i++ )); do
    for CORE in "${CORES[@]}"; do
        echo "Iteration $i Running on core: $CORE"
        while true; do
            sudo ASAN_OPTIONS="detect_leaks=0" taskset -c "$CORE" ../build/slice_partition_bayesian_inference >> "$FOLDER_PATH/data_${FNAME}_${CORE}.txt"
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
done
echo 0 | sudo tee /proc/sys/vm/nr_hugepages