#include <mpi.h>
#include <iostream>
#include <vector>
#include <random>
#include <cmath>
#include <ctime>
// #include <caliper/cali.h>
// #include <caliper/cali-manager.h>

using namespace std;

#define SORTED 0
#define RANDOM 1
#define REVERSE 2
#define PERTURBED 3

int main(int argc, char *argv[]) {
    // CALI_CXX_MARK_FUNCTION;
    MPI_Init(&argc,&argv);
    
    // TODO initialize all iteration variables
    int rc;

    // Check all inputs present
    if (argc != 3) { // TODO check
        cerr << "Usage: sbatch radix.grace_job <# processes> <# elements> <type of input>" << endl;
        MPI_Abort(MPI_COMM_WORLD, rc);
        exit(1);
    }

    // Get and check rank
    int rank, n_procs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &n_procs);
    if ((n_procs & (n_procs - 1)) != 0) {
        cerr << "Number of processes must be a power of two" << endl;
        MPI_Abort(MPI_COMM_WORLD, rc);
        exit(1);
    }

    // Get and check array size
    int n_elems = stoi(argv[1]);
    if (n_elems == 0 || (n_elems & (n_elems - 1)) != 0) {
        cerr << "Number of elements must be a power of two and greater than 0" << endl;
        MPI_Abort(MPI_COMM_WORLD, rc);
        exit(1);     
    }

    // Get and check input type
    int input_type = stoi(argv[2]);
    if (input_type < 0 || input_type >= 4) {
        cerr << "Number of elements must be a power of two and greater than 0" << endl;
        MPI_Abort(MPI_COMM_WORLD, rc);
        exit(1);     
    }

    // Generate array
    int i;
    int j;
    vector<int>::const_iterator k;
    int bound;

    int n_per_proc = n_elems / n_procs;
    vector<int> arr(n_per_proc);
    if (input_type == SORTED || input_type == PERTURBED) {
        j = rank * n_per_proc;
        for (i = 0; i < n_per_proc; ++i) {
            arr.at(i) = j;
            ++j;

        }
        if (input_type == PERTURBED) {
            srand(0); // TODO seed with dynamic value
            int threshold = RAND_MAX * 0.50;
            for (i = 0; i < n_per_proc - 1; ++i) {
                if (rand() <= threshold) {
                    swap(arr.at(i), arr.at(i + 1));
                }
            }
            int buffer;
            if (rank != n_procs - 1) { // Swap forward
                buffer = rand() <= threshold ? arr.at(n_per_proc - 1) : -1;
                MPI_Send(&buffer, 1, MPI_INT, rank + 1, 0, MPI_COMM_WORLD);
            }
            if (rank != 0) { // Receive swap and respond
                MPI_Recv(&buffer, 1, MPI_INT, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                if (buffer != -1) {
                    swap(buffer, arr.at(0));
                }
                else {
                    buffer = -1;
                }
                MPI_Send(&buffer, 1, MPI_INT, rank - 1, 0, MPI_COMM_WORLD);
            }
            if (rank != n_procs - 1) { // Receive response
                MPI_Recv(&buffer, 1, MPI_INT, rank + 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                if (buffer != -1) {
                    arr.at(n_per_proc - 1) = buffer; 
                }
            }
        }
    }
    else if (input_type == RANDOM) {
        srand(0); // TODO seed with dynamic value
        for (i = 0; i < n_per_proc; ++i) {
            arr.at(i) = rand() % 1000;

        }
    }
    else if (input_type == REVERSE) {
        j = (n_procs - rank) * n_per_proc - 1;
        for (i = 0; i < n_per_proc; ++i) {
            arr.at(i) = j;
            --j;
        }
    }

    // Get local max length
    int local_length = -1;
    for (const int& elem : arr) {
        local_length = max(local_length, elem);
    }
    local_length = ceil(log10(local_length));

    // Get global max length
    int global_length;
    MPI_Allreduce(&local_length, &global_length, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);

    vector<int> local_buckets; // TODO reuse?
    vector<int> global_buckets;
    vector<int> left_prefix;
    vector<int> global_sum;
    vector<int> global_prefix;
    vector<int> temp; 
    vector<int> buffer; // TODO Fix with double name
    int slice = 1;
    int index;
    for (i = 0; i < global_length; ++i) {
        local_buckets.assign(10, 0);
        for (const int& elem : arr) {
            local_buckets.at((elem / slice) % 10) += 1;

        }

        // Aggregate bucket data
        global_buckets.assign(10 * n_procs, 0);
        MPI_Allgather(local_buckets.data(), 10, MPI_INT, global_buckets.data(), 10, MPI_INT, MPI_COMM_WORLD);

        left_prefix.assign(10, 0);
        global_sum.assign(10, 0);
        bound = 10 * n_procs;
        for (j = 0; j < bound; ++j) {
            if (j / 10 <= rank) {
                left_prefix.at(j % 10) += global_buckets.at(j);

            }
            global_sum.at(j % 10) += global_buckets.at(j);

        }

        global_prefix.assign(10, 0);
        global_prefix.at(0) = global_sum.at(0);
        for (j = 1; j < 10; ++j) {
            global_prefix.at(j) = global_sum.at(j) + global_prefix.at(j - 1);

        }

        // Repartition across processes
        temp.assign(n_per_proc, 0);
        buffer.assign(2, 0);
        for (k = arr.cend() - 1; k >= arr.cbegin(); --k) {
            j = (*k / slice) % 10;
            index = global_prefix.at(j) - global_sum.at(j) + left_prefix.at(j) - 1; // Earliest index (max index - # of val) + # of vals before (global and local) 
            global_prefix.at(j) -= 1;
            
            if (index / n_per_proc == rank) {
                temp.at(index % n_per_proc) = *k;
            }
            else {
                buffer.at(0) = index % n_per_proc;
                buffer.at(1) = *k;
                MPI_Send(buffer.data(), 2, MPI_INT, index / n_per_proc, 0, MPI_COMM_WORLD);
                MPI_Recv(buffer.data(), 2, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                temp.at(buffer.at(0)) = buffer.at(1);
            }
        }
        slice *= 10;
        arr = move(temp);
    }
 
    MPI_Barrier(MPI_COMM_WORLD);
    // Check sort
    int check;
    for (i = 1; i < n_per_proc; ++i) {
        if (arr.at(i) < arr.at(i - 1)) {
            cerr << "Sort failed" << endl;
            MPI_Abort(MPI_COMM_WORLD, rc);
            exit(1);     
        }
    }

    // Interprocess check
    if (rank != n_procs - 1) {
        MPI_Send(&arr.at(n_per_proc - 1), 1, MPI_INT, rank + 1, 0, MPI_COMM_WORLD);
    }
    if (rank != 0) {
        MPI_Recv(&check, 1, MPI_INT, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        if (arr.at(0) < check) {
            cerr << rank << "Sort failed" << endl;
            MPI_Abort(MPI_COMM_WORLD, rc);
            exit(1);     
        }
    }

    cout << rank << ' ' << " sorted and exited successfully!" << endl;

   // Create caliper ConfigManager object
//    cali::ConfigManager mgr;
//    mgr.start();


    // adiak::init(NULL);
    // adiak::launchdate();    // launch date of the job
    // adiak::libraries();     // Libraries used
    // adiak::cmdline();       // Command line used to launch the job
    // adiak::clustername();   // Name of the cluster
    // adiak::value("algorithm", algorithm); // The name of the algorithm you are using (e.g., "merge", "bitonic")
    // adiak::value("programming_model", programming_model); // e.g. "mpi"
    // adiak::value("data_type", data_type); // The datatype of input elements (e.g., double, int, float)
    // adiak::value("size_of_data_type", size_of_data_type); // sizeof(datatype) of input elements in bytes (e.g., 1, 2, 4)
    // adiak::value("input_size", input_size); // The number of elements in input dataset (1000)
    // adiak::value("input_type", input_type); // For sorting, this would be choices: ("Sorted", "ReverseSorted", "Random", "1_perc_perturbed")
    // adiak::value("n_procs", n_procs); // The number of processors (MPI ranks)
    // adiak::value("scalability", scalability); // The scalability of your algorithm. choices: ("strong", "weak")
    // adiak::value("group_num", group_number); // The number of your group (integer, e.g., 1, 10)
    // adiak::value("implementation_source", implementation_source); // Where you got the source code of your algorithm. choices: ("online", "ai", "handwritten").

    // mgr.stop();
    // mgr.flush();
    MPI_Finalize();

   return 0;
}