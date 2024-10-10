#include <iostream>
#include <vector>
#include <string>
#include <cmath>

#include "mpi.h"
#include <caliper/cali.h>
#include <caliper/cali-manager.h>
#include <adiak.hpp>

using namespace std;

// Could use bool here but whatever
static constexpr int MASTER(0), DECREASING(0), INCREASING(1);

bool needsSwap(int v1, int v2, int dir) {
    if (dir == DECREASING && v1 < v2) {
        return true;
    }
    if (dir == INCREASING && v1 > v2) {
        return true;
    }

    return false;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        cerr << "Usage: sbatch bitonic.grace_job <# processes> <# elements>" << endl;
        return 1;
    }

    size_t n = stoi(argv[1]);

    MPI_Init(&argc, &argv);

    int rank, n_procs, n_workers;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &n_procs);

    n_workers = n_procs - 1;

    // Each process will get a value, master included
    int local_val;

    // The master task should do checks and MPI_Send each worker a value (workers will ofc Recv in that case)
    if (rank == MASTER) {
        // Assume n power of two (https://stackoverflow.com/questions/57025836/how-to-check-if-a-given-number-is-a-power-of-two)
        if (n & (n - 1) != 0) {
            cerr << "Number of elements must be a power of two" << endl;
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        // Assume n = n_procs, for now
        if (n != n_procs) {
            cerr << "Number of elements must be equal to number of processes" << endl;
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        // Initialize and populate arr[n]
        // srand(time(nullptr));
        // for (int& v : A) {
        //     v = rand();
        // }
        vector<int> A(n);
        for (int i = 0; i < n; i++) {
            A[n - i - 1] = i;
            printf("%d ", i);
        }
        printf("\n");

        printf("Initial, unsorted array: [ ");
        for (const int& v : A) {
            printf("%d ", v);
        }
        printf(" ]\n");

        // Send the array elements to all processes, master keeps 0
        local_val = A[0];
        for (int i = 1; i < n; i++) {
            // int MPI_Send(const void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm)
            int rc = MPI_Send(&A[i], 1, MPI_INT, i, 0, MPI_COMM_WORLD);

            if (rc != MPI_SUCCESS) {
                cerr << "Initial master MPI_Send of values failed @ line " << __LINE__ << " with rc " << rc << endl;
                MPI_Abort(MPI_COMM_WORLD, 1);
            }
        }
    }
    else {
        // int MPI_Recv(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Status *status)
        MPI_Recv(&local_val, 1, MPI_INT, MASTER, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE); // TODO: Add a MPI_Status &status here? 
    }

    // Main algorithm TODO: Move this to a function...
    int n_stages = int(log2(n_procs)); // TODO: log2(nprocs) + 1?

    for (int stage = 0; stage < n_stages; stage++) {
        for (int stride = 0; stride < stage + 1; stride++) {
            int stride_sz = 1 << (stage - stride);
            int partner_rank = rank ^ stride_sz;

            int half = rank / (1 << (stage + 1));
            int dir = INCREASING;
            if (half == 1) {
                dir = DECREASING;
            }

            // TODO: Probably refactor to use MPI_Sendrecv
            if (rank < partner_rank) {
                int val_buf = -1;
                MPI_Recv(&val_buf, 1, MPI_INT, partner_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

                if (needsSwap(local_val, val_buf, dir)) {
                    MPI_Send(&local_val, 1, MPI_INT, partner_rank, 0, MPI_COMM_WORLD);
                    local_val = val_buf;
                }
                else {
                    MPI_Send(&val_buf, 1, MPI_INT, partner_rank, 0, MPI_COMM_WORLD);
                }
            }
            else {
                MPI_Send(&local_val, 1, MPI_INT, partner_rank, 0, MPI_COMM_WORLD);

                int val_buf = local_val;
                MPI_Recv(&val_buf, 1, MPI_INT, partner_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

                local_val = val_buf;
            }

            // We must wait for every process to finish at a given stride or vertical level
            MPI_Barrier(MPI_COMM_WORLD);
        }
    }

    // Make buffer known to each process, but only allocate space in master
    vector<int> A_sorted;
    if (rank == MASTER) {
        A_sorted.resize(n);
    }

    // https://mpitutorial.com/tutorials/mpi-scatter-gather-and-allgather/
    // int MPI_Gather(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, int root, MPI_Comm comm)
    int rc = MPI_Gather(&local_val, 1, MPI_INT, &A_sorted[0], 1, MPI_INT, MASTER, MPI_COMM_WORLD);

    if (rc != MPI_SUCCESS) {
        cerr << "MPI_Gather failed @ line " << __LINE__ << " with rc " << rc << endl;
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    // Can print to test
    if (rank == MASTER) {
        printf("Final, sorted array: [ ");
        for (const int& v : A_sorted) {
            printf("%d ", v);
        }
        printf(" ]\n");
    }

    MPI_Finalize();
    return 0;
}