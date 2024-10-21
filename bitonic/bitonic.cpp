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

// Sorted, Random, Reverse sorted, 1%perturbed
void genInputArray(vector<int>& A, string type) {
    int n = A.size();

    if (type == "Sorted") {
        for (int i = 0; i < n; ++i) {
            A[i] = i;
        }
    }
    else if (type == "ReverseSorted") {
        for (int i = 0; i < n; ++i) {
            A[i] = n - i;
        }
    }
    else if (type == "Random") {
        for (int i = 0; i < n; ++i) {
            A[i] = rand() % 1000;
        }
    }
    else if (type == "1_perc_perturbed") {
        for (int i = 0; i < n; ++i) {
            A[i] = i;
        }
        // Our pertubation is a random swap - if n = 100, we would have 1; swaps => n / 100
        int num_swaps = n / 100;
        for (int i = 0; i < num_swaps; ++i) {
            int idx1 = rand() % n;
            int idx2 = rand() % n;

            int tmp = A[idx2];
            A[idx2] = A[idx1];
            A[idx1] = tmp;
        }
    }
    else {
        cerr << "Invalid array input type \"" << type << "\", please use Sorted, Random, ReverseSorted, or 1_perc_perturbed" << endl;
    }
}

int main(int argc, char** argv) {
    CALI_CXX_MARK_FUNCTION;

    // This is to be run with prepened mpirun -np $n_procs
    if (argc != 3) {
        cerr << "./bitonic $array_sz $array_input_type" << endl;
        return 1;
    }

    int n = stoi(argv[1]);
    string inp_type = argv[2];

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
        cerr << "Cali ConfigManager error: " << mgr.error_msg() << endl;
    }

    // The master task initializes the input array and distributes it
    vector<int> A;
    if (rank == MASTER) {
        // Check if n is greater than 0 and a power of two
        if (n <= 0 || (n & (n - 1)) != 0) {
            cerr << "Number of elements must be a power of two and greater than 0" << endl;
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        CALI_MARK_BEGIN("data_init_runtime");
        // Initialize and populate the array
        A.resize(n);
        genInputArray(A, inp_type);
        CALI_MARK_END("data_init_runtime");

        // printf("Initial (Unsorted) Array: ");
        // printvec(A);
    }
    
    // Scatter the array to all processes
    int* sendbuf = nullptr;
    if (rank == MASTER) {
        sendbuf = &A[0];
    }

    CALI_MARK_BEGIN("comm");
    CALI_MARK_BEGIN("comm_large");
    MPI_Scatter(sendbuf, n_local, MPI_INT, &A_local[0], n_local, MPI_INT, MASTER, MPI_COMM_WORLD);
    CALI_MARK_END("comm_large");
    CALI_MARK_END("comm");

    // Each process sorts its local array
    sort(A_local.begin(), A_local.end());

    int n_stages = (int)log2(n_procs);

    vector<int> merged(n_local * 2);
    vector<int> A_partner(n_local);
    for (int stage = 0; stage < n_stages; stage++) {
        for (int stride = 0; stride < stage + 1; stride++) {
            // These are small computations
            CALI_MARK_BEGIN("comp");
            CALI_MARK_BEGIN("comp_small");
            int stride_sz = 1 << (stage - stride);
            int partner_rank = rank ^ stride_sz;

            int dir = INCREASING;
            // Determine the sorting direction
            int half = rank / (1 << (stage + 1));
            if ((half % 2) == 1) {
                dir = DECREASING;
            }

            CALI_MARK_END("comp_small");
            CALI_MARK_END("comp");

            CALI_MARK_BEGIN("comm");
            CALI_MARK_BEGIN("comm_large");
            // Exchange data with the partner process
            MPI_Sendrecv(&A_local[0], n_local, MPI_INT, partner_rank, 0,
                         &A_partner[0], n_local,  MPI_INT, partner_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            CALI_MARK_END("comm_large");
            CALI_MARK_END("comm");

            // Merge the two arrays
            CALI_MARK_BEGIN("comp");
            CALI_MARK_BEGIN("comp_large");
            std::merge(A_local.begin(), A_local.end(), A_partner.begin(), A_partner.end(), merged.begin());

            int lesser_rank = min(rank, partner_rank);
            int higher_rank = max(rank, partner_rank);
            if (dir == INCREASING) {
                // Lesser rank gets the first half
                if (rank == lesser_rank) {
                    std::copy(merged.begin(), merged.begin() + n_local, A_local.begin());
                }
                else {
                    std::copy(merged.begin() + n_local, merged.end(), A_local.begin());
                }
            } else {
                // Lesser rank gets the second half
                if (rank == higher_rank) {
                    std::copy(merged.begin(), merged.begin() + n_local, A_local.begin());
                }
                else {
                    std::copy(merged.begin() + n_local, merged.end(), A_local.begin());
                }
            }
            CALI_MARK_END("comp_large");
            CALI_MARK_END("comp");

            // Synchronize processes
            CALI_MARK_BEGIN("comm");
            MPI_Barrier(MPI_COMM_WORLD);
            CALI_MARK_END("comm");
        }
    }

    // Gather the sorted subarrays to the master process
    CALI_MARK_BEGIN("comp");
    CALI_MARK_BEGIN("comp_large");
    vector<int> A_sorted;
    int* recvbuf = nullptr;
    if (rank == MASTER) {
        A_sorted.resize(n);
        recvbuf = &A_sorted[0];
    }
    CALI_MARK_END("comp_large");
    CALI_MARK_END("comp");

    CALI_MARK_BEGIN("comm");
    CALI_MARK_BEGIN("comm_large");
    MPI_Gather(&A_local[0], n_local, MPI_INT, recvbuf, n_local, MPI_INT, MASTER, MPI_COMM_WORLD);
    CALI_MARK_END("comm_large");
    CALI_MARK_END("comm");

    // Verification stage
    if (rank == MASTER) {
        CALI_MARK_BEGIN("correctness_check");
        bool sorted = is_sorted(A_sorted.begin(), A_sorted.end());
        CALI_MARK_END("correctness_check");

        if (!sorted) {
            printf("Sort failed\n");
        }
        else {
            printf("Sort successful\n");
        }

        // printf("Final Array: ");
        // printvec(A_sorted);
    }

    if (rank == MASTER) {
        adiak::init(NULL);
        adiak::launchdate();
        adiak::libraries();
        adiak::cmdline();
        adiak::clustername();
        adiak::value("algorithm", "bitonic"); // The name of the algorithm
        adiak::value("programming_model", "mpi"); // e.g., "mpi"
        adiak::value("data_type", "int"); // The datatype of input elements
        adiak::value("size_of_data_type", sizeof(int)); // Size of datatype in bytes
        adiak::value("input_size", n); // Number of elements in input dataset
        adiak::value("input_type", inp_type); // Input type
        adiak::value("num_procs", n_procs); // Number of processors (MPI ranks)
        adiak::value("scalability", "weak"); // The scalability of your algorithm
        adiak::value("group_num", 3); // Your group number
        adiak::value("implementation_source", "handwritten"); // Source code origin
    }

    // Flush Caliper output before finalizing MPI
    mgr.stop();
    mgr.flush();

    MPI_Finalize();
    return 0;
}