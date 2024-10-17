# CSCE 435 Group project

## 0. Group number: 
Group 3

## 1. Group members:
1. Andrew Leach
2. Gage Mariano
3. Brian Nguyen
4. Anil Parthasarathi

## 2. Project topic
Parallel sorting algorithms

### 2a. Brief project description
- Bitonic Sort: Andrew
  - Bitonic sort is based on two primary ideas - bitonic sequences, and sorting networks. Any sequence of values can be defined as bitonic (increasing and decreasing or vice versa) at the smallest level (two elements) and from there merged to make larger bitonic sequences. As we iterate over the "stages" of our sorting network we have a corresponding set of "strides" which help calculate what "partner" processes are merged according to a specific direction. Eventually, we end up with a full sequence of purely increasing values; the first half of a bitonic sequence which is of course sorted. Notably, this code runs two nested $log(n)$ loops, which explains its $log^2(n)$ behavior, regardless of if the input array is sorted.
- Sample Sort: Brian
    - Sample sort is a parallel sorting algorithm that works by sampling values from the unsorted input array, building buckets from the sampled values, and distributing work for processors to place elements into their respective buckets as well as sorting the buckets. It is often called a generalization of quicksort as it uses the same pivot mechanism to sort but does so in a way that we can more efficiently parallelize.
- Merge Sort: Anil
    - Merge sort is a sorting algorithm that takes a recursive, divide and conquer, approach toward putting the list in order. It works by splitting the list in half repeatedly until it is down to singular elements, then repeatedly merging sorted lists together. The algorithm has been made parallel by dividing the list between each processor in equal chunks and calling standard merge sort within those chunks. Once all the processors have finished this, the sorted lists are merged in parallel, where a sequence of stages takes place. Corresponding to the processor count, in each stage half of the processors from the previous stage will go idle after merging their contents into a different processor. Eventually only one processor containing the complete sorted list will remain.
- Radix Sort: Gage
    - A non-comparison based sorting algorithm, Radix sort works digit by digit to repeatedly reorder the array in a stable manner until all digits have been considered and the array is sorted. Each process handles a segment of the array, creating a histogram of the local digits, before consolidating the gathered data to generate a global histogram. Elements are then reordered accordingly in a distributed fashion, setting the stage for the process to repeat with the next digit. Procedurally similar to the sequential implementation, parallelization of the counting aspect of Radix sort allows for significant performance benefits at the cost of requiring extensive communication to physically sort elements and maintain an even distribution between processes.

### 2b. Pseudocode for each parallel algorithm
#### Bitonic Sort:

```
"""
    Assume we have an (initially unsorted) array A[] of size n, which is a power of two.

    At the lowest level, bitonic sort should compare two elements, and put them in the "correct"
    order specified by the direction bit "dir" for 0 = decreasing (2->1), and 1 = increasing (3->4).

    Dividing this algorithm into "stages" or levels, we can say there are log_2(n) levels or stages, and
    our goal is to perform that comparison between two processes or positions, I found that it is commonly
    described as a "partner" at a distance "stride" away. 
"""

#define DECREASING 0
#define INCREASING 1

"""
    If this goal is decreasing, and it is increasing, swap.
    If the goal is increasing, and it is decreasing, swap.
"""
def needsSwap(v1, v2, dir):
    if (dir == DECREASING && v1 < v2):
        return true
    if (dir == INCREASING && v1 > v2):
        return true

    return false

def main():
    # Start conditions by randomizing A, getting length, and assume we want increasing for now (this can be easily adjusted later on, just affects the later dir calc)
    A = [...]
    n = A.size()

    # Arbitrary MPI functions, I don't recall their names.
    rank = MPI_Rank(...)
    n_procs = MPI_Size(...)

    assert(n is a power of two)

    # Divide our overall array into (n / n_procs) chunks, **I ASSUME N = N_PROCS FOR THIS CODE**
    n_cur = n / n_procs

    # For each stage of the way we have a new partner and direction calculable in O(1)
    n_stages = log_2{n_procs}

    # The calculations done here are primarily using the "07_CSCE435_algorithms.pdf" bitonic network visuals
    for stage in range(n_stages):
        for stride in range(stage + 1):
            # For stage 0 1 <<2>> we need stride_sz = 4, 2, 1 where stride goes (0, 1, 2)
            stride_sz = 1 << (stage - stride)
            partner_rank = rank ^ stride_sz

            """
                Direction is a bit more tricky, we need to know which "half" we reside in at
                the current stage (not step). All we know is that when we are at stage 0, we have
                two halves (10, 20 and 5, 9) and our goal is to get to a inc/dec or dec/inc pair.
                Also, this assumes we start at increasing.

                Our question is, are we the [10, 20] or [5, 9].
                
                stage = 0 => pair_sz = (2 ^ (stage + 1))
                [0, 1] 0000 // 2 = 0, 0001 // 2 == 0
                [2, 3] 0010 // 2 = 1, 0011 // 2 == 1

                so I (think):

                half = rank // (2 ^ (stage + 1))

                and arbitrarily half = 0 => INC, half = 1 => DEC
            """

            half = rank // (2 ^ (stage + 1))
            dir = INCREASING
            if half == 1:
                dir = DECREASING

            # Send the lower rank partner your value, they will decide whether it need be sorted based on dir
            if rank < partner_rank:
                val_buf = -1
                MPI_Recv(&val_buf, ...)

                if needsSwap(A[rank], val_buf):
                    MPI_Send(A[rank], ...)
                    A[rank] = val_buf
                else:
                    MPI_Send(val_buf)
            else:
                MPI_Send(A[rank], ...)

                val_buf = A[rank]
                MPI_Recv(&val_buf)

                # Regardless, we will get the value we need to update to (no swap means we got same back)
                A[rank] = val_buf

    # After all stages and strides done, we are sorted
    print(A)
```

