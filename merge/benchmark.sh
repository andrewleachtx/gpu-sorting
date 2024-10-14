#!/bin/bash

# Grace properties
cores_per_node=48
mem_per_node=384

sortname="merge"
output_dir="./results/"
timestamp_hash=$(date +%s)

# In merge.grace_job these are set to this for sed to find
replace_sortname="SORTNAME"
replace_outputdir="OUTPUT_DIR"
replace_array="ARRAY-SZ"
replace_proc="NUM-PROCESSES"
replace_nodes="NUM_NODES"
replace_tasks="NUM_TASKS"
replace_mem="MEM_NEEDED"
replace_hash="HASH"

# Uncomment these ones to test
array_sizes=(65536)
num_processes=(128)
#array_sizes=(4194304 16777216 67108864)
#num_processes=(2 4 8 16 32 64 128 256 512)

for array_sz in "${array_sizes[@]}"; do
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

        # Determine memory needed per node
        mem_needed=$(($num_tasks * $mem_per_node / $cores_per_node))

        # Create altered job file - "s|<string to look for>|<what to replace it with>|g"
        sed "s|$replace_sortname|$sortname|g; \
             s|$replace_outputdir|$output_dir|g; \
             s|$replace_hash|$timestamp_hash|g; \
             s|$replace_array|$array_sz|g; \
             s|$replace_proc|$p|g; s|$replace_nodes|$num_nodes|g; \
             s|$replace_tasks|$num_tasks|g; \
             s|$replace_mem|$mem_needed|g" \
             merge.grace_job > temp.grace_job
        
        # Dispatch job
        sbatch temp.grace_job $p $array_sz
        
        # Remove altered job file
        rm temp.grace_job
    done
done
