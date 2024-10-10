#include <stdio.h>
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

using namespace std;

int main(int argc, char** argv) {
  if(argc != 4) {
    cout << "Usage: incorrect number of arguments" << endl;
    cout << "Please provide as follows: <number of workers> <input size> <oversampling factor>" << endl;
    exit(1);
  }

  int num_workers = atoi(argv[1]);
  int input_size = atoi(argv[2]);
  int oversampling_factor = atoi(argv[3]);

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
    if(i + step_size >= sample.size()) {
      pivots.push_back(sample.back());
    }
  }

  cout << "Bucket Boundaries" << endl;
  for(int i = 0; i < pivots.size(); i++) {
    cout << pivots[i] << ", ";
  }
  cout << endl;

  //send bucket boundaries to worker processes
  

}