#ifndef __GC_STRATEGIES_H_
#define __GC_STRATEGIES_H_
#include "config.h"
#include "extent_manager.h"
#include "extent_object_stripe.h"
#include "stripers.h"
#include "striping_process_coordinator.h"
#include <algorithm>
#include <cassert>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

typedef unordered_map<string, float> ext_type_cost_map;
typedef unordered_map<string, short> obj_ext_type_map;
typedef unordered_map<string, short> gc_ext_type_num_map;
typedef unordered_map<string, int> space_ext_type_map;
using std::set;

struct gc_handler_ret {
  long double reclaimed_space = 0, total_user_reads = 0, total_user_writes = 0,
        total_global_parity_reads = 0, total_global_parity_writes = 0,
        total_local_parity_reads = 0, total_local_parity_writes = 0,
        total_obsolete_data_reads = 0, total_absent_data_reads = 0,
        total_valid_obj_transfers = 0,
        total_storage_node_to_parity_calculator = 0,
        total_num_exts_replaced = 0;
  space_ext_type_map total_reclaimed_space_by_ext_type = space_ext_type_map();
  ;
};
struct stripe_gc_ret {
  long double temp_space = 0, user_reads = 0, user_writes = 0,
        global_parity_reads = 0, global_parity_writes = 0,
        local_parity_reads = 0, local_parity_writes = 0,
        obsolete_data_reads = 0, absent_data_reads = 0, valid_obj_transfers = 0,
        storage_node_to_parity_calculator = 0, num_exts_replaced = 0;
  space_ext_type_map reclaimed_space_by_ext_types = space_ext_type_map();
};
inline bool stripe_cmpr(Stripe *s1, Stripe *s2) { return s1->id < s2->id; }
class GarbageCollectionStrategy {
protected:
  short primary_threshold, secondary_threshold;
  shared_ptr<ExtentManager> extent_manager;
  shared_ptr<StriperWithEC> gc_striper;
  shared_ptr<StripingProcessCoordinator> striping_process_coordinator;
  short num_gc_cycles, num_exts_gced, num_localities_in_gc;
  ext_type_cost_map ext_types_to_cost;
  obj_ext_type_map valid_objs_by_ext_type;
  gc_ext_type_num_map gc_ed_exts_by_type;

public:
  GarbageCollectionStrategy(short p_thresh, short s_thresh,
                            shared_ptr<ExtentManager> eman,
                            shared_ptr<StripingProcessCoordinator> s_p_c,
                            shared_ptr<StriperWithEC> striper)
      : primary_threshold(p_thresh), secondary_threshold(s_thresh),
        extent_manager(eman), striping_process_coordinator(s_p_c),
        gc_striper(striper) {
    this->num_gc_cycles = 0;

    // Need to divide by number of gc cycles to get the avg number of
    // exts gced per cycle and the number of localities per cycle
    this->num_exts_gced = 0;
    this->num_localities_in_gc = 0;
  }

  void add_num_gc_cycles(const int &num) { this->num_gc_cycles += num; }

  void add_num_exts_gced(const int &num) { this->num_exts_gced += num; }

  void add_localities_in_gc(const int &num) {
    this->num_localities_in_gc += num;
  }

  gc_ext_type_num_map get_gc_ed_exts_by_type() {
    return this->gc_ed_exts_by_type;
  }
  obj_ext_type_map get_valid_objs_by_ext_type() {
    return this->valid_objs_by_ext_type;
  }

  // Defines the strategy for gc on a single stripe
  virtual stripe_gc_ret stripe_gc(Stripe *stripe) = 0;
  // Mechanism for determining which stripes are ready for gc
  virtual gc_handler_ret gc_handler(set<Stripe *> &stripe_set) = 0;

  vector<Stripe *> sorted_stripe_set(set<Stripe *> &stripes) {
    std::vector<Stripe *> v(stripes.begin(), stripes.end());
    std::sort(v.begin(), v.end(), stripe_cmpr);
    return v;
  }
};

class StripeLevelNoExtsGCStrategy : public GarbageCollectionStrategy {
protected:
  shared_ptr<StripeManager> stripe_manager;

public:
  StripeLevelNoExtsGCStrategy(short p_thresh, short s_thresh,
                              shared_ptr<ExtentManager> eman,
                              shared_ptr<StripingProcessCoordinator> s_p_c,
                              shared_ptr<StriperWithEC> striper,
                              shared_ptr<StripeManager> s_m)
      : GarbageCollectionStrategy(p_thresh, s_thresh, eman, s_p_c, striper),
        stripe_manager(s_m) {}

