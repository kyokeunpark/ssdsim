#include <cstdlib>
#include <iostream>

#include "configs.h"
#include "object_packer.h"
#include "samplers.h"
#include "stripers.h"
using extent_lst = std::vector<int>;

int main(int argc, char *argv[]) {
  int ext_size;
  short threshold;

  if (argc == 4) {
    ext_size = atol(argv[2]);
    threshold = atol(argv[3]);

    std::cout << ext_size << ", " << threshold << std::endl;
  } else {
    ext_size = 3 * 1024;
    threshold = 10;
  }

  const unsigned short ave_obj_size = 35000;
  const unsigned long data_center_size = 3500000 * (unsigned long)ave_obj_size;
  const float striping_cycle = 1.0 / 12.0;
  const float deletion_cycle = striping_cycle;
  const float simul_time = 365.0;
  const int num_objs = 1000000;

  // Flag to record information about ext size distributions - small object
  // extents, large obj exts, etc
  const bool record_ext_types = false;

  const int total_objs = num_objs / (365 / simul_time);

  const short num_stripes_per_cycle = 100;
  const short num_iterations = 1;
  SimpleSampler sampler = DeterministicDistributionSampler(simul_time);
  const short secondary_threshold = threshold;

  extent_lst ext_sizes = {ext_size};

  std::cout << threshold << ", " << secondary_threshold << std::endl;

  return 0;
}
