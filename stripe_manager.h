#pragma once
#include "config.h"
#include "extent_object_stripe.h"
#include <cstdio>
// get_extents(stripe id) not used anywhere not implemented
// member variable ext_size_to_stripes not used anywhere not implemented
// getters not implemented since variables are public
class StripeManager {
public:
  list<Stripe *> *stripes;
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
                float coding_overhead = 0)
      : stripes(new list<Stripe *>()),
        num_data_exts_per_locality(num_data_extents),
        num_local_parities(num_local_parities),
        num_global_parities(num_global_parities),
        num_localities_in_stripe(num_localities_in_stripe), max_id(1) {

    num_exts_per_stripe =
        num_data_exts_per_locality * num_localities_in_stripe +
        num_local_parities + num_global_parities;
    num_data_exts_per_stripe =
        num_data_exts_per_locality * num_localities_in_stripe;
    if (coding_overhead == 0.0) {
      this->coding_overhead = (num_global_parities + num_local_parities +
                               num_data_exts_per_stripe) /
                              num_data_exts_per_stripe;
    } else {
      this->coding_overhead = coding_overhead;
    }
  }

  double get_data_dc_size() {
    int dc_size = 0;
    for (Stripe *stripe : *stripes) {
      dc_size += stripe->ext_size * num_data_exts_per_stripe;
    }
    fprintf(stderr, "dc size: %f %d\n", configtime, dc_size);
    return dc_size;
  }

  double get_total_dc_size() {
    fprintf(stderr, "dc total size: %f %f coding overhead: %f\n", configtime,
            get_data_dc_size() * coding_overhead, coding_overhead);
    return get_data_dc_size() * coding_overhead;
  }

  int get_num_stripes() { return stripes->size(); }

  Stripe *create_new_stripe(int ext_size) {
    // not translated if block since ext_size local variable cant be found
    // or assigned anywhere else in stripe manager object!! if (ext_size is
    // None):
    //  ext_size = self.ext_size
    Stripe *stripe = new Stripe(max_id++, num_data_exts_per_locality,
                                num_localities_in_stripe, ext_size, 15);
    stripes->push_back(stripe);
    return stripe;
  }

  void delete_stripe(Stripe *stripe) { stripes->remove(stripe); }
};
