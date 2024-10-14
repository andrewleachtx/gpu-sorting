#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <random>

#include "mpi.h"
#include <caliper/cali.h>
#include <caliper/cali-manager.h>
#include <adiak.hpp>

using namespace std;

int MASTER = 0;

template<typename T>
static void printvec(const vector<T>& A) {
    cout << "[ ";
    for (const T& v : A) {
        cout << v << " ";
    }
    cout << " ]" << endl;
}

vector<int> merge(const vector<int>& list1, const vector<int>& list2){
    
    //take two sorted lists and merge them together
    //go through them both and insert lesser value in list until all values from both are inserted
    
    int list1Len = list1.size();
    int list2Len = list2.size();
    
    int index1 = 0;
    int index2 = 0;
    
    vector<int> mergedList;
    mergedList.reserve(list1Len + list2Len);
    
    while (index1 < list1Len && index2 < list2Len){
        if (list1[index1] <= list2[index2]){
            mergedList.push_back(list1[index1]);
            index1++;
        }
        else{
            mergedList.push_back(list2[index2]);
            index2++;
        }
        
    }
    
    while (index1 < list1Len){
        mergedList.push_back(list1[index1]);
        index1++;
    }
    
    while (index2 < list2Len){
        mergedList.push_back(list2[index2]);
        index2++;
    }
    
    return mergedList;
}

vector<int> mergeSort(vector<int>& list){
    
    //recursive mergesort function that takes a list, splits it in half then calls merge sort on both halves
    //once they are broken down into singular items, the lists are repeatedly merged
    
    int listLen = list.size();
    
    if (listLen <= 1){
        return list;
    }
    
    int midPoint = listLen / 2;
    
    vector<int> list1(list.begin(), list.begin() + midPoint);
    
    vector<int> list2(list.begin() + midPoint, list.end());
    
    return merge(mergeSort(list1), mergeSort(list2));
}