#### Sample Sort:

```
Quicksort(data):
- recursive implementation
- each iteration, choose a pivot point
- split data into two groups based on if given data is less than or greater than/equal to pivot
- recursively call quicksort on respective halfs of the data
- base case: if data size is less than or equal to 1, simply return data
- once recursive calls finish, combine data as follows: left + pivot + right
- and return data

Main():
- from command line args take in the number of processors, splitters, and samples
- from the master processor, evenly distribute data to each worker processor
- in the worker processor, receive the data from the master processor
- in the worker processor, sort the received data using quicksort
- in the worker processor, draw s samples from the sorted data
- in the worker processor, send selected samples to the master processor
- in the master processor, receive selected samples from all worker processors
- in the master processor, sort received samples using quicksort and choose m splitters
- in the master processor, send the respective bucket ranges to each worker processor
- in the worker processor, receive the bucket ranges and sort data into respective buckets
- once sorted by bucket, send the respective bucket's data to it's given worker processor
- once each worker processor receives the data for the respective bucket they manage,
use quicksort one last time to sort the bucket's data.
- each worker should then send the respective bucket's sorted data to the master processor
- and the master processor should receive and combine the values of the buckets to 
construct the fully sorted input.
```

#### Merge Sort:

```
global variable threadCount
global variable maxThreads

function merge (list1, list2):
- loop through the lists and create a merged list in sorted order
- - This will involve keeping an index for list1 and list2 and comparing the items at those indexes, adding the item less than or equal to the other to the new list
- - The list that has its item added will get its index incremented
- - This goes on until all items from both lists get added
- - O(n + m) time total
- Add leftover values from one of the lists if necessary
- Return sorted list

function parallelMergeSort(list):
- If list is only 1 entry return
- Find the middle index of the list
- Create two new lists each containing half of the original list
- if threadCount is less than maxthreads:
- - Create a thread for the first half of the list and call parallelMergeSort() within that thread
- - increment threadCount by 1 (make sure to secure a lock before modifying the global variable)
- - Call parallelMergeSort() on the second half of the list
- - Synch threads (wait until "master" has the results from both halves of the list)
- - Decrement threadCount to potentially allow a new thread to be made (make sure to secure a lock before modifying the global variable)
- Else:
- - call parallelMergeSort() on the first half of the list
- - call parallelMergeSort() on the second half of the list
- Call merge using the two halves of the lists which will return the sorted list
```

#### Radix Sort:

