#pragma once
#include "config.h"
#include "extent_object_stripe.h"
#include "lock.h"
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
  shared_ptr<mutex> mtx = nullptr;

  StripeManager(int num_data_extents, float num_local_parities,
                float num_global_parities, int num_localities_in_stripe,
                float coding_overhead = -1, bool is_threaded = false)
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

    if (is_threaded)
      mtx = make_shared<mutex>();
  }

  double get_data_dc_size() {
    long dc_size = 0;
    lock(mtx);
    for (auto stripe : *stripes) {
      dc_size += stripe->ext_size * num_data_exts_per_stripe;
    }
    unlock(mtx);
    return dc_size;
  }

  double get_total_dc_size() {
    return get_data_dc_size() * coding_overhead;
  }

  int get_num_stripes() {
    lock(mtx);
    return stripes->size();
    unlock(mtx);
  }

  stripe_ptr create_new_stripe(int ext_size) {
    // not translated if block since ext_size local variable cant be found
    // or assigned anywhere else in stripe manager object!! if (ext_size is
    // None):
    //  ext_size = self.ext_size
    lock(mtx);
    stripe_ptr stripe = make_shared<Stripe>(max_id++, num_data_exts_per_locality,
                                num_localities_in_stripe, ext_size, 15);
    stripes->insert(stripe);
    unlock(mtx);
    return stripe;
  }

  void delete_stripe(stripe_ptr stripe) {
    lock(mtx);
    stripes->erase(stripe);
    unlock(mtx);
  }
};
