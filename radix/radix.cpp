#include <mpi.h>
#include <iostream>
#include <vector>
#include <random>
#include <cmath>
#include <ctime>
#include <caliper/cali.h>
#include <caliper/cali-manager.h>
#include <adiak.hpp>

using namespace std;

#define SORTED 0
#define RANDOM 1
#define REVERSE 2
#define PERTURBED 3

int main(int argc, char *argv[]) {
    // CALI_CXX_MARK_FUNCTION;
    MPI_Init(&argc,&argv);
    
    MPI_Status status;
    int rc,
        rank, 
        n_procs,
        n_elems,
        input_type,
        n_per_proc,
        bound,
        local_length,
        global_length,
        slice,
        index,
        check,
        i,
        j,
        l,
        buffer,
        threshold,
        received; 
    vector<int>::const_iterator k;
    vector<int> arr,
                local_buckets,
                global_buckets,
                left_sum,
                global_sum,
                global_prefix,
                temp,
                recv_buffer,
                thing,
                *dest;
    vector<vector<int>> send_buffer;
    const char *data_init_X = "data_init_X",
               *comm = "comm",
               *comm_small = "comm_small",
               *comm_large = "comm_large",
               *comp = "comp",
               *comp_small = "comp_small",
               *comp_large = "comp_large",
               *correctness_check = "correctness_check";

    // Check all inputs present
    if (argc != 3) {
        cerr << "Usage: sbatch radix.grace_job <# processes> <# elements> <type of input>" << endl;
        MPI_Abort(MPI_COMM_WORLD, rc);
        exit(1);
    }

    // Get and check rank
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &n_procs);
    if ((n_procs & (n_procs - 1)) != 0) {
        cerr << "Number of processes must be a power of two" << endl;
        MPI_Abort(MPI_COMM_WORLD, rc);
        exit(1);
    }

    // Get and check array size
    n_elems = stoi(argv[1]);
    if (n_elems == 0 || (n_elems & (n_elems - 1)) != 0) {
        cerr << "Number of elements must be a power of two and greater than 0" << endl;
        MPI_Abort(MPI_COMM_WORLD, rc);
        exit(1);     
    }

    // Get and check input type
    input_type = stoi(argv[2]);
    if (input_type < 0 || input_type >= 4) {
        cerr << "Number of elements must be a power of two and greater than 0" << endl;
        MPI_Abort(MPI_COMM_WORLD, rc);
        exit(1);     
    }

    // Create caliper ConfigManager object
    cali::ConfigManager mgr;
    mgr.start();

    // Generate array
    CALI_MARK_BEGIN(data_init_X);
    n_per_proc = n_elems / n_procs;
    arr.assign(n_per_proc, 0);
    if (input_type == SORTED || input_type == PERTURBED) {
        j = rank * n_per_proc;
        for (i = 0; i < n_per_proc; ++i) {
            arr.at(i) = j;
            ++j;

        }
        if (input_type == PERTURBED) {
            srand(time(NULL) * (rank + n_procs)); 
            threshold = RAND_MAX * 0.01;
            for (i = 0; i < n_per_proc - 1; ++i) {
                if (rand() <= threshold) {
                    swap(arr.at(i), arr.at(i + 1));
                }
            }
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
        srand(time(NULL) * (rank + n_procs)); 
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
    // CALI_MARK_END(data_init_X);

    // Get local max length
    CALI_MARK_BEGIN(comp);
    CALI_MARK_BEGIN(comp_small);
    local_length = -1;
    for (const int& elem : arr) {
        local_length = max(local_length, elem);
    }
    local_length = ceil(log10(local_length));
    CALI_MARK_END(comp_small);
    CALI_MARK_END(comp);

    // Get global max length
    CALI_MARK_BEGIN(comm);
    CALI_MARK_BEGIN(comm_small);
    MPI_Allreduce(&local_length, &global_length, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
    CALI_MARK_END(comm_small);
    CALI_MARK_END(comm);

    // Radix
    slice = 1;
    global_buckets.assign(10 * n_procs, 0);
    global_prefix.assign(10, 0);
    send_buffer.assign(n_procs, vector<int>(1 + n_per_proc * 2, 0));
    recv_buffer.resize(n_procs);
    thing.resize(n_per_proc * 2);
    for (i = 0; i < global_length; ++i) {
        CALI_MARK_BEGIN(comp);
        CALI_MARK_BEGIN(comp_small);
        local_buckets.assign(10, 0);
        for (const int& elem : arr) {
            local_buckets.at((elem / slice) % 10) += 1;
        }

        // Aggregate bucket data
        CALI_MARK_END(comp_small);
        CALI_MARK_END(comp);

        CALI_MARK_BEGIN(comm);
        CALI_MARK_BEGIN(comm_large);
        MPI_Allgather(local_buckets.data(), 10, MPI_INT, global_buckets.data(), 10, MPI_INT, MPI_COMM_WORLD);
        CALI_MARK_END(comm_large);
        CALI_MARK_END(comm);

        CALI_MARK_BEGIN(comp);
        CALI_MARK_BEGIN(comp_large);        
        left_sum.assign(10, 0);
        global_sum.assign(10, 0);
        bound = 10 * n_procs;
        for (j = 0; j < bound; ++j) {
            if (j / 10 <= rank) {
                left_sum.at(j % 10) += global_buckets.at(j);

            }
            global_sum.at(j % 10) += global_buckets.at(j);

        }

        global_prefix.at(0) = global_sum.at(0);
        for (j = 1; j < 10; ++j) {
            global_prefix.at(j) = global_sum.at(j) + global_prefix.at(j - 1);

        }

        // Repartition across processes
        temp.resize(n_per_proc);
        for (k = arr.cend() - 1; k >= arr.cbegin(); --k) {
            j = (*k / slice) % 10;
            //      max index          to     min_index   to  position in set of vals 
            index = global_prefix.at(j) - global_sum.at(j) + left_sum.at(j) - 1; 
            global_prefix.at(j) -= 1;
            
            if (index / n_per_proc == rank) {
                temp.at(index % n_per_proc) = *k;
                received += 1;
            }
            else {
                dest = &send_buffer.at(index / n_per_proc);
                dest->at(0) += 2;
                dest->at(dest->at(0) - 1) = index % n_per_proc;
                dest->at(dest->at(0)) = *k;
            }
        }
        CALI_MARK_END(comp_large);
        CALI_MARK_END(comp);

        CALI_MARK_BEGIN(comm);
        CALI_MARK_BEGIN(comm_small);
        for (j = 0; j < n_procs; ++j) {
            MPI_Send(&send_buffer.at(j).at(0), 1, MPI_INT, j, 0, MPI_COMM_WORLD);
            MPI_Recv(&buffer, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
            recv_buffer.at(status.MPI_SOURCE) = buffer;
        }

        CALI_MARK_END(comm_small);
        MPI_Barrier(MPI_COMM_WORLD);

        CALI_MARK_BEGIN(comm_large);
        dest = &send_buffer.at(rank);
        for (j = 0; j < n_procs; ++j) {
            if (j != rank) {
                MPI_Send(&send_buffer.at(j).at(1), send_buffer.at(j).at(0), MPI_INT, j, 0, MPI_COMM_WORLD);
                send_buffer.at(j).at(0) = 0;
            }
        }
        for (j = 0; j < n_procs; ++j) {
            if (j != rank) {
                MPI_Recv(thing.data(), recv_buffer.at(j), MPI_INT, j, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                bound = recv_buffer.at(j);
                for (l = 0; l < bound; l += 2) {
                    temp.at(thing.at(l)) = thing.at(l + 1);
                }    
            }
        }

        CALI_MARK_END(comm_large);
        CALI_MARK_END(comm);

        CALI_MARK_BEGIN(comp);
        CALI_MARK_BEGIN(comp_small);
        slice *= 10;
        arr = move(temp);        
        CALI_MARK_END(comp_small);
        CALI_MARK_END(comp);
    }
 
    CALI_MARK_BEGIN(comm);
    MPI_Barrier(MPI_COMM_WORLD);
    CALI_MARK_END(comm);

    // Check sort
    CALI_MARK_BEGIN(correctness_check);
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
    CALI_MARK_END(correctness_check);

    cout << rank << ' ' << " sorted and exited successfully!" << endl;

    adiak::init(NULL);
    adiak::launchdate();    // launch date of the job
    adiak::libraries();     // Libraries used
    adiak::cmdline();       // Command line used to launch the job
    adiak::clustername();   // Name of the cluster
    adiak::value("algorithm", "radix"); // The name of the algorithm you are using (e.g., "merge", "bitonic")
    adiak::value("programming_model", "mpi"); // e.g. "mpi"
    adiak::value("data_type", "int"); // The datatype of input elements (e.g., double, int, float)
    adiak::value("size_of_data_type", sizeof(int)); // sizeof(datatype) of input elements in bytes (e.g., 1, 2, 4)
    adiak::value("input_size", n_elems); // The number of elements in input dataset (1000)
    // For sorting, this would be choices: ("Sorted", "ReverseSorted", "Random", "1_perc_perturbed")
    adiak::value("input_type", (input_type == 0) ? "sorted" : 
                               (input_type == 1) ? "ReveseSorted" :
                               (input_type == 2) ? "Random" : "1_perc_perturbed"); 
    adiak::value("n_procs", n_procs); // The number of processors (MPI ranks)
    adiak::value("scalability", "weak"); // The scalability of your algorithm. choices: ("strong", "weak")
    adiak::value("group_num", "3"); // The number of your group (integer, e.g., 1, 10)
    adiak::value("implementation_source", "Handwritten"); // Where you got the source code of your algorithm. choices: ("online", "ai", "handwritten").

    mgr.stop();
    mgr.flush();
    MPI_Finalize();

   return 0;
}