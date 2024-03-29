#pragma once
#include "config.h"
#include "extent_object_stripe.h"
#include <cstdio>
#include <set>
// get_extents(stripe id) not used anywhere not implemented
// member variable ext_size_to_stripes not used anywhere not implemented
// getters not implemented since variables are public
class StripeManager {
public:
  std::set<stripe_ptr> *stripes;
  int num_data_exts_per_locality;
  float num_local_parities;
  float num_global_parities;
  int num_localities_in_stripe;
  int num_exts_per_stripe;
  int num_data_exts_per_stripe;
  float coding_overhead;
  int max_id;

  StripeManager(int num_data_extents, float num_local_parities,
                float num_global_parities, int num_localities_in_stripe,
                float coding_overhead = -1)
      : stripes(new std::set<stripe_ptr>()),
        num_data_exts_per_locality(num_data_extents),
        num_local_parities(num_local_parities),
        num_global_parities(num_global_parities),
        num_localities_in_stripe(num_localities_in_stripe), max_id(1) {

    num_exts_per_stripe =
        num_data_extents * num_localities_in_stripe +
        num_local_parities + num_global_parities;
    num_data_exts_per_stripe =
        num_data_extents * num_localities_in_stripe;
    if (coding_overhead == 0) {
      this->coding_overhead = (num_global_parities + num_local_parities +
                               num_data_exts_per_stripe) /
                              num_data_exts_per_stripe;
    } else {
      this->coding_overhead = coding_overhead;
    }
  }

  double get_data_dc_size() {
    long dc_size = 0;
    for (auto stripe : *stripes) {
      dc_size += stripe->ext_size * num_data_exts_per_stripe;
    }
    // fprintf(stderr, "dc size: %f %d\n", configtime, dc_size);
    return dc_size;
  }

  double get_total_dc_size() {
    // fprintf(stderr, "dc total size: %f %f coding overhead: %f\n", configtime,
    //         get_data_dc_size() * coding_overhead, coding_overhead);
    return get_data_dc_size() * coding_overhead;
  }

  int get_num_stripes() { return stripes->size(); }

  stripe_ptr create_new_stripe(int ext_size) {
    // not translated if block since ext_size local variable cant be found
    // or assigned anywhere else in stripe manager object!! if (ext_size is
    // None):
    //  ext_size = self.ext_size
    stripe_ptr stripe = make_shared<Stripe>(max_id++, num_data_exts_per_locality,
                                num_localities_in_stripe, ext_size, 15);
    stripes->insert(stripe);
    return stripe;
  }

  void delete_stripe(stripe_ptr stripe) {
    stripes->erase(stripe);
  }
};
