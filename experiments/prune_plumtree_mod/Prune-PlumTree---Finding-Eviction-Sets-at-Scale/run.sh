#!/bin/bash

# Function to clean up before exiting
cleanup() {
    echo "Interrupted. Cleaning up..."
    rm -f data.txt
    exit 1
}

# Trap SIGINT (Ctrl+C) to call the cleanup function
trap cleanup SIGINT

if [[ $# -ne 1 ]]; then
    echo "Usage: ./run.sh [NUMBER OF PAGE OFFSETS TO BUILD EVSETS FOR]"
    exit 1
else
    echo "Running run.sh"
fi

# Read thread siblings list
THREAD_SIBLINGS=$(cat /sys/devices/system/cpu/cpu0/topology/thread_siblings_list)
IFS=',-' read -r -a THREAD_SIBLINGS_ARRAY <<< "$THREAD_SIBLINGS"
NUM_CPUS=$(nproc)
CPU_LIST=""
echo "${THREAD_SIBLINGS_ARRAY[@]}"

# Build the project
make clean
make -j
if [[ $? -gt 0 ]]; then
    exit 1
fi
rm -f data.txt

# Main loop
for (( i = 0; i < 100; i++ )); do #100
    for (( p = 0; p < $1; p++ )); do #64
        echo "$i $p"
        while true; do
            sleep 1
            sudo ASAN_OPTIONS="detect_leaks=0" taskset -c "${THREAD_SIBLINGS_ARRAY[1]}" ./main "$p" >> data.txt
            RES=$?
            if [[ $RES -eq 0 ]]; then
                break
            fi
            echo "Command failed with exit status $RES, retrying..."
        done
    done
done
