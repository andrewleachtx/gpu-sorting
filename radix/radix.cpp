#include <iostream>
#include <vector>
#include <random>
// #include <caliper/cali.h>
// #include <caliper/cali-manager.h>

using namespace std;

#define SORTED 0
#define RANDOM 1
#define REVERSE 2
#define PERTURBED 3

int main(int argc, char *argv[]) {
    // CALI_CXX_MARK_FUNCTION;
    // MPI_Init(&argc,&argv);

    // Check all inputs present
    if (argc != 4) { // TODO change to 3
        cerr << "Usage: sbatch radix.grace_job <# processes> <# elements> <type of input>" << endl;
        // MPI_Abort(MPI_COMM_WORLD, rc);
        exit(1);
    }

    // Get and check rank
    int rank, n_procs;
    // MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    // MPI_Comm_size(MPI_COMM_WORLD, &n_procs);
    n_procs = stoi(argv[1]); // TODO remove
    rank = 3; // TODO replace with above comments
    if (n_procs % 2 != 0) {
        cerr << "Number of processes must be a power of two" << endl;
        // MPI_Abort(MPI_COMM_WORLD, rc);
        exit(1);
    }

    // Get and check array size
    int num_elem = stoi(argv[2]);
    if (num_elem % 2 != 0 || num_elem == 0) {
        cerr << "Number of elements must be a power of two and greater than 0" << endl;
        // MPI_Abort(MPI_COMM_WORLD, rc);
        exit(1);     
    }

    // Get and check input type
    int input_type = stoi(argv[3]);
    if (input_type < 0 || input_type >= 4) {
        cerr << "Number of elements must be a power of two and greater than 0" << endl;
        // MPI_Abort(MPI_COMM_WORLD, rc);
        exit(1);     
    }

    int num_per_proc = num_elem / n_procs;
    vector<int> arr(num_per_proc);
    if (input_type == SORTED) {
        for (int i = rank * num_per_proc; i < (rank + 1) * num_per_proc; ++i) {
            arr.at(i % num_per_proc) = i;

        }
    }
    else if (input_type == RANDOM) {
        srand(0); // TODO seed with dynamic value
        for (int i = 0; i < num_per_proc; ++i) {
            arr.at(i) = rand() % 1000;

        }
    }
    else if (input_type == REVERSE) {
        int j = (n_procs - rank) * num_per_proc - 1; // value
        for (int i = 0; i < num_per_proc; ++i) {
            arr.at(i) = j;
            --j;
        }
    }
    else if (input_type == PERTURBED) {
        for (int i = rank*num_per_proc; i < num_per_proc; ++i) {
            arr.at(i % num_per_proc) = i;

        }
        // TODO implement transfer across
    }

    for (auto elem : arr) {
        cout << elem << ' ';
    }
    cout << endl;


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
    // adiak::value("num_procs", num_procs); // The number of processors (MPI ranks)
    // adiak::value("scalability", scalability); // The scalability of your algorithm. choices: ("strong", "weak")
    // adiak::value("group_num", group_number); // The number of your group (integer, e.g., 1, 10)
    // adiak::value("implementation_source", implementation_source); // Where you got the source code of your algorithm. choices: ("online", "ai", "handwritten").

    // mgr.stop();
    // mgr.flush();
    // MPI_Finalize();

   return 0;
}