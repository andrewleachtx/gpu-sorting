# CSCE 435 Group project

## 0. Group number: 
Group 3

## 1. Group members:
1. Andrew Leach
2. Gage Mariano
3. Brian Nguyen
4. Anil Parthasarathi

## 2. Project topic (e.g., parallel sorting algorithms)
Parallel sorting algorithms

### 2a. Brief project description (what algorithms will you be comparing and on what architectures)

- Bitonic Sort: Andrew
- Sample Sort: Brian
- Merge Sort: Anil
- Radix Sort: Gage

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

### 2c. Evaluation plan - what and how will you measure and compare
- Input sizes, Input types
- Strong scaling (same problem size, increase number of processors/nodes)
- Weak scaling (increase problem size, increase number of processors)
- Varying input sizes (powers of 2) and varying numbers of processors

### 3. Communication
Discord will be used as the primary means of meeting and communicating with everyone.
