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

// Could use bool here but whatever
static constexpr int MASTER(0), DECREASING(0), INCREASING(1);

template<typename T>
static void printvec(const vector<T>& A) {
    cout << "[ ";
    for (const T& v : A) {
        cout << v << " ";
    }
    cout << " ]" << endl;
}

static bool needsSwap(int v1, int v2, int dir) {
    if (dir == DECREASING && v1 < v2) {
        return true;
    }
    if (dir == INCREASING && v1 > v2) {
        return true;
    }

    return false;
}

int main(int argc, char** argv) {
    CALI_CXX_MARK_FUNCTION;

    if (argc != 2) {
        cerr << "Usage: sbatch bitonic.grace_job <# processes> <# elements>" << endl;
        return 1;
    }

    int n = stoi(argv[1]);

    // srand(time(nullptr));
    srand(0);

    MPI_Init(&argc, &argv);
    int rank, n_procs, n_workers;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &n_procs);

    n_workers = n_procs - 1;

    // Each process will get a value, master included
    int local_val;
    
    // Create caliper ConfigManager object https://software.llnl.gov/Caliper/ConfigManagerAPI.html
    cali::ConfigManager mgr;
    mgr.start();
    if (mgr.error()) {
        std::cerr << "Cali ConfigManager error: " << mgr.error_msg() << std::endl;
    }

    // The master task should do checks and MPI_Send each worker a value (workers will ofc Recv in that case)
    if (rank == MASTER) {
        // n > 1
        if (n <= 0) {
            cerr << "Please provide a number of elements greater than 0" << endl;
            MPI_Abort(MPI_COMM_WORLD, 1);

            // Bound checking isn't invalid per se
            exit(0);
        }

        // Assume n power of two (https://stackoverflow.com/questions/57025836/how-to-check-if-a-given-number-is-a-power-of-two)
        if (n & (n - 1) != 0) {
            cerr << "Number of elements must be a power of two" << endl;
            MPI_Abort(MPI_COMM_WORLD, 1);
            exit(0);
        }

        // Assume n = n_procs, for now FIXME: is it okay to use a local sort in the case of n_procs <= n?
        if (n != n_procs) {
            cerr << "Number of elements must be equal to number of processes" << endl;
            MPI_Abort(MPI_COMM_WORLD, 1);
            exit(0);
        }

        CALI_MARK_BEGIN("data_init_runtime");
        // Initialize and populate arr[n]
        vector<int> A(n);
        for (int& v : A) {
            v = rand();
        }
        CALI_MARK_END("data_init_runtime");

        printf("Initial (Unsorted) Array: ");
        printvec(A);

        // Send the array elements to all processes, master keeps 0
        // Although this is a loop of "small" communications, it is better suited to describe it as a "large" communication
        local_val = A[0];
        CALI_MARK_BEGIN("comm");
        CALI_MARK_BEGIN("comm_large");
        for (int i = 1; i < n; i++) {
            // int MPI_Send(const void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm)
            int rc = MPI_Send(&A[i], 1, MPI_INT, i, 0, MPI_COMM_WORLD);

            if (rc != MPI_SUCCESS) {
                cerr << "Initial master MPI_Send of values failed @ line " << __LINE__ << " with rc " << rc << endl;
                MPI_Abort(MPI_COMM_WORLD, 1);
            }
        }
        CALI_MARK_END("comm_large");
        CALI_MARK_END("comm");
    }
    else {
        // int MPI_Recv(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Status *status)
        CALI_MARK_BEGIN("comm");
        CALI_MARK_BEGIN("comm_small");
        MPI_Recv(&local_val, 1, MPI_INT, MASTER, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        CALI_MARK_END("comm_small");
        CALI_MARK_END("comm");
    }

    // Main algorithm TODO: Move this to a function
    int n_stages = int(log2(n_procs));
    
    for (int stage = 0; stage < n_stages; stage++) {
        for (int stride = 0; stride < stage + 1; stride++) {
            // These are small computations
            CALI_MARK_BEGIN("comp");
            CALI_MARK_BEGIN("comp_small");
            int stride_sz = 1 << (stage - stride);
            int partner_rank = rank ^ stride_sz;

            int half = rank / (1 << (stage + 1));
            int dir = INCREASING;
            if ((half % 2) == 1) {
                dir = DECREASING;
            }

            // TODO: Probably refactor to use MPI_Sendrecv
            if (rank < partner_rank) {
                CALI_MARK_END("comp_small");
                CALI_MARK_END("comp");

                CALI_MARK_BEGIN("comm");
                CALI_MARK_BEGIN("comm_small");
                int val_buf = -1;
                MPI_Recv(&val_buf, 1, MPI_INT, partner_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                CALI_MARK_END("comm_small");
                CALI_MARK_END("comm");
                
                CALI_MARK_BEGIN("comp");
                CALI_MARK_BEGIN("comp_small");
                if (needsSwap(local_val, val_buf, dir)) {
                    CALI_MARK_END("comp_small");
                    CALI_MARK_END("comp");

                    CALI_MARK_BEGIN("comm");
                    CALI_MARK_BEGIN("comm_small");
                    MPI_Send(&local_val, 1, MPI_INT, partner_rank, 0, MPI_COMM_WORLD);
                    local_val = val_buf;
                    CALI_MARK_END("comm_small");
                    CALI_MARK_END("comm");
                }
                else {
                    CALI_MARK_END("comp_small");
                    CALI_MARK_END("comp");

                    CALI_MARK_BEGIN("comm");
                    CALI_MARK_BEGIN("comm_small");
                    MPI_Send(&val_buf, 1, MPI_INT, partner_rank, 0, MPI_COMM_WORLD);
                    CALI_MARK_END("comm_small");
                    CALI_MARK_END("comm");
                }
            }
            else {
                CALI_MARK_END("comp_small");
                CALI_MARK_END("comp");

                CALI_MARK_BEGIN("comm");
                CALI_MARK_BEGIN("comm_small");
                MPI_Send(&local_val, 1, MPI_INT, partner_rank, 0, MPI_COMM_WORLD);

                int val_buf;
                MPI_Recv(&val_buf, 1, MPI_INT, partner_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                CALI_MARK_END("comm_small");
                CALI_MARK_END("comm");

                local_val = val_buf;
            }

            // We must wait for every process to finish at a given stride or vertical level
            CALI_MARK_BEGIN("comm");
            MPI_Barrier(MPI_COMM_WORLD);
            CALI_MARK_END("comm");
        }
    }

    // Make buffer known to each process, but only allocate space in master
    vector<int> A_sorted;
    if (rank == MASTER) {
        // Memory allocation is probably a "small" operation but it happens for the "large" amount of data (malloc(n * sizeof(int))) in this case
        CALI_MARK_BEGIN("comp");
        CALI_MARK_BEGIN("comp_large");
        A_sorted.resize(n);
        CALI_MARK_END("comp_large");
        CALI_MARK_END("comp");
    }

    // https://mpitutorial.com/tutorials/mpi-scatter-gather-and-allgather/
    // int MPI_Gather(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, int root, MPI_Comm comm)
    CALI_MARK_BEGIN("comm");
    CALI_MARK_BEGIN("comm_large");
    int rc = MPI_Gather(&local_val, 1, MPI_INT, &A_sorted[0], 1, MPI_INT, MASTER, MPI_COMM_WORLD);
    CALI_MARK_END("comm_large");
    CALI_MARK_END("comm");

    if (rc != MPI_SUCCESS) {
        cerr << "MPI_Gather failed @ line " << __LINE__ << " with rc " << rc << endl;
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    // Verification stage, assumes desired output is increasing
    if (rank == MASTER) {
        CALI_MARK_BEGIN("correctness_check");
        int idx(-1);

        // [2 1] 0 -> 1 < 2 ? yes, break @ idx=1
        while (idx++ < n - 1) {
            if (A_sorted[idx + 1] < A_sorted[idx]) {
                break;
            }
        }
        CALI_MARK_END("correctness_check");

        if (idx != (n - 1)) {
            printf("Sort failed @ idx [%d, %d]\n", idx, idx + 1);
        }
        else {
            printf("Sort successful\n");
        }

        printf("Final Array: ");
        printvec(A_sorted);
    }
    
    if (rank == MASTER) {
        adiak::init(NULL);
        adiak::launchdate();
        adiak::libraries();
        adiak::cmdline();
        adiak::clustername();
        adiak::value("algorithm", "bitonic"); // The name of the algorithm you are using (e.g., "merge", "bitonic")
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