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
- Sample Sort: Brian
- Merge Sort: Anil
- Radix Sort: Gage

### 2b. Pseudocode for each parallel algorithm
- Bitonic Sort:
- Sample Sort:
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
- Merge Sort:

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

- Radix Sort:

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

### 3. Communication
Discord will be used as the primary means of meeting and communicating with everyone.