  stripe_gc_ret stripe_gc(Stripe *stripe) override {
    stripe_gc_ret ret;
    vector<int> exts_per_locality;
    vector<int> obs_data_per_locality;
    vector<int> valid_objs_per_locality;
    for (int i = 0; i < stripe->num_localities; ++i) {
      exts_per_locality.push_back(0);
      obs_data_per_locality.push_back(0);
      valid_objs_per_locality.push_back(0);
    }
    add_num_gc_cycles(1);
    set<ExtentObject *> objs;
    set<int> local_parities;
    std::list<Extent *> extent_list = stripe->extents;
    space_ext_type_map reclaimed_space_by_ext_types;
    for (Extent *ext : extent_list) {
      assert(ext->get_obsolete_percentage() <= 100);
      ret.temp_space += ext->obsolete_space;
      short valid_objs = ext->ext_size - ext->obsolete_space;
      if (ext_types_to_cost.find(ext->type) != ext_types_to_cost.end()) {
        ext_types_to_cost[ext->type] += valid_objs * 2;
        valid_objs_by_ext_type[ext->type] += valid_objs;
        gc_ed_exts_by_type[ext->type] += 1;
      } else {
        ext_types_to_cost[ext->type] = valid_objs * 2;
        valid_objs_by_ext_type[ext->type] = valid_objs;
        gc_ed_exts_by_type[ext->type] = 1;
      }
      if (reclaimed_space_by_ext_types.find(ext->type) !=
          reclaimed_space_by_ext_types.end()) {
        reclaimed_space_by_ext_types[ext->type] += ext->obsolete_space;
      } else {
        reclaimed_space_by_ext_types[ext->type] = ext->obsolete_space;
      }
      ret.valid_obj_transfers += valid_objs;
      valid_objs_per_locality[ext->locality] += valid_objs;
      striping_process_coordinator->gc_extent(ext, objs);
      exts_per_locality[ext->locality] += 1;
      obs_data_per_locality[ext->locality] += ext->obsolete_space;
      if (local_parities.find(ext->locality) != local_parities.end()) {
        local_parities.insert(ext->locality);
      }
      ret.num_exts_replaced += 1;
      stripe->del_extent(ext);
      extent_manager->delete_extent(ext);
    }
    add_num_exts_gced(ret.num_exts_replaced);
    add_localities_in_gc(local_parities.size());
    if (ret.temp_space == 0) {
      return ret;
    }
    stripe_manager->delete_stripe(stripe);
    str_costs generate_res =
        striping_process_coordinator->generate_gc_stripes();
    int num_stripes = generate_res.stripes;
    int reads = generate_res.reads;
    int writes = generate_res.writes;
    if (num_stripes < 1) {
      str_costs stripe_res = striping_process_coordinator->get_stripe();
      num_stripes = stripe_res.stripes;
      ret.user_reads = stripe_res.reads;
      ret.user_writes = stripe_res.writes;
    } else {
      ret.user_reads = reads;
      ret.user_writes = writes;
    }

    double parity_writes = ret.user_writes - ret.user_reads;
    ret.global_parity_writes = parity_writes / 2;
    ret.local_parity_writes = parity_writes / 2;
    ret.user_writes = ret.user_reads;
    return ret;
  }

