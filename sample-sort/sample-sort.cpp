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
  if(argc != 4) {
    cout << "Usage: incorrect number of arguments" << endl;
    cout << "Please provide as follows: <number of workers> <input size> <oversampling factor>" << endl;
    exit(1);
  }

  int num_workers = atoi(argv[1]) - 1;
  int input_size = atoi(argv[2]);
  int oversampling_factor = atoi(argv[3]);

  MPI_Init(&argc, &argv);

  int rank, n_procs, n_workers;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &n_procs);

  //send bucket boundaries to worker processes
  if(rank == MASTER) {
    //create input
    vector<int> input;
    for(int i = 0; i < input_size; i++) {
      input.push_back(rand() % 100);
    }

    //sample p*k values
    //assumes p*k is less than size of input
    vector<int> sample;
    for(int i = 0; i < num_workers * oversampling_factor; i++) {
      sample.push_back(input[i]);
    }

    //sort sampled values
    sort(sample.begin(), sample.end());

    cout << "Sample size: " << sample.size() << endl;
    for(int i = 0; i < sample.size(); i++) {
      cout << sample[i] << ", ";
    }
    cout << endl;

    //create buckets
    vector<int> pivots;
    int step_size = sample.size() / num_workers;
    for(int i = 0; i < sample.size(); i += step_size) {
      pivots.push_back(sample[i]);
    }
    pivots.push_back(sample[sample.size() - 1]);

    MPI_Bcast(&(pivots[0]), pivots.size(), MPI_INT, 0, MPI_COMM_WORLD);

    //send workloads to each processor
    int workload_size = input_size / num_workers;
    for(int i = 0; i < num_workers; i++) {
      MPI_Send(input[i * workload_size], workload_size, MPI_INT, i + 1, 0, MPI_COMM_WORLD);
    }

    //receive bucket data from each processor
    vector<int> result;
    for(int i = 1; i <= num_workers; i++) {
      int data_size;
      MPI_Recv(&data_size, 1, MPI_INT, i, i, MPI_COMM_WORLD);
      int* buf = new int[data_size];
      MPI_Recv(buf, data_size, MPI_INT, i, i, MPI_COMM_WORLD);
      for(int j = 0; j < data_size; j++) {
        result.push_back(buf[j]);
      }
    }

  } else {
    //worker processor
    //receive bucket boundaries
    int* pivots = new int[oversampling_factor];
    MPI_Recv(pivots, oversampling_factor, MPI_INT, 0, 0, MPI_COMM_WORLD);
    
    //receive workload
    int workload_size = input_size / num_workers;
    int* workload = new int[workload_size];
    MPI_Recv(workload, workload_size, MPI_INT, 0, 0, MPI_COMM_WORLD);

    //sort data into buckets
    vector<vector<int>> buckets(num_workers, vector<int>());
    for(int i = 0; i < workload_size; i++) {
      int num = workload[i];
      for(int j = 0; j < oversampling_factor - 1; j++) {
        int lower_bound = pivots[j];
        int upper_bound = pivots[j + 1];
        if(num >= lower_bound && num < upper_bound) {
          buckets[j].push_back(num);
          break;
        }
      }
    }

    //send bucket data to respective processor
    //traverse thru the buckets
    //for each bucket, if the bucket belongs to another processor
    //first send the bucket size to the given processor
    //then send the bucket's data to the given processor
    //use processor's rank as msg tag

    for(int i = 0; i < buckets.size(); i++) {
      if(i + 1 == rank) {
        continue;
      }
      int bucket_size = buckets[i].size();
      MPI_Send(&bucket_size, 1, MPI_INT, i + 1, rank, MPI_COMM_WORLD);
      MPI_Send(buckets[i][0], buckets[i].size(), MPI_INT, i + 1, rank, MPI_COMM_WORLD);
    }

    //receive data for our bucket
    for(int i = 1; i <= num_workers; i++) {
      if(i == rank) {
        continue;
      }
      int data_size;
      MPI_Recv(&data_size, 1, MPI_INT, i, i, MPI_COMM_WORLD);

      int* buf = new int[data_size];
      MPI_Recv(buf, data_size, MPI_INT, i, i, MPI_COMM_WORLD);

      //store received data
      for(int j = 0; j < data_size; j++) {
        buckets[rank - 1].push_back(buf[j]);
      }
      delete[] buf;
    }

    //sort bucket
    sort(buckets[rank - 1].begin(), buckets[rank - 1].end());

    //send bucket data back to master processor
    int bucket_size = buckets[rank - 1].size();
    MPI_Send(&bucket_size, 1, MPI_INT, 0, rank, MPI_COMM_WORLD);
    MPI_Send(buckets[rank - 1][0], bucket_size, MPI_INT, 0, rank, MPI_COMM_WORLD);
    

  }

}