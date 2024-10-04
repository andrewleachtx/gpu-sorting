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
- For MPI programs, include MPI calls you will use to coordinate between processes
- Bitonic Sort:
- Sample Sort:
- Merge Sort:

global variable threadCount

global variable maxThreads

function merge (list1, list2):
- loop through the lists and create a merged list in sorted order
-  - This will involve keeping an index for list1 and list2 and comparing the items at those indexes, adding the item less than or equal to the other to the new list
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
- - Synch the threads with join
- - Decrement threadCount to potentially allow a new thread to be made (make sure to secure a lock before modifying the global variable)
- Else:
- - call parallelMergeSort() on the first half of the list
- - call parallelMergeSort() on the second half of the list
- Call merge using the two halves of the lists which will return the sorted list

- Radix Sort:

### 2c. Evaluation plan - what and how will you measure and compare
- Input sizes, Input types
- Strong scaling (same problem size, increase number of processors/nodes)
- Weak scaling (increase problem size, increase number of processors)

### 3. Communication
Discord will be used as the primary means of meeting and communicating with everyone.