  gc_handler_ret gc_handler(set<Stripe *> &stripe_set) override {
    struct gc_handler_ret ret;
    set<Stripe *> deleted;
    vector<Stripe *> stripe_lst = sorted_stripe_set(stripe_set);
    for (auto stripe : stripe_lst) {
      // what should obsolete's type be? pr what type should get
      // percentage return?
      double obsolete = stripe->get_obsolete_percentage();
      if (obsolete >= primary_threshold && stripe != nullptr) {
        // fprintf(stderr, "%f %d", configtime, stripe->id);
        stripe_gc_ret stripe_gc_res = stripe_gc(stripe);
        for (auto &kv : stripe_gc_res.reclaimed_space_by_ext_types) {
          string key = kv.first;
          if (ret.total_reclaimed_space_by_ext_type.find(key) !=
              ret.total_reclaimed_space_by_ext_type.end()) {
            ret.total_reclaimed_space_by_ext_type[key] +=
                stripe_gc_res.reclaimed_space_by_ext_types[key];
          } else {
            ret.total_reclaimed_space_by_ext_type[key] =
                stripe_gc_res.reclaimed_space_by_ext_types[key];
          }
        }
        if (stripe_gc_res.temp_space > 0) {
          deleted.insert(stripe);
        }
        ret.reclaimed_space += stripe_gc_res.temp_space;
        ret.total_user_reads += stripe_gc_res.user_reads;
        ret.total_user_writes += stripe_gc_res.user_writes;
        ret.total_valid_obj_transfers += stripe_gc_res.valid_obj_transfers;
        ret.total_storage_node_to_parity_calculator +=
            stripe_gc_res.storage_node_to_parity_calculator;
        ret.total_global_parity_reads += stripe_gc_res.global_parity_reads;
        ret.total_global_parity_writes += stripe_gc_res.global_parity_writes;
        ret.total_local_parity_reads += stripe_gc_res.local_parity_reads;
        ret.total_local_parity_writes += stripe_gc_res.local_parity_writes;
        ret.total_obsolete_data_reads += stripe_gc_res.obsolete_data_reads;
        ret.total_absent_data_reads += stripe_gc_res.absent_data_reads;
        ret.total_num_exts_replaced += stripe_gc_res.num_exts_replaced;
      }
    }
    for (Stripe *d : deleted) {
      stripe_set.erase(d);
    }
    return ret;
  }
};

class StripeLevelWithExtsGCStrategy : public GarbageCollectionStrategy {
public:
  using GarbageCollectionStrategy::GarbageCollectionStrategy;

  bool filter_ext(Extent *ext) {
    return ext->get_obsolete_percentage() >= secondary_threshold;
  }

  struct gc_ext_res {
    short user_reads;
    short user_writes;
    gc_ext_res(short r, short w) : user_reads(r), user_writes(w) {}
  };
  gc_ext_res gc_ext(Extent *ext, Stripe *stripe) {
    std::any key = extent_manager->get_key(ext);
    Extent *temp_ext =
        striping_process_coordinator->get_gc_extent(std::any_cast<int>(key));
    if (temp_ext == nullptr) {
      temp_ext =
          striping_process_coordinator->get_extent(std::any_cast<int>(key));
    }

    stripe->add_extent(temp_ext);
    short user_writes = ext->ext_size;
    short user_reads = ext->ext_size;

    return gc_ext_res(user_writes, user_reads);
  }

  stripe_gc_ret stripe_gc(Stripe *stripe) override {
    stripe_gc_ret ret;
    vector<int> exts_per_locality;
    vector<int> obs_data_per_locality;
    vector<int> valid_objs_per_locality;
    for (int i = 0; i < stripe->num_localities; ++i) {
      exts_per_locality.push_back(0);
      obs_data_per_locality.push_back(0);
      valid_objs_per_locality.push_back(0);
    }
    add_num_gc_cycles(1);
    set<ExtentObject *> objs;
    set<int> local_parities;
    list<Extent *> extent_list = stripe->extents;
    int ext_size = 0;
    space_ext_type_map reclaimed_space_by_ext_types;
    for (Extent *ext : extent_list) {
      if (filter_ext(ext)) {
        assert(ext->get_obsolete_percentage() <= 100);
        ret.temp_space += ext->obsolete_space;
        short valid_objs = ext->ext_size - ext->obsolete_space;
        if (ext_types_to_cost.find(ext->type) != ext_types_to_cost.end()) {
          ext_types_to_cost[ext->type] += valid_objs * 2;
          valid_objs_by_ext_type[ext->type] += valid_objs;
          gc_ed_exts_by_type[ext->type] += 1;
        } else {
          ext_types_to_cost[ext->type] = valid_objs * 2;
          valid_objs_by_ext_type[ext->type] = valid_objs;
          gc_ed_exts_by_type[ext->type] = 1;
        }
        if (reclaimed_space_by_ext_types.find(ext->type) !=
            reclaimed_space_by_ext_types.end()) {
          reclaimed_space_by_ext_types[ext->type] += ext->obsolete_space;
        } else {
          reclaimed_space_by_ext_types[ext->type] = ext->obsolete_space;
        }
        ret.valid_obj_transfers += valid_objs;
        valid_objs_per_locality[ext->locality] += valid_objs;
        striping_process_coordinator->gc_extent(ext, objs);
        exts_per_locality[ext->locality] += 1;
        obs_data_per_locality[ext->locality] += ext->obsolete_space;
        if (local_parities.find(ext->locality) != local_parities.end()) {
          local_parities.insert(ext->locality);
        }
        ret.num_exts_replaced += 1;
        stripe->del_extent(ext);
        extent_manager->delete_extent(ext);
        gc_ext_res gc_ext_data = gc_ext(ext, stripe);
        ret.user_writes += gc_ext_data.user_writes;
        ret.user_reads += gc_ext_data.user_reads;
      }
    }
    add_num_exts_gced(ret.num_exts_replaced);
    add_localities_in_gc(local_parities.size());

    if (ret.temp_space > 0) {
      repl_costs costs = gc_striper->cost_to_replace_extents(
          ext_size, exts_per_locality, obs_data_per_locality,
          valid_objs_per_locality);
      ret.storage_node_to_parity_calculator += costs.valid_obj_reads;
    }
    return ret;
  }

