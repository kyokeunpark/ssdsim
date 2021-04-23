#include <cstdlib>
#include <iostream>

#include "configs.h"
#include "data_center.h"
#include "object_packer.h"
#include "samplers.h"
#include "stripers.h"
#include <fstream>
#include <string>
using ext_lst = std::vector<int>;

/*
 * Since we can't just evaluate function name like Python, a long switch
 * statement to parse the input string is necessary.
 */


void print_to_file(const string confname, const string filename, int ext_size,
                   const short primary_threshold, const short secondary_threshold, sim_metric res){
      std::ofstream myFile(filename);
      vector<string> row_header = {"extent sizes",
                                "primary threshold",
                                "secondary threshold",
                                "write amplification",
                                "obsolete space",
                                "time weighted space",
                                "obsolete percentage",
                                "obsolete percentages",
                                "max obsolete percentage",
                                "GC data transfer/User data transfer",
                                "reclaimed space",
                                "parity writes",
                                "parity reads",
                                "stale obj reads",
                                "pool to parity calculator",
                                "parity calculator to storage node",
                                "storage node to parity calculator",
                                "non-stale data block reads",
                                "total bandwidth",
                                "gc bandwidth",
                                "user reads",
                                "user writes",
                                "number of objects",
                                "number of extents",
                                "number of stripes",
                                "dc size",
                                "leftovers",
                                "ave exts gced",
    };
    for(auto s : row_header)
    {
      myFile << s << ",";
    }
    myFile << endl;
    string obs_percentages_str = "[";
    for (auto op: res.obs_percentages)
    {
      obs_percentages_str += std::to_string(op) + ",";
    }
    obs_percentages_str += "]";
    myFile << ext_size << ","
            << primary_threshold << ","
            << secondary_threshold << ","
            << res.gc_amplification << ","
            << res.total_obsolete << ","
            << res.total_used_space << ","
            << res.total_obsolete * 1.0/res.total_used_space * 100 << ","
            << obs_percentages_str << ","
            << res.max_obs_perc << ","
            << res.gc_ratio << ","
            << res.total_reclaimed_space << ","
            << res.parity_writes << ","
            << res.parity_reads << ","
            << res.total_obsolete_data_reads << ","
            << res.total_pool_to_parity_calculator << ","
            << res.total_parity_calculator_to_storage_node << ","
            << res.total_storage_node_to_parity_calculator << ","
            << res.total_absent_data_reads << ","
            << res.total_bandwidth << ","
            << res.total_gc_bandwidth << ","
            << res.total_user_data_reads << ","
            << res.total_user_data_writes << ","
            << res.num_objs << ","
            << res.num_exts << ","
            << res.num_stripes << ","
            << res.dc_size << ","
            << res.total_leftovers << ","
            << res.ave_exts_gced << "," << endl;

  myFile.close();
}

DataCenter (*parse_config(string confname))(const unsigned long, const float,
                                            const float, const int, const short,
                                            const short,
                                            shared_ptr<SimpleSampler>,
                                            const short, const float,
                                            const int) {
  // Baseline
  if (confname == "stripe_level_with_no_exts_config")
    return stripe_level_with_no_exts_config;
  else if (confname == "no_exts_mix_objs_config")
    return no_exts_mix_objs_config;
  // Cross-extent erasure coding
  else if (confname == "stripe_level_with_extents_separate_pools_config")
    return stripe_level_with_extents_separate_pools_config;
  else if (confname ==
           "stripe_level_with_extents_separate_pools_efficient_config")
    return stripe_level_with_extents_separate_pools_efficient_config;
  // Placement strategies
  else if (confname == "age_based_config")
    return age_based_config;
  else if (confname == "generational_config")
    return generational_config;
  else if (confname == "size_based_stripe_level_no_exts_baseline_config")
    return size_based_stripe_level_no_exts_baseline_config;
  else if (confname ==
           "size_based_stripe_level_no_exts_larger_whole_obj_config")
    return size_based_stripe_level_no_exts_larger_whole_obj_config;
  else if (confname == "size_based_stripe_level_no_exts_smaller_obj_config")
    return size_based_stripe_level_no_exts_smaller_obj_config;
  else if (confname ==
           "size_based_stripe_level_no_exts_dynamic_strategy_config")
    return size_based_stripe_level_no_exts_dynamic_strategy_config;
  else if (confname == "size_based_whole_obj_config")
    return size_based_whole_obj_config;
  else if (confname == "age_based_config_no_exts")
    return age_based_config_no_exts;
  else if (confname == "age_based_rand_config_no_exts")
    return age_based_rand_config_no_exts;
  else if (confname == "randomized_objs_no_exts_config")
    return randomized_objs_no_exts_config;
  else if (confname == "randomized_ext_placement_joined_pools_config")
    return randomized_ext_placement_joined_pools_config;
  else if (confname == "randomized_obj_placement_joined_pools_config")
    return randomized_obj_placement_joined_pools_config;
  else if (confname == "randomized_objs_no_exts_mix_objs_config")
    return randomized_objs_no_exts_mix_objs_config;
  else if (confname == "no_exts_mix_objs_config")
    return no_exts_mix_objs_config;
  return nullptr;
}



