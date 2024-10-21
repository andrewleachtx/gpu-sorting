#!/bin/bash

# Grace properties
cores_per_node=48
mem_per_node=384

sortname="bitonic"
output_dir="./results/"
timestamp_hash=$(date +%s)

# Arrays of parameters - can comment/uncomment as needed
array_sizes=(16777216)
num_processes=(1024)
input_types=("1_perc_perturbed")

# array_sizes=(65536 262144 1048576 4194304 16777216 67108864 268435456)
# num_processes=(2 4 8 16 32 64 128 256 512 1024)
# input_types=("Sorted" "Random" "ReverseSorted" "1_perc_perturbed")

# Strings to replace in the job script
replace_sortname="SORTNAME"
replace_outputdir="OUTPUT_DIR"
replace_array="ARRAY-SZ"
replace_proc="NUM-PROCESSES"
replace_nodes="NUM_NODES"
replace_tasks="NUM_TASKS"
replace_mem="MEM_NEEDED"
replace_hash="HASH"

for array_sz in "${array_sizes[@]}"; do
    for input_type in "${input_types[@]}"; do
        # Add per input type, add if not there
        output_dir="./results/$input_type"
        mkdir -p $output_dir

        for p in "${num_processes[@]}"; do
            # Determine number of nodes needed
            num_nodes=$(($p / $cores_per_node))
            if (($p % $cores_per_node > 0)); then
                num_nodes=$(($num_nodes + 1))
            fi

            # Determine tasks per node needed
            num_tasks=$(($p / $num_nodes))
            if (($p % $num_nodes > 0)); then
                num_tasks=$(($num_tasks + 1))
            fi

            mem_needed=$(($num_tasks * $mem_per_node / $cores_per_node))
            mem_needed=$(( mem_needed > 32 ? 360 : mem_needed ))
            echo Requesting ${mem_needed}Gb

            # Create altered job file
            sed "s|$replace_sortname|$sortname|g; \
                 s|$replace_outputdir|$output_dir|g; \
                 s|$replace_hash|$timestamp_hash|g; \
                 s|$replace_array|$array_sz|g; \
                 s|$replace_proc|$p|g; \
                 s|$replace_nodes|$num_nodes|g; \
                 s|$replace_tasks|$num_tasks|g; \
                 s|$replace_mem|$mem_needed|g" \
                 bitonic.grace_job > temp.grace_job

            # Dispatch job with arguments
            sbatch temp.grace_job $p $array_sz $input_type

            # Remove altered job file
            rm temp.grace_job
        done
    done
done