  gc_handler_ret gc_handler(set<Stripe *> &stripe_set) override {
    struct gc_handler_ret ret;
    set<Stripe *> deleted;
    vector<Stripe *> stripe_lst = sorted_stripe_set(stripe_set);
    for (auto stripe : stripe_lst) {
      // what should obsolete's type be? pr what type should get
      // percentage return?
      double obsolete = stripe->get_obsolete_percentage();
      if (obsolete >= primary_threshold && stripe != nullptr) {
        // fprintf(stderr,"%f %d", configtime, stripe->id);
        stripe_gc_ret stripe_gc_res = stripe_gc(stripe);
        for (auto &kv : stripe_gc_res.reclaimed_space_by_ext_types) {
          string key = kv.first;
          if (ret.total_reclaimed_space_by_ext_type.find(key) !=
              ret.total_reclaimed_space_by_ext_type.end()) {
            ret.total_reclaimed_space_by_ext_type[key] +=
                stripe_gc_res.reclaimed_space_by_ext_types[key];
          } else {
            ret.total_reclaimed_space_by_ext_type[key] =
                stripe_gc_res.reclaimed_space_by_ext_types[key];
          }
        }
        if (stripe_gc_res.temp_space > 0) {
          deleted.insert(stripe);
        }
        ret.reclaimed_space += stripe_gc_res.temp_space;
        ret.total_user_reads += stripe_gc_res.user_reads;
        ret.total_user_writes += stripe_gc_res.user_writes;
        ret.total_valid_obj_transfers += stripe_gc_res.valid_obj_transfers;
        ret.total_storage_node_to_parity_calculator +=
            stripe_gc_res.storage_node_to_parity_calculator;
        ret.total_global_parity_reads += stripe_gc_res.global_parity_reads;
        ret.total_global_parity_writes += stripe_gc_res.global_parity_writes;
        ret.total_local_parity_reads += stripe_gc_res.local_parity_reads;
        ret.total_local_parity_writes += stripe_gc_res.local_parity_writes;
        ret.total_obsolete_data_reads += stripe_gc_res.obsolete_data_reads;
        ret.total_absent_data_reads += stripe_gc_res.absent_data_reads;
        ret.total_num_exts_replaced += stripe_gc_res.num_exts_replaced;
      }
    }
    for (Stripe *d : deleted) {
      stripe_set.erase(d);
    }
    return ret;
  };

  class MixObjStripeLevelStrategy : public GarbageCollectionStrategy {
  public:
    using GarbageCollectionStrategy::GarbageCollectionStrategy;

