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
    CALI_CXX_MARK_FUNCTION;
    MPI_Init(&argc,&argv);
    
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
        i,
        j,
        k,
        buffer,
        received,
        sent,
        local_buckets[10],
        left_sum[10],
        global_sum[10],
        global_prefix[10];
    vector<int>::const_iterator it;
    vector<int> arr, // n_per_proc
                global_buckets, // 10 * n_procs
                temp, // n_per_proc
                locations, // n_procs
                sizes; // n_procs
    vector<vector<int>> send_buffer, // n_per_proc * 2
                        recv_buffer; // n_per_proc * 2
    vector<MPI_Request> requests; // n_procs
    vector<MPI_Status> statuses; // n_procs
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
    arr.reserve(n_per_proc);
    if (input_type == SORTED || input_type == PERTURBED) {
        j = rank * n_per_proc;
        for (i = 0; i < n_per_proc; ++i) {
            arr.push_back(j);
            ++j;

        }
        if (input_type == PERTURBED) {
            srand(time(NULL) * (rank + n_procs)); 
            bound = RAND_MAX * 0.01;
            for (i = 0; i < n_per_proc - 1; ++i) {
                if (rand() <= bound) {
                    swap(arr[i], arr[i + 1]);

                }

            }
            if (rank != n_procs - 1) { // Swap forward
                buffer = (rand() <= bound) ? arr[n_per_proc - 1] : -1;
                MPI_Send(&buffer, 1, MPI_INT, rank + 1, 0, MPI_COMM_WORLD);

            }
            if (rank != 0) { // Receive swap and respond
                MPI_Recv(&buffer, 1, MPI_INT, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                if (buffer != -1) {
                    swap(buffer, arr[0]);

                }
                else {
                    buffer = -1;

                }
                MPI_Send(&buffer, 1, MPI_INT, rank - 1, 0, MPI_COMM_WORLD);

            }
            if (rank != n_procs - 1) { // Receive response
                MPI_Recv(&buffer, 1, MPI_INT, rank + 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                if (buffer != -1) {
                    arr[n_per_proc - 1] = buffer; 

                }

            }

        }

    }
    else if (input_type == RANDOM) {
        srand(time(NULL) * (rank + n_procs)); 
        for (i = 0; i < n_per_proc; ++i) {
            arr.push_back(rand() % (n_elems / 4));

        }

    }
    else if (input_type == REVERSE) {
        j = (n_procs - rank) * n_per_proc - 1;
        for (i = 0; i < n_per_proc; ++i) {
            arr.push_back(j);
            --j;

        }

    }
    CALI_MARK_END(data_init_X);

    // Get local max length
    CALI_MARK_BEGIN(comp);
    CALI_MARK_BEGIN(comp_small);
    local_length = -1;
    for (const int& elem : arr) {
        local_length = max(local_length, elem);
    }
    local_length = ceil(log10(local_length));    
    slice = 1;
    global_buckets.resize(10 * n_procs);
    memset(global_prefix, 0, 10 * sizeof(int));
    requests.resize(n_procs * 2);
    statuses.resize(n_procs * 2);
    CALI_MARK_END(comp_small);
    CALI_MARK_END(comp);

    // Get global max length
    CALI_MARK_BEGIN(comm);
    CALI_MARK_BEGIN(comm_small);
    MPI_Allreduce(&local_length, &global_length, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
    CALI_MARK_END(comm_small);
    CALI_MARK_END(comm);

    // Radix Sort
    for (i = 0; i < global_length; ++i) {
        CALI_MARK_BEGIN(comp);
        CALI_MARK_BEGIN(comp_small);
        // Local histogram
        memset(local_buckets, 0, 10 * sizeof(int));
        for (const int& elem : arr) {
            local_buckets[(elem / slice) % 10] += 1;

        }
        CALI_MARK_END(comp_small);
        CALI_MARK_END(comp);

        CALI_MARK_BEGIN(comm);
        CALI_MARK_BEGIN(comm_large);
        // Aggregate bucket data
        MPI_Allgather(local_buckets, 10, MPI_INT, global_buckets.data(), 10, MPI_INT, MPI_COMM_WORLD);
        CALI_MARK_END(comm_large);
        CALI_MARK_END(comm);

        CALI_MARK_BEGIN(comp);
        CALI_MARK_BEGIN(comp_large);    
        // Calculate global prefix and sums    
        memset(left_sum, 0, 10 * sizeof(int));
        memset(global_sum, 0, 10 * sizeof(int));
        bound = 10 * n_procs;
        for (j = 0; j < bound; ++j) {
            if (j / 10 <= rank) {
                left_sum[j % 10] += global_buckets[j]; 

            }
            global_sum[j % 10] += global_buckets[j];

        }

        global_prefix[0] = global_sum[0];
        for (j = 1; j < 10; ++j) {
            global_prefix[j] = global_sum[j] + global_prefix[j - 1];

        }

        // Repartition across processes
        received = 0;
        temp.resize(n_per_proc);
        send_buffer.assign(n_procs, vector<int>());
        for (it = arr.cend() - 1; it >= arr.cbegin(); --it) {
            j = (*it / slice) % 10;
            //      max index          to     min_index   to  position in set of vals 
            index = global_prefix[j] - global_sum[j] + left_sum[j] - 1; 
            global_prefix[j] -= 1;

            buffer = index / n_per_proc;
            if (buffer == rank) {
                temp[index % n_per_proc] = *it;
                received += 2;

            }
            else {
                send_buffer[buffer].push_back(index % n_per_proc);
                send_buffer[buffer].push_back(*it);

            }

        }
        CALI_MARK_END(comp_large);
        CALI_MARK_END(comp);

        CALI_MARK_BEGIN(comm);
        CALI_MARK_BEGIN(comm_small);
        // Send sizes
        sent = 0;
        for (j = 0; j < n_procs; ++j) {
            if (send_buffer[j].size() > 0) {
                send_buffer[j].push_back(send_buffer[j].size());
                MPI_Isend(&send_buffer[j].back(), 1, MPI_INT, j, 0, MPI_COMM_WORLD, requests.data());
                sent += 1;

            }
            send_buffer[j].shrink_to_fit();

        }

        // Receive sizes
        locations.clear();
        sizes.clear();
        while (received != (n_per_proc * 2)) {
            MPI_Recv(&buffer, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, statuses.data());
            locations.push_back(statuses[0].MPI_SOURCE);
            sizes.push_back(buffer);
            received += buffer;

        }
        locations.shrink_to_fit();
        sizes.shrink_to_fit();
        recv_buffer.assign(locations.size(), vector<int>());
        CALI_MARK_END(comm_small);

        MPI_Barrier(MPI_COMM_WORLD);

        CALI_MARK_BEGIN(comm_large);
        // Send elements
        received = (int) locations.size();
        j = 0;
        k = 0;
        requests.resize(sent + received);
        statuses.resize(sent + received);
        while (j != n_procs || k != received) {
            if (j < n_procs && send_buffer[j].size() != 0) {
                sent -= 1;
                MPI_Isend(send_buffer[j].data(), send_buffer[j].back(), MPI_INT, j, 0, MPI_COMM_WORLD, &requests[received + sent]);

            }
            ++j;

            if (k < received) {
                recv_buffer[k].resize(sizes[k]);
                MPI_Irecv(recv_buffer[k].data(), sizes[k], MPI_INT, locations[k], MPI_ANY_TAG, MPI_COMM_WORLD, &requests[k]);
                ++k;

            }

        }
        MPI_Waitall(sent + received, requests.data(), statuses.data());
        CALI_MARK_END(comm_large);
        CALI_MARK_END(comm);

        CALI_MARK_BEGIN(comp);
        CALI_MARK_BEGIN(comp_large);
        // Place received elements
        for (j = 0; j < received; ++j) {
            bound = (int) recv_buffer[j].size();

            for (k = 0; k < bound; k += 2) {
                temp[recv_buffer[j][k]] = recv_buffer[j][k + 1];

            }

        }
        slice *= 10;
        arr = move(temp);        
        CALI_MARK_END(comp_large);
        CALI_MARK_END(comp);

    }
 
    CALI_MARK_BEGIN(comm);
    MPI_Barrier(MPI_COMM_WORLD);
    CALI_MARK_END(comm);

    // Check sort
    CALI_MARK_BEGIN(correctness_check);
    for (i = 1; i < n_per_proc; ++i) {
        if (arr[i] < arr[i - 1]) {
            cerr << "Sort failed" << endl;
            MPI_Abort(MPI_COMM_WORLD, rc);
            exit(1);    

        }

    }

    // Interprocess check
    if (rank != n_procs - 1) {
        MPI_Send(&arr[n_per_proc - 1], 1, MPI_INT, rank + 1, 0, MPI_COMM_WORLD);

    }
    if (rank != 0) {
        MPI_Recv(&buffer, 1, MPI_INT, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        if (arr[0] < buffer) {
            cerr << rank << "Sort failed" << endl;
            MPI_Abort(MPI_COMM_WORLD, rc);
            exit(1);   

        }

    }
    MPI_Barrier(MPI_COMM_WORLD);
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