```
function radix_sort():
    // Partition Data
    Read from command line: input_size, input_type
    Get number of processes and rank
    Generate partition of data
    if input_type is perturbed:
        exchange elements with other processes

    // Calculation
    Find local_maximum (most digits) element
    Synchronize to global_maximum

    for digit in global_maximum: // From least to most significant
        
        // Counting Sort
        buckets = [0]*10
        for elem in partition:
            increment bucket indexed by current elem digit

        Collect information on counts across all processes (global count) and store in buckets

        Perform prefix-sum on buckets. 

        // Repartition
        Send elements to appropriate position and process according to buckets while maintaining stability  
        Receive elements and store in partition, moving current elements as necessary

    // Check
    Iterate through partition and check order

    if rank != highest:
        Send maximum value in partition to next rank process (unless highest rank)

    if rank != lowest:
        Receive maximum value from previous rank process (unless lowest rank)
        Check if local minimum is greater than or equal to received value

```

### 2c. Evaluation plan - what and how will you measure and compare
- Varying input sizes and varying numbers of processors (powers of 2)
    - Input Sizes: 2^16, 2^18, 2^20, 2^22, 2^24, 2^26, 2^28
    - Num Procs: 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024
- Input Types: Sorted, Random, Reverse sorted, 1% perturbed

### 2d. Communication
Discord will be used as the primary means of meeting and communicating with everyone.

### 3a. Caliper instrumentation
Please use the caliper build `/scratch/group/csce435-f24/Caliper/caliper/share/cmake/caliper` 
(same as lab2 build.sh) to collect caliper files for each experiment you run.

Your Caliper annotations should result in the following calltree
(use `Thicket.tree()` to see the calltree):
```
main
|_ data_init_X      # X = runtime OR io
|_ comm
|    |_ comm_small
|    |_ comm_large
|_ comp
|    |_ comp_small
|    |_ comp_large
|_ correctness_check
```