    stripe_gc_ret stripe_gc(Stripe *stripe) override {
      stripe_gc_ret ret;
      vector<int> exts_per_locality;
      vector<int> obs_data_per_locality;
      vector<int> valid_objs_per_locality;
      for (int i = 0; i < stripe->num_localities; ++i) {
        exts_per_locality.push_back(0);
        obs_data_per_locality.push_back(0);
        valid_objs_per_locality.push_back(0);
      }
      add_num_gc_cycles(1);
      set<ExtentObject *> objs;
      list<Extent *> extent_list = stripe->extents;
      space_ext_type_map reclaimed_space_by_ext_types;
      set<int> local_parities;
      for (Extent *ext : extent_list) {
        assert(ext->get_obsolete_percentage() <= 100);
        ret.temp_space += ext->obsolete_space;
        short valid_objs = ext->ext_size - ext->obsolete_space;
        if (ext_types_to_cost.find(ext->type) != ext_types_to_cost.end()) {
          ext_types_to_cost[ext->type] += valid_objs * 2;
          valid_objs_by_ext_type[ext->type] += valid_objs;
          gc_ed_exts_by_type[ext->type] += 1;
        } else {
          ext_types_to_cost[ext->type] = valid_objs * 2;
          valid_objs_by_ext_type[ext->type] = valid_objs;
          gc_ed_exts_by_type[ext->type] = 1;
        }
        if (reclaimed_space_by_ext_types.find(ext->type) !=
            reclaimed_space_by_ext_types.end()) {
          reclaimed_space_by_ext_types[ext->type] += ext->obsolete_space;
        } else {
          reclaimed_space_by_ext_types[ext->type] = ext->obsolete_space;
        }
        ret.valid_obj_transfers += valid_objs;
        valid_objs_per_locality[ext->locality] += valid_objs;
        striping_process_coordinator->gc_extent(ext, objs);
        exts_per_locality[ext->locality] += 1;
        obs_data_per_locality[ext->locality] += ext->obsolete_space;
        if (local_parities.find(ext->locality) != local_parities.end()) {
          local_parities.insert(ext->locality);
        }
        ret.num_exts_replaced += 1;
        stripe->del_extent(ext);
        extent_manager->delete_extent(ext);
      }
      add_num_exts_gced(ret.num_exts_replaced);
      add_localities_in_gc(local_parities.size());
      if (ret.temp_space > 0) {
        // return stripe_manager->delete_stripe(stripe);
      }
      return ret;
    }

    gc_handler_ret gc_handler(set<Stripe *> &stripe_set) override {
      struct gc_handler_ret ret;
      set<Stripe *> deleted;
      vector<Stripe *> stripe_lst = sorted_stripe_set(stripe_set);
      for (auto stripe : stripe_lst) {
        double obsolete = stripe->get_obsolete_percentage();
        if (obsolete >= primary_threshold && stripe != nullptr) {
          // fprintf(stderr,"%f %d", configtime, stripe->id);
          stripe_gc_ret stripe_gc_res = stripe_gc(stripe);
          for (auto &kv : stripe_gc_res.reclaimed_space_by_ext_types) {
            string key = kv.first;
            if (ret.total_reclaimed_space_by_ext_type.find(key) !=
                ret.total_reclaimed_space_by_ext_type.end()) {
              ret.total_reclaimed_space_by_ext_type[key] +=
                  stripe_gc_res.reclaimed_space_by_ext_types[key];
            } else {
              ret.total_reclaimed_space_by_ext_type[key] =
                  stripe_gc_res.reclaimed_space_by_ext_types[key];
            }
          }
          if (stripe_gc_res.temp_space > 0) {
            deleted.insert(stripe);
          }
          ret.reclaimed_space += stripe_gc_res.temp_space;
          ret.total_user_reads += stripe_gc_res.user_reads;
          ret.total_user_writes += stripe_gc_res.user_writes;
          ret.total_valid_obj_transfers += stripe_gc_res.valid_obj_transfers;
          ret.total_storage_node_to_parity_calculator +=
              stripe_gc_res.storage_node_to_parity_calculator;
          ret.total_global_parity_reads += stripe_gc_res.global_parity_reads;
          ret.total_global_parity_writes += stripe_gc_res.global_parity_writes;
          ret.total_local_parity_reads += stripe_gc_res.local_parity_reads;
          ret.total_local_parity_writes += stripe_gc_res.local_parity_writes;
          ret.total_obsolete_data_reads += stripe_gc_res.obsolete_data_reads;
          ret.total_absent_data_reads += stripe_gc_res.absent_data_reads;
          ret.total_num_exts_replaced += stripe_gc_res.num_exts_replaced;
        }
      }
      for (Stripe *d : deleted) {
        stripe_set.erase(d);
      }
      striping_process_coordinator->generate_exts();
      striping_process_coordinator->generate_objs(ret.reclaimed_space);
      striping_process_coordinator->pack_exts(ret.total_num_exts_replaced);
      for (int i = 0; i < ret.total_num_exts_replaced; ++i) {
      }
      str_costs stripe_res = striping_process_coordinator->get_stripe();
      int num_stripes = stripe_res.stripes;
      int user_reads = stripe_res.reads;
      int user_writes = stripe_res.writes;
      int parity_writes = user_writes - user_reads;
      ret.total_global_parity_writes += parity_writes / 2;
      ret.total_local_parity_writes += parity_writes / 2;
      user_writes = user_reads;
      ret.total_user_writes += user_writes;
      ret.total_user_reads += user_reads;
      return ret;
    }
  };
};
#endif // __GC_STRATEGIES_H_