/*
 * TODO: Run the simulator and write out the results to the csv file.
 */
void run_simulator(const string confname, const int percent_correct, const ext_lst ext_sizes,
                   const short primary_threshold,
                   const short secondary_threshold,
                   const short num_stripes_per_cycle,
                   const float striping_cycle, const float deletion_cycle,
                   const unsigned long data_center_size, const float simul_time,
                   SimpleSampler &sampler, const int total_objs,
                   bool save_to_file = true, bool record_ext_types = true) {
  string file_basename = confname;
  int num_objs_per_cycle = total_objs / simul_time * striping_cycle;
  auto config = parse_config(confname);
  shared_ptr<SimpleSampler> samplerptr = make_shared<SimpleSampler>(sampler);

  if (!config && (confname != "mortal_immortal_no_exts_config")) {
    std::cerr << "Error: invalid config (" << confname
        << ") detected! Exiting..." << std::endl;
    exit(1);
  }

  for (auto ext_size : ext_sizes) {
    std::cout << "Extent " << ext_size << std::endl;
    DataCenter dc = confname == "mortal_immortal_no_exts_config" ? 
      mortal_immortal_no_exts_config(data_center_size, striping_cycle, simul_time, ext_size,
                     primary_threshold, secondary_threshold, samplerptr,
                     num_stripes_per_cycle, deletion_cycle, num_objs_per_cycle, percent_correct):
      config(data_center_size, striping_cycle, simul_time, ext_size,
                     primary_threshold, secondary_threshold, samplerptr,
                     num_stripes_per_cycle, deletion_cycle, num_objs_per_cycle);
    auto res = dc.run_simulation();
    if(save_to_file)
    {
      string filename;
      if(confname == "mortal_immortal_no_exts_config")
      {
        filename = string(confname) + "_" + std::to_string(percent_correct) + "_" + std::to_string(ext_size) + "-" + std::to_string(total_objs) + "_objs-"
        +std::to_string(primary_threshold)+"-"+std::to_string(secondary_threshold)+ "_" + std::string(sampler) + ".csv";
      }else{
        filename = string(confname) + "_" + std::to_string(ext_size) + "-" + std::to_string(total_objs) + "_objs-"
        +std::to_string(primary_threshold)+"-"+std::to_string(secondary_threshold)+ "_" + std::string(sampler) + ".csv";
      }
      print_to_file(confname, filename, ext_size, primary_threshold, secondary_threshold, res);
    }

  }
}

int main(int argc, char *argv[]) {
  int ext_size;
  short threshold;
  string config;

  if (argc == 4 || argc == 5) {
    config = argv[1];
    ext_size = atol(argv[2]);
    threshold = atol(argv[3]);

    std::cout << argv[1] << std::endl;
    std::cout << ext_size << ", " << threshold << std::endl;
  }else {
    // Baseline
    config = "stripe_level_with_no_exts_config";

    // config = "randomized_objs_no_exts_mix_objs_config";
    ext_size = 3 * 1024;
    threshold = 10;
  }

  const unsigned short ave_obj_size = 35000;
  const unsigned long data_center_size = 3500000 * (unsigned long)ave_obj_size;
  const float striping_cycle = 1.0 / 12.0;
  const float deletion_cycle = striping_cycle;
  const float simul_time = 1;
  const int num_objs = 1000000;
  const int percent_correct = argc == 5? atoi(argv[4]) : 100;
  // Flag to record information about ext size distributions - small object
  // extents, large obj exts, etc
  const bool record_ext_types = false;

  const int total_objs = num_objs / (365 / simul_time);

  const short num_stripes_per_cycle = 100;
  const short num_iterations = 1;
  SimpleSampler sampler = DeterministicDistributionSampler(simul_time);
  const short secondary_threshold = threshold;

  ext_lst ext_sizes = {ext_size};

  std::cout << threshold << ", " << secondary_threshold << std::endl;
  run_simulator(config, percent_correct, ext_sizes, threshold, secondary_threshold,
                num_stripes_per_cycle, striping_cycle, deletion_cycle,
                data_center_size, simul_time, sampler, total_objs);
  
  return 0;
}
