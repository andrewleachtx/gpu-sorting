#!/bin/bash

sortname="radix"
input_type_num=0

# Arrays of parameters
cores=(1 1 1 1 1 2 4 8 16 32)
array_sizes=(65536 262144 1048576 4194304 16777216 67108864 268435456)
num_processes=(2 4 8 16 32 64 128 256 512 1024)
input_types=("Sorted" "Random" "ReverseSorted" "1_perc_perturbed")

# Strings to replace in the job script
replace_input_type_name="INPUT-TYPE-NAME"
replace_input_type_num="INPUT-TYPE-NUM"
replace_proc="NUM-PROCESSES"
replace_array="ARRAY-SZ"
replace_nodes="NUM_NODES"
replace_tasks="NUM_TASKS"
replace_outputdir="OUTPUT_DIR"

input_type_name=${input_types[input_type_num]}
length=${#cores[@]}
for array_sz in "${array_sizes[@]}"; do
    # Output to folder based on input type
    output_dir="./results/$input_type_name"
    mkdir -p $output_dir

    # Format .grace_job and run
    for ((i = 0; i < length; ++i)); do
        p=${num_processes[i]}
        curr_core=${cores[i]}

        # Determine number of nodes needed
        nodes_per_core=$((p / curr_core))

        # Create altered job file
        sed "s|$replace_input_type_name|$input_type_name|g; \
             s|$replace_input_type_num|$input_type_num|g; \
             s|$replace_proc|$p|g; \
             s|$replace_array|$array_sz|g; \
             s|$replace_outputdir|$output_dir|g; \
             s|$replace_nodes|$curr_core|g; \
             s|$replace_tasks|$nodes_per_core|g;" \
             radix.grace_job > temp.grace_job

        # Dispatch job with arguments
        sbatch temp.grace_job $p $array_sz $input

        # Remove altered job file
        rm temp.grace_job
    done
done
