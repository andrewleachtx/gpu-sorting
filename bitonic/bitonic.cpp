#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <random>
#include <algorithm>

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

    // Each program receives an n / n_procs work (assuming valid input)
    int n_local = n / n_procs;
    vector<int> A_local(n_local);
    
    // Create caliper ConfigManager object https://software.llnl.gov/Caliper/ConfigManagerAPI.html
    cali::ConfigManager mgr;
    mgr.start();
    if (mgr.error()) {
        std::cerr << "Cali ConfigManager error: " << mgr.error_msg() << std::endl;
    }
    
    // The master task should do checks and MPI_Send each worker a value (workers will ofc Recv in that case)
    vector<int> A;
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

        CALI_MARK_BEGIN("data_init_runtime");
        // Initialize and populate arr[]
        A.resize(n);
        for (int& v : A) {
            v = rand() % 1000;
        }
        CALI_MARK_END("data_init_runtime");

        printf("Initial (Unsorted) Array: ");
        printvec(A);
    }
    
    // Much more efficient to use MPI_Scatter as opposed to a for loop - only weird part is now only the master has a sendbuf
    int* sendbuf = nullptr;
    if (rank == MASTER) {
        sendbuf = &A[0];
    }

    CALI_MARK_BEGIN("comm");
    CALI_MARK_BEGIN("comm_large");
    // int MPI_Scatter(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, int root, MPI_Comm comm)
    MPI_Scatter(sendbuf, n_local, MPI_INT, &A_local[0], n_local, MPI_INT, MASTER, MPI_COMM_WORLD);
    CALI_MARK_END("comm_large");
    CALI_MARK_END("comm");

    // Now, we should use some series sort for each buffer
    sort(A_local.begin(), A_local.end());

    int n_stages = int(log2(n_procs));

    for (int stage = 0; stage < n_stages; stage++) {
        for (int stride = 0; stride < stage + 1; stride++) {
            // These are small computations
            CALI_MARK_BEGIN("comp");
            CALI_MARK_BEGIN("comp_small");
            int stride_sz = 1 << (stage - stride);
            int partner_rank = rank ^ stride_sz;

            int dir = INCREASING;
            // Depending on what partition or "half" you are in, you will be decreasing or increasing
            int half = rank / (1 << (stage + 1));
            if ((half % 2) == 1) {
                dir = DECREASING;
            }

            vector<int> A_partner(n_local);
            CALI_MARK_END("comp_small");
            CALI_MARK_END("comp");

            CALI_MARK_BEGIN("comm");
            CALI_MARK_BEGIN("comm_large");
            // int MPI_Sendrecv(const void *sendbuf, int sendcount, MPI_Datatype sendtype, int dest, int sendtag, void *recvbuf, int recvcount, MPI_Datatype recvtype, int source, int recvtag, MPI_Comm comm, MPI_Status *status)
            MPI_Sendrecv(&A_local[0], n_local, MPI_INT, partner_rank, 0,
                         &A_partner[0], n_local,  MPI_INT, partner_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            CALI_MARK_END("comm_large");
            CALI_MARK_END("comm");

            /*
                At this point, we have our buffer in A_local[] and the partner's in A_partner[]. The question is, if we had 8 elements total,
                so four in A_local[] and 4 in A_partner[], how do we make it so both processes have the correct elements?

                What does correct mean? Well at each step we must verify that the buffer size has a bitonic ordering; thinking of the subarray
                buffer that A_local[] has as ONE UNIT, if we say it is fully INCREASING, then A_partner[] must be fully DECREASING.

                This way when any two processes later receive them, they have the correct bitonic ordering to work with.

                This is what the bitonic "merge" step looks like - if we have a fully sorted resultant array, we can easily redefine one
                partition as "increasing" and the other as "decreasing".

                The confusing part are these two cases:
                    1. If we are "INCREASING"
                        - The lesser rank should start with the first half of the partition
                        - Then of course the greater receives the second half
                    2. If we are "DECREASING"
                        - The lesser rank gets the second half
                        - The upper gets the first.

                It is confusing because it has nothing to do with inc or decreasing
            */

            // To merge, we can use two pointer approach that simply decides which value to pull from each time. Edit: I'm too lazy, use std::merge
            CALI_MARK_BEGIN("comp");
            CALI_MARK_BEGIN("comp_large");
            vector<int> merged(n_local * 2);
            merge(A_local.begin(), A_local.end(), A_partner.begin(), A_partner.end(), merged.begin());

            // TODO: There is a way to do this more concisely, but MVP
            int lesser_rank = min(rank, partner_rank);
            int higher_rank = max(rank, partner_rank);
            if (dir == INCREASING) {
                // Lesser rank gets the first half
                if (rank == lesser_rank) {
                    for (int i = 0; i < n_local; i++) {
                        A_local[i] = merged[i];
                    }
                }

                // Higher gets second half
                if (rank == higher_rank) {
                    for (int i = 0; i < n_local; i++) {
                        A_local[i] = merged[i + n_local];
                    }
                }
            }
            else {
                if (rank == higher_rank) {
                    for (int i = 0; i < n_local; i++) {
                        A_local[i] = merged[i];
                    }
                }

                if (rank == lesser_rank) {
                    for (int i = 0; i < n_local; i++) {
                        A_local[i] = merged[i + n_local];
                    }
                }
            }

            CALI_MARK_END("comp_large");
            CALI_MARK_END("comp");

            // We must wait for every process to finish at a given stride or vertical level
            CALI_MARK_BEGIN("comm");
            MPI_Barrier(MPI_COMM_WORLD);
            CALI_MARK_END("comm");
        }
    }

    // Make buffer known to each process, but only allocate space in master
    CALI_MARK_BEGIN("comp");
    CALI_MARK_BEGIN("comp_large");
    vector<int> A_sorted;
    int* recvbuf = nullptr;
    if (rank == MASTER) {
        // Memory allocation is probably a "small" operation but it happens for the "large" amount of data (malloc(n * sizeof(int))) in this case
        A_sorted.resize(n);
        recvbuf = &A_sorted[0];
    }
    CALI_MARK_END("comp_large");
    CALI_MARK_END("comp");

    // https://mpitutorial.com/tutorials/mpi-scatter-gather-and-allgather/
    // int MPI_Gather(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, int root, MPI_Comm comm)
    // We should send our buffer + n_local all to the A_sorted in the master process; a little weird but MPI_Gather knows
    CALI_MARK_BEGIN("comm");
    CALI_MARK_BEGIN("comm_large");
    MPI_Gather(&A_local[0], n_local, MPI_INT, recvbuf, n_local, MPI_INT, MASTER, MPI_COMM_WORLD);
    CALI_MARK_END("comm_large");
    CALI_MARK_END("comm");

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

        if (idx != n) {
            printf("Sort failed @ idx [%d, %d]\n", idx, idx + 1);
            printf("Sort failed, @ idx [%d, %d] with [%d, %d]\n", idx, idx + 1, A_sorted[idx], A_sorted[idx + 1]);
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