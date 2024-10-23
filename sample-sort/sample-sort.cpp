#include <stdio.h>
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

#include "mpi.h"
#include <caliper/cali.h>
#include <caliper/cali-manager.h>
#include <adiak.hpp>

#define MASTER 0

using namespace std;

int main(int argc, char** argv) {
  CALI_CXX_MARK_FUNCTION;

  if(argc != 3) {
    cout << "Usage: incorrect number of arguments" << endl;
    cout << "Please provide as follows: <number of processors> <input size> <oversampling factor>" << endl;
    exit(1);
  }

  //define caliper region names
  const char* data_init_runtime = "data_init_runtime";
  const char* comm = "comm";
  const char* comm_small = "comm_small";
  const char* comm_large = "comm_large";
  const char* comp = "comp";
  const char* comp_small = "comp_small";
  const char* comp_large = "comp_large";
  const char* correctness_check = "correctness_check";

  MPI_Init(&argc, &argv);

  // Create caliper ConfigManager object
  cali::ConfigManager mgr;
  mgr.start();

  if (mgr.error()) {
    std::cerr << "Cali ConfigManager error: " << mgr.error_msg() << std::endl;
  }

  int rank, n_procs;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &n_procs);

  int num_workers = n_procs - 1;
  int input_size = atoi(argv[1]);
  char* input_type = argv[2];
  int oversampling_factor = 8;
  vector<int> result;

  CALI_MARK_BEGIN("main");
  //send bucket boundaries to worker processes
  if(rank == MASTER) {
    //create input
    CALI_MARK_BEGIN(data_init_runtime);
    vector<int> input;
    if(strcmp(input_type, "Random") == 0) {
      for(int i = 0; i < input_size; i++) {
        input.push_back(rand() % 100);
      }
    } else if(strcmp(input_type, "Sorted") == 0) {
      for(int i = 0; i < input_size; i++) {
        input.push_back(i);
      }
    } else if(strcmp(input_type, "ReverseSorted") == 0) {
      for(int i = input_size - 1; i >= 0; i--) {
        input.push_back(i);
      }
    } else if(strcmp(input_type, "1_perc_perturbed") == 0){
      for(int i = 0; i < input_size; i++) {
        if(((float) i) / input_size <= .01) {
          input.push_back(rand() % 100);
        } else {
          input.push_back(i);
        }
      }
    }

    //sample p*k values
    //assumes p*k is less than size of input
    if(num_workers * oversampling_factor > input_size) {
      cout << "Usage Error: sampling size > input size" << endl;
      MPI_Abort(MPI_COMM_WORLD, 1);
    }

    vector<int> sample;
    for(int i = 0; i < num_workers * oversampling_factor; i++) {
      sample.push_back(input[i]);
    }
    CALI_MARK_END(data_init_runtime);

    //sort sampled values
    CALI_MARK_BEGIN(comp);
    CALI_MARK_BEGIN(comp_small);
    sort(sample.begin(), sample.end());

    //create buckets
    vector<int> pivots;
    int step_size = sample.size() / num_workers;
    for(int i = 0; i < sample.size(); i += step_size) {
      pivots.push_back(sample[i]);
    }
    pivots.push_back(sample[sample.size() - 1]);

    // for(int i = 0; i < pivots.size(); i++) {
    //   cout << pivots[i] << ", ";
    // }
    // cout << endl;
   
    CALI_MARK_END(comp_small);
    CALI_MARK_END(comp);

    CALI_MARK_BEGIN(comm);
    CALI_MARK_BEGIN(comm_small);
    for(int i = 1; i <= num_workers; i++) {
      MPI_Send(&(pivots[0]), pivots.size(), MPI_INT, i, 0, MPI_COMM_WORLD);
    }
    CALI_MARK_END(comm_small);

    //send workloads to each processor
    if(input_size % num_workers != 0) {
      cout << "Usage Error: Work is not able to be evenly divided amongst workers" << endl;
      MPI_Abort(MPI_COMM_WORLD, 1);
    }

    int workload_size = input_size / num_workers;
    CALI_MARK_BEGIN(comm_large);
    for(int i = 0; i < num_workers; i++) {
      MPI_Send(&(input[i * workload_size]), workload_size, MPI_INT, i + 1, 0, MPI_COMM_WORLD);
    }

    //receive bucket data from each processor
    for(int i = 1; i <= num_workers; i++) {
      int data_size;
      MPI_Recv(&data_size, 1, MPI_INT, i, i, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
      int* buf = new int[data_size];
      MPI_Recv(buf, data_size, MPI_INT, i, i, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
      for(int j = 0; j < data_size; j++) {
        result.push_back(buf[j]);
      }
    }
    CALI_MARK_END(comm_large);

    CALI_MARK_END(comm);

  } else {
    //worker processor
    //receive bucket boundaries
    int* pivots = new int[num_workers + 1];
    
    CALI_MARK_BEGIN(comm);
    CALI_MARK_BEGIN(comm_large);
    MPI_Recv(pivots, num_workers + 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    //receive workload
    int workload_size = input_size / num_workers;
    int* workload = new int[workload_size];
    MPI_Recv(workload, workload_size, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    CALI_MARK_END(comm_large);
    CALI_MARK_END(comm);

    // cout << "Received Workload" << endl;
    // for(int i = 0; i < workload_size; i++) {
    //   cout << workload[i] << ", ";
    // }
    // cout << endl;
    // cout << "Workload Size: " << workload_size << endl;

    //sort data into buckets
    CALI_MARK_BEGIN(comp);
    CALI_MARK_BEGIN(comp_large);
    vector<vector<int>> buckets(num_workers, vector<int>());
    for(int i = 0; i < workload_size; i++) {
      int num = workload[i];
      bool added = false;
      for(int j = 0; j < num_workers; j++) {
        int lower_bound = pivots[j];
        int upper_bound = pivots[j + 1];
        if(num >= lower_bound && num <= upper_bound) {
          buckets[j].push_back(num);
          added = true;
          break;
        }
      }
      if(!added) {
        if(num < pivots[0]) {
          buckets[0].push_back(num);
        } else {
          buckets[buckets.size() - 1].push_back(num);
        }
      }
  
    }
    CALI_MARK_END(comp_large);
    CALI_MARK_END(comp);

    //send bucket data to respective processor
    CALI_MARK_BEGIN(comm);
    CALI_MARK_BEGIN(comm_large);
    for(int i = 0; i < buckets.size(); i++) {
      if(i + 1 == rank) {
        continue;
      }
      int bucket_size = buckets[i].size();
      MPI_Send(&bucket_size, 1, MPI_INT, i + 1, rank, MPI_COMM_WORLD);
      MPI_Send(&(buckets[i][0]), buckets[i].size(), MPI_INT, i + 1, rank, MPI_COMM_WORLD);
    }
  
    //receive data for our bucket
    for(int i = 1; i <= num_workers; i++) {
      if(i == rank) {
        continue;
      }
      int data_size;
      MPI_Recv(&data_size, 1, MPI_INT, i, i, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

      int* buf = new int[data_size];
      MPI_Recv(buf, data_size, MPI_INT, i, i, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

      //store received data
      for(int j = 0; j < data_size; j++) {
        buckets[rank - 1].push_back(buf[j]);
      }
    }
    CALI_MARK_END(comm_large);
    CALI_MARK_END(comm);

    //sort bucket
    CALI_MARK_BEGIN(comp);
    CALI_MARK_BEGIN(comp_large);
    sort(buckets[rank - 1].begin(), buckets[rank - 1].end());
    CALI_MARK_END(comp_large);
    CALI_MARK_END(comp);

    //send bucket data back to master processor
    CALI_MARK_BEGIN(comm);
    CALI_MARK_BEGIN(comm_large);
    int bucket_size = buckets[rank - 1].size();
    MPI_Send(&bucket_size, 1, MPI_INT, 0, rank, MPI_COMM_WORLD);
    MPI_Send(&(buckets[rank - 1][0]), bucket_size, MPI_INT, 0, rank, MPI_COMM_WORLD);
    CALI_MARK_END(comm_large);
    CALI_MARK_END(comm);
  }
  CALI_MARK_END("main");

  if(rank == MASTER) {
    adiak::init(NULL);
    adiak::launchdate();
    adiak::libraries();
    adiak::cmdline();
    adiak::clustername();
    adiak::value("algorithm", "sample"); // The name of the algorithm you are using (e.g., "merge", "bitonic")
    adiak::value("programming_model", "mpi"); // e.g. "mpi"
    adiak::value("data_type", "int"); // The datatype of input elements (e.g., double, int, float)
    adiak::value("size_of_data_type", sizeof(int)); // sizeof(datatype) of input elements in bytes (e.g., 1, 2, 4)
    adiak::value("input_size", input_size); // The number of elements in input dataset (1000)
    adiak::value("input_type", "Random"); // For sorting, this would be choices: ("Sorted", "ReverseSorted", "Random", "1_perc_perturbed")
    adiak::value("num_procs", n_procs); // The number of processors (MPI ranks)
    adiak::value("scalability", "weak"); // The scalability of your algorithm. choices: ("strong", "weak")
    adiak::value("group_num", 3); // The number of your group (integer, e.g., 1, 10)
    adiak::value("implementation_source", "handwritten"); // Where you got the source code of your algorithm. choices: ("online", "ai", "handwritten").
  }

  // Can print to test
  if (rank == MASTER) {
    CALI_MARK_BEGIN(correctness_check);
    for(int i = 1; i < result.size(); i++) {
      if(result[i] < result[i - 1]) {
        cout << "Sorting incorrect" << result[i - 1] << ", " << result[i] << endl;
        MPI_Abort(MPI_COMM_WORLD, 1);
      }
      //cout << result[i - 1] << ", ";
    }
    //cout << endl;
    cout << "Sorting correct" << endl;
    cout << "Result size: " << result.size() << endl;
    CALI_MARK_END(correctness_check);
  }
  // Flush Caliper output before finalizing MPI
  mgr.stop();
  mgr.flush();

  MPI_Finalize();
  return 0;

}