int main(int argc, char** argv) {
            
    CALI_CXX_MARK_FUNCTION;

    if (argc != 2) {
        cerr << "Usage: sbatch merge.grace_job <# processes> <# elements>" << endl;
        return 1;
    }

    int n = stoi(argv[1]);
    int inputType = 1;
    
    srand(0);

    MPI_Init(&argc, &argv);
    
    int rank;
    int n_procs; 
    int n_workers;
    
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &n_procs);

    n_workers = n_procs - 1;

    int local_val;
    
    cali::ConfigManager mgr;
    mgr.start();
    if (mgr.error()) {
        std::cerr << "Cali ConfigManager error: " << mgr.error_msg() << std::endl;
    }
    
    CALI_MARK_BEGIN("main");
    
    vector<int> A;
    
    if (rank == MASTER) {
        // n > 1
        if (n <= 0) {
            cerr << "Please provide a number of elements greater than 0" << endl;
            MPI_Abort(MPI_COMM_WORLD, 1);

            exit(0);
        }

        if (n & (n - 1) != 0) {
            cerr << "Number of elements must be a power of two" << endl;
            MPI_Abort(MPI_COMM_WORLD, 1);
            exit(0);
        }
        
        //fill in the data

        CALI_MARK_BEGIN("data_init_runtime");
        // Initialize and populate arr[]
        A.resize(n);
        
        //SORTED
        if (inputType == 0){
            for (int i = 0; i < n; i++) {
                A[i] = i;
            }
        }
        //RANDOM
        else if (inputType == 1){
            for (int& v : A) {
                v = rand() % 1000;
            }
        }
        //REVERSE
        else if (inputType == 2){
            for (int i = 0; i < n; i++) {
                A[i] = n - i;
            }
        }
        //PERTURBED
        else if (inputType == 3){
            for (int& v : A) {
                v = rand() % 1000;
                
                v = int(float(v) * 1.01);
            }
        }
        
        CALI_MARK_END("data_init_runtime");

        //printf("Initial (Unsorted) Array: ");
        //printvec(A);
    }
    
    //use MPI scatter to send each process its workload
    
    int* fullBuf = nullptr;
    if (rank == MASTER) {
        fullBuf = &A[0];
    }
    
    int workSize = n / n_procs;
    
    vector<int> workload(workSize);
    
    CALI_MARK_BEGIN("comm");
    CALI_MARK_BEGIN("comm_large");
    
    MPI_Scatter(fullBuf, workSize, MPI_INT, &workload[0], workSize, MPI_INT, MASTER, MPI_COMM_WORLD);
    
    CALI_MARK_END("comm_large");
    CALI_MARK_END("comm");
    
    CALI_MARK_BEGIN("comp");
    CALI_MARK_BEGIN("comp_large");
    
    //each process gets an equal amount of work to send to the mergesort function
    
    vector<int> localSort = mergeSort(workload);
    
    CALI_MARK_END("comp_large");
    CALI_MARK_END("comp");
            
    int procCount = n_procs;
    
    procCount /= 2;
    
    //finally, once all the the processes are done with merge sort, merge together all the remaining lists
    //this will be done in parallel, for every iteration the amount of processeors working will be cut in half
    
    while(procCount >= 1){       
        
        int listSize = n / (2 * procCount);
        
        if (rank < procCount){
            
            vector<int> otherHalf(listSize);
            
            //GET A LIST FROM ANOTHER PROCESSOR
            
            CALI_MARK_BEGIN("comm");
    
            CALI_MARK_BEGIN("comm_large");
            
            MPI_Recv(&otherHalf[0], listSize, MPI_INT, procCount + rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            
            CALI_MARK_END("comm_large");
    
            CALI_MARK_END("comm");
            
            //CALL MERGE ON BOTH LISTS
            
            CALI_MARK_BEGIN("comp");
            
            CALI_MARK_BEGIN("comp_large");

            vector<int> mergedSort = merge(localSort, otherHalf);
            localSort = mergedSort; 
            
            CALI_MARK_END("comp_large");
            
            CALI_MARK_END("comp");
            
        }
        else{
            
            int destination = rank - procCount;
            
            CALI_MARK_BEGIN("comm");
    
            CALI_MARK_BEGIN("comm_large");
           
            MPI_Send(&localSort[0], localSort.size(), MPI_INT, destination, 0, MPI_COMM_WORLD);
            
            CALI_MARK_END("comm_large");
    
            CALI_MARK_END("comm");
            
            break; 
            
        }
        
        procCount /= 2;
    }
            
    vector<int> A_sorted = localSort;
    
    bool sortingValidity = true;
    
    // Verification stage, assumes desired output is increasing
    if (rank == MASTER) {
        
        CALI_MARK_BEGIN("correctness_check");
        
        int ind;

        for (ind = 0 ; ind < n - 1; ind++){
            if (A_sorted[ind + 1] < A_sorted[ind]) {
                break;
            }
        }
            
        if (ind != (n - 1)) {
            sortingValidity = false;
        }
        
        CALI_MARK_END("correctness_check");

        //printf("Final Array: ");
        
    }
    
    CALI_MARK_END("main");
    
    if (rank == MASTER) {
        
        if (sortingValidity){
            
            printf("Sort successful\n");
            
        }
        else{
            printf("Sort failed\n");
        }

        printvec(A_sorted);
    }
    
    if (rank == MASTER) {
        adiak::init(NULL);
        adiak::launchdate();
        adiak::libraries();
        adiak::cmdline();
        adiak::clustername();
        adiak::value("algorithm", "merge"); // The name of the algorithm you are using (e.g., "merge", "bitonic")
        adiak::value("programming_model", "mpi"); // e.g. "mpi"
        adiak::value("data_type", "int"); // The datatype of input elements (e.g., double, int, float)
        adiak::value("size_of_data_type", sizeof(int)); // sizeof(datatype) of input elements in bytes (e.g., 1, 2, 4)
        adiak::value("input_size", n); // The number of elements in input dataset (1000)
        adiak::value("input_type", "Random"); // For sorting, this would be choices: ("Sorted", "ReverseSorted", "Random", "1_perc_perturbed")
        adiak::value("num_procs", n_procs); // The number of processors (MPI ranks)
        adiak::value("scalability", "weak"); // The scalability of your algorithm. choices: ("strong", "weak")
        adiak::value("group_num", 3); // The number of your group (integer, e.g., 1, 10)
        adiak::value("implementation_source", "handwritten"); // Where you got the source code of your algorithm. choices: ("online", "ai", "handwritten").
    }

    // Flush Caliper output before finalizing MPI
    mgr.stop();
    mgr.flush();

    MPI_Finalize();
    return 0;
}
