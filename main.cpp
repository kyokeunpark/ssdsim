#include <cstdlib>
#include <iostream>

#include "configs.h"
#include "object_packer.h"
#include "samplers.h"
#include "stripers.h"
using extent_lst = std::vector<int>;

/*
 * TODO: Run the simulator and write out the results to the csv file.
 */
void run_simulator() {
}

/*
 * Since we can't just evaluate function name like Python, a long switch
 * statement to parse the input string is necessary.
 */
DataCenter (*parse_config(string confname))(const unsigned long, const float,
                                         const float, const int, const short,
                                         const short, shared_ptr<SimpleSampler>,
                                         const short, const float, const int)
{
  // Baseline
  if (confname == "stripe_level_with_no_exts_config")
    return stripe_level_with_no_exts_config;
  // Cross-extent erasure coding
  else if (confname == "stripe_level_with_extents_separate_pools_config")
    return stripe_level_with_extents_separate_pools_config;
  else if (confname == "stripe_level_with_extents_separate_pools_efficient_config")
    return stripe_level_with_extents_separate_pools_efficient_config;
  // Placement strategies
  else if (confname == "age_based_config")
    return age_based_config;
  else if (confname == "generational_config")
    return generational_config;
  else if (confname == "size_based_stripe_level_no_exts_baseline_config")
    return size_based_stripe_level_no_exts_baseline_config;
  else if (confname == "size_based_stripe_level_no_exts_larger_whole_obj_config")
    return size_based_stripe_level_no_exts_larger_whole_obj_config;
  else if (confname == "size_based_stripe_level_no_exts_smaller_obj_config")
    return size_based_stripe_level_no_exts_smaller_obj_config;
  else if (confname == "size_based_stripe_level_no_exts_dynamic_strategy_config")
    return size_based_stripe_level_no_exts_dynamic_strategy_config;
  else if (confname == "size_based_whole_obj_config")
    return size_based_whole_obj_config;
  // Predictive strategies TODO
  // Randomization
  else if (confname == "age_based_config_no_exts")
    return age_based_config_no_exts;
  else if (confname == "age_based_rand_config_no_exts")
    return age_based_rand_config_no_exts;
  else if (confname == "randomized_objs_no_exts_config")
    return randomized_objs_no_exts_config;
  else if (confname == "randomized_ext_placement_joined_pools_config")
    return randomized_ext_placement_joined_pools_config;
  else if (confname == "no_exts_mix_objs_config")
    return no_exts_mix_objs_config;
  return nullptr;
}

int main(int argc, char *argv[]) {
  int ext_size;
  short threshold;

  auto config = randomized_ext_placement_joined_pools_config;

  if (argc == 4) {
    config = parse_config(argv[1]);
    ext_size = atol(argv[2]);
    threshold = atol(argv[3]);

    std::cout << argv[1] << std::endl;
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