Required region annotations:
- `main` - top-level main function.
    - `data_init_X` - the function where input data is generated or read in from file. Use *data_init_runtime* if you are generating the data during the program, and *data_init_io* if you are reading the data from a file.
    - `correctness_check` - function for checking the correctness of the algorithm output (e.g., checking if the resulting data is sorted).
    - `comm` - All communication-related functions in your algorithm should be nested under the `comm` region.
      - Inside the `comm` region, you should create regions to indicate how much data you are communicating (i.e., `comm_small` if you are sending or broadcasting a few values, `comm_large` if you are sending all of your local values).
      - Notice that auxillary functions like MPI_init are not under here.
    - `comp` - All computation functions within your algorithm should be nested under the `comp` region.
      - Inside the `comp` region, you should create regions to indicate how much data you are computing on (i.e., `comp_small` if you are sorting a few values like the splitters, `comp_large` if you are sorting values in the array).
      - Notice that auxillary functions like data_init are not under here.
    - `MPI_X` - You will also see MPI regions in the calltree if using the appropriate MPI profiling configuration (see **Builds/**). Examples shown below.

All functions will be called from `main` and most will be grouped under either `comm` or `comp` regions, representing communication and computation, respectively. You should be timing as many significant functions in your code as possible. **Do not** time print statements or other insignificant operations that may skew the performance measurements.

### **Nesting Code Regions Example** - all computation code regions should be nested in the "comp" parent code region as following:
```
CALI_MARK_BEGIN("comp");
CALI_MARK_BEGIN("comp_small");
sort_pivots(pivot_arr);
CALI_MARK_END("comp_small");
CALI_MARK_END("comp");

# Other non-computation code
...

CALI_MARK_BEGIN("comp");
CALI_MARK_BEGIN("comp_large");
sort_values(arr);
CALI_MARK_END("comp_large");
CALI_MARK_END("comp");
```

### **Calltree Example**:
```
# MPI Mergesort
4.695 main
├─ 0.001 MPI_Comm_dup
├─ 0.000 MPI_Finalize
├─ 0.000 MPI_Finalized
├─ 0.000 MPI_Init
├─ 0.000 MPI_Initialized
├─ 2.599 comm
│  ├─ 2.572 MPI_Barrier
│  └─ 0.027 comm_large
│     ├─ 0.011 MPI_Gather
│     └─ 0.016 MPI_Scatter
├─ 0.910 comp
│  └─ 0.909 comp_large
├─ 0.201 data_init_runtime
└─ 0.440 correctness_check
```
#### Calltrees
- Bitonic Sort: Andrew
```
0.94869 main
├─ 0.00004 MPI_Init
├─ 0.00003 data_init_runtime
├─ 0.01050 comm
│  ├─ 0.00686 comm_large
│  │  ├─ 0.00583 MPI_Scatter
│  │  ├─ 0.00074 MPI_Sendrecv
│  │  └─ 0.00020 MPI_Gather
│  └─ 0.00348 MPI_Barrier
├─ 0.00023 comp
│  ├─ 0.00004 comp_small
│  └─ 0.00005 comp_large
├─ 0.00001 MPI_Finalize
├─ 0.00001 correctness_check
├─ 0.00001 MPI_Initialized
├─ 0.00001 MPI_Finalized
└─ 0.00106 MPI_Comm_dup
```

- Sample Sort: Brian
```
1.61089 main
├─ 0.00003 MPI_Init
├─ 0.00796 data_init_runtime
├─ 0.04400 comm
│  ├─ 0.04294 comm_large
│  │  ├─ 0.04174 MPI_Recv
│  │  └─ 0.00077 MPI_Send
│  └─ 0.00920 comm_small
│     └─ 0.00916 MPI_Send
├─ 0.00249 comp
│  ├─ 0.00002 comp_small
│  └─ 0.00277 comp_large
├─ 0.00008 correctness_check
├─ 0.00000 MPI_Finalize
├─ 0.00001 MPI_Initialized
├─ 0.00001 MPI_Finalized
└─ 0.02370 MPI_Comm_dup
```

- Merge Sort: Anil
```
26.42026 main
├─ 0.00004 MPI_Init
├─ 2.16805 main
│  ├─ 0.03342 data_init_runtime
│  ├─ 1.99352 comp
│  │  └─ 1.99346 comp_large
│  ├─ 0.10944 comm
│  │  └─ 0.10940 comm_large
│  │     ├─ 0.17937 MPI_Recv
│  │     └─ 0.01974 MPI_Send
│  └─ 1.42442 correctness_check
├─ 0.00001 MPI_Finalize
├─ 0.00001 MPI_Initialized
├─ 0.00001 MPI_Finalized
└─ 21.95693 MPI_Comm_dup
```

- Radix Sort: Gage
```
profile 	3586131742
launchdate 	1729134313
libraries 	[/scratch/group/csce435-f24/Caliper/caliper/li...
cmdline 	[./radix, 1048576, 0]
cluster 	c
algorithm 	radix
programming_model 	mpi
data_type 	int
size_of_data_type 	4
input_size 	1048576
input_type 	sorted
n_procs 	4
scalability 	weak
group_num 	3
implementation_source 	Handwritten
```

### 3b. Collect Metadata

Have the following code in your programs to collect metadata:
```
adiak::init(NULL);
adiak::launchdate();    // launch date of the job
adiak::libraries();     // Libraries used
adiak::cmdline();       // Command line used to launch the job
adiak::clustername();   // Name of the cluster
adiak::value("algorithm", algorithm); // The name of the algorithm you are using (e.g., "merge", "bitonic")
adiak::value("programming_model", programming_model); // e.g. "mpi"
adiak::value("data_type", data_type); // The datatype of input elements (e.g., double, int, float)
adiak::value("size_of_data_type", size_of_data_type); // sizeof(datatype) of input elements in bytes (e.g., 1, 2, 4)
adiak::value("input_size", input_size); // The number of elements in input dataset (1000)
adiak::value("input_type", input_type); // For sorting, this would be choices: ("Sorted", "ReverseSorted", "Random", "1_perc_perturbed")
adiak::value("num_procs", num_procs); // The number of processors (MPI ranks)
adiak::value("scalability", scalability); // The scalability of your algorithm. choices: ("strong", "weak")
adiak::value("group_num", group_number); // The number of your group (integer, e.g., 1, 10)
adiak::value("implementation_source", implementation_source); // Where you got the source code of your algorithm. choices: ("online", "ai", "handwritten").
```
#### Metadata
- Bitonic Sort: Andrew
```
profile	471284192
launchdate	1728608854
libraries	[/scratch/group/csce435-f24/Caliper/caliper/li...
cmdline	[./bitonic, 32]
cluster	c
algorithm	bitonic
programming_model	mpi
data_type	int
size_of_data_type	4
input_size	32
input_type	Random
num_procs	16
scalability	weak
group_num	3
implementation_source	handwritten
```

- Sample Sort: Brian
```
profile	1223936475
launchdate	1729092251
libraries	[/scratch/group/csce435-f24/Caliper/caliper/li...
cmdline	[./sample-sort, 1024, 8]
cluster	c
algorithm	sample
programming_model	mpi
data_type	int
size_of_data_type	4
input_size	1024
input_type	Random
num_procs	9
scalability	weak
group_num	3
implementation_source	handwritten
```

- Merge Sort: Anil
```
profile: 554544421
cali.caliper.version: 2.11.0
mpi.world.size: 256
spot.metrics: min#inclusive#sum#time.duration,max#inclusive#...
spot.timeseries.metrics:	
spot.format.version: 2	
spot.options: time.variance,profile.mpi,node.order,region.co...	
spot.channels: regionprofile	
cali.channel: spot	
spot:node.order: true	
spot:output: results/cali/256_268435456.cali		
spot:profile.mpi: true	
spot:region.count: true	
spot:time.exclusive: true	
spot:time.variance: true	
launchdate: 1728917829	
libraries: [/scratch/group/csce435-f24/Caliper/caliper/li...]		
cmdline: [./merge, 268435456]		
cluster: c	
algorithm: merge	
programming_model: mpi	
data_type: int	
size_of_data_type: 4	
input_size: 268435456	
input_type: Random	
num_procs: 256	
scalability: weak	
group_num: 3	
implementation_source: handwritten
```

- Radix Sort: Gage
```
| Profile                  | 3586131742                                        |
|--------------------------|---------------------------------------------------|
| Launch Date              | 1729134313                                        |
| Libraries                | [/scratch/group/csce435-f24/Caliper/caliper/li... |
| Command Line             | [./radix, 1048576, 0]                             |
| Cluster                  | c                                                 |
| Algorithm                | radix                                             |
| Programming Model        | mpi                                               |
| Data Type                | int                                               |
| Size of Data Type        | 4                                                 |
| Input Size               | 1048576                                           |
| Input Type               | sorted                                            |
| Number of Processes      | 4                                                 |
| Scalability              | weak                                              |
| Group Number             | 3                                                 |
| Implementation Source    | Handwritten                                       |
```

They will show up in the `Thicket.metadata` if the caliper file is read into Thicket.

### **See the `Builds/` directory to find the correct Caliper configurations to get the performance metrics.** They will show up in the `Thicket.dataframe` when the Caliper file is read into Thicket.
## 4. Performance evaluation

Include detailed analysis of computation performance, communication performance. 
Include figures and explanation of your analysis.

### 4a. Vary the following parameters
For input_size's:
- 2^16, 2^18, 2^20, 2^22, 2^24, 2^26, 2^28

For input_type's:
- Sorted, Random, Reverse sorted, 1%perturbed

MPI: num_procs:
- 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024

This should result in 4x7x10=280 Caliper files for your MPI experiments.

### 4b. Hints for performance analysis

To automate running a set of experiments, parameterize your program.

- input_type: "Sorted" could generate a sorted input to pass into your algorithms
- algorithm: You can have a switch statement that calls the different algorithms and sets the Adiak variables accordingly
- num_procs: How many MPI ranks you are using

When your program works with these parameters, you can write a shell script 
that will run a for loop over the parameters above (e.g., on 64 processors, 
perform runs that invoke algorithm2 for Sorted, ReverseSorted, and Random data).  

### 4c. You should measure the following performance metrics
- `Time`
    - Min time/rank
    - Max time/rank
    - Avg time/rank
    - Total time
    - Variance time/rank


## 5. Presentation
Plots for the presentation should be as follows:
- For each implementation:
    - For each of comp_large, comm, and main:
        - Strong scaling plots for each input_size with lines for input_type (7 plots - 4 lines each)
        - Strong scaling speedup plot for each input_type (4 plots)
        - Weak scaling plots for each input_type (4 plots)

Analyze these plots and choose a subset to present and explain in your presentation.

## 6. Final Report
Submit a zip named `TeamX.zip` where `X` is your team number. The zip should contain the following files:
- Algorithms: Directory of source code of your algorithms.
- Data: All `.cali` files used to generate the plots seperated by algorithm/implementation.
- Jupyter notebook: The Jupyter notebook(s) used to generate the plots for the report.
- Report.md
