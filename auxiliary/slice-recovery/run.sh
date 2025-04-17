#!/bin/bash

if [[ $1 = "--save" ]]; then
 	MODEL_NAME=$(lscpu | grep "Intel" | awk -F "Intel" '{print $2}' | awk -F " " '{print $3}')
 	MODEL_NAME=$(printf "%s" $MODEL_NAME)
 	FAMILY=$(lscpu | grep "CPU family" | awk -F "CPU family" '{print $2}' | awk -F " " '{print $2}')
 	STEPPING=$(lscpu | grep "Stepping" | awk -F "Stepping" '{print $2}' | awk -F " " '{print $2}')
 	MODEL=$(echo $(lscpu | grep "Model" | awk -F "Model" '{print $2}' | awk -F " " '{print $2}') | awk -F " " '{print $2}')

	OUTPUT=$(sudo chrt -r 1 sudo taskset -c 0 ./get_slice_mapping)

 	SLICES=$(echo $OUTPUT | awk -F " " '{print $1}')
 	ADDR_BITS=$(echo $OUTPUT | awk -F " " '{print $2}')
 	OF=$(printf "./output/%s_%s_%s_%s_%s_%s_slice_function.txt\n" $MODEL_NAME $FAMILY $MODEL $STEPPING $SLICES $ADDR_BITS)
 	echo "$OUTPUT" > $OF
else
	if [[ $1 == "--sim" ]]; then
		sudo chrt -r 1 sudo taskset -c 0 ./get_slice_mapping --sim $2
	else
		sudo chrt -r 1 sudo taskset -c 0 ./get_slice_mapping
	fi
fi
