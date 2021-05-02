#pragma once
#include "extent_manager.h"
#include "extent_stack.h"
#include "stripe_manager.h"
#include "lock.h"
#include <array>
#include <memory>

using std::shared_ptr;
typedef struct stripe_costs {
  int stripes;
  int reads;
  int writes;

  stripe_costs &operator+=(const stripe_costs &rhs) {
    this->stripes += rhs.stripes;
    this->reads += rhs.reads;
    this->writes += rhs.writes;
    return *this;
  }
} str_costs;

typedef struct replacement_costs {
  double global_parity_reads;
  double global_parity_writes;
  int local_parity_reads;
  int local_parity_writes;
  int obsolete_data_reads;
  int valid_obj_reads;
  int absent_data_reads;
} repl_costs;

class AbstractStriper {

protected:
  shared_ptr<mutex> mtx = nullptr;
public:
  shared_ptr<StripeManager> stripe_manager;
  shared_ptr<ExtentManager> extent_manager;
  int num_times_alternatives;
  int num_times_default;
  AbstractStriper() {}
  AbstractStriper(shared_ptr<StripeManager> s_m, shared_ptr<ExtentManager> e_m,
                  bool is_threaded = false)
      : stripe_manager(s_m), extent_manager(e_m), num_times_alternatives(0),
        num_times_default(0) {
    if (is_threaded)
      mtx = make_shared<mutex>();
  }

  virtual str_costs create_stripes(shared_ptr<AbstractExtentStack> extent_stack,
                                   float simulation_time) {
    std::cerr << "virtual create_stripes should never happen";
    return str_costs();
  };
  virtual str_costs create_stripe(shared_ptr<AbstractExtentStack> extent_stack,
                                  float simulation_time) {
    std::cerr << "virtual create_stripes should never happen";
    return str_costs();
  };
  virtual repl_costs cost_to_replace_extents(int ext_size,
                                             int exts_per_locality,
                                             double obs_data_per_locality) {
    std::cerr << "virtual cost_to_replace_extents should never happen";
    return repl_costs();
  };
  virtual repl_costs
  cost_to_replace_extents(int ext_size, vector<int> exts_per_locality,
                          vector<int> obs_data_per_locality,
                          vector<int> valid_objs_per_locality) {
    std::cerr << "virtual cost_to_replace_extents should never happen";
    return repl_costs();
  };
  virtual double cost_to_write_data(int data) = 0;
  virtual int num_stripes_reqd() = 0;
};

class SimpleStriper : public AbstractStriper {
public:
  using AbstractStriper::AbstractStriper;
  list<string> ext_types;
  int num_stripes_reqd() override { return 1; }
  str_costs create_stripes(shared_ptr<AbstractExtentStack> extent_stack,
                           float simulation_time) override {
    
    int num_exts = stripe_manager->num_data_exts_per_stripe;
    int writes = 0;
    int reads = 0;
    int stripes = 0;
    list<ext_ptr> exts_to_stripe = extent_stack->pop_stripe_num_exts(num_exts);
    stripe_ptr current_stripe =
        stripe_manager->create_new_stripe(exts_to_stripe.front()->ext_size);
    for (int i = 0; i < num_exts; i++) {
      ext_ptr ext = exts_to_stripe.front();
      exts_to_stripe.pop_front();
      current_stripe->add_extent(ext);
      writes += ext->ext_size;
      reads += ext->ext_size;
    }
    stripes += 1;
    return {stripes, reads, writes};
  }

  repl_costs cost_to_replace_extents(int ext_size, int exts_per_locality,
                                     double obs_data_per_locality) override {
    repl_costs costs = {0, 0, 0, 0, 0, 0, 0};
    return costs;
  }
  double cost_to_write_data(int data) override { return data; }
};

class AbstractStriperDecorator : public AbstractStriper {
protected:
  shared_ptr<AbstractStriper> striper;

public:
  AbstractStriperDecorator(shared_ptr<AbstractStriper> s, bool is_threaded = false)
      : AbstractStriper(s->stripe_manager, s->extent_manager, is_threaded), striper(s) {}

  virtual str_costs create_stripe(shared_ptr<AbstractExtentStack> extent_stack,
                                  float simulation_time) = 0;

  virtual repl_costs
  cost_to_replace_extents(int ext_size, vector<int> exts_per_locality,
                          vector<int> obs_data_per_locality,
                          vector<int> valid_objs_per_locality) {
    return repl_costs();
  }

  virtual repl_costs cost_to_replace_extents(int ext_size,
                                             int exts_per_locality,
                                             double obs_data_per_locality) {
    return repl_costs();
  };
};

class ExtentStackStriper : public AbstractStriperDecorator {
public:
  using AbstractStriperDecorator::AbstractStriperDecorator;

  int num_stripes_reqd() override { return 0; }

  str_costs create_stripes(shared_ptr<AbstractExtentStack> extent_stack,
                           float simulation_time) override {
    lock(mtx);
    int num_exts = stripe_manager->num_data_exts_per_stripe;
    str_costs total = {0};
    while (extent_stack->num_stripes(num_exts))
      total += striper->create_stripes(extent_stack, simulation_time);
    unlock(mtx);
    return total;
  }

  str_costs create_stripe(shared_ptr<AbstractExtentStack> extent_stack,
                          float simulation_time) override {
    lock(mtx);
    int num_exts = stripe_manager->num_data_exts_per_stripe;
    str_costs total = {0};
    if (extent_stack->num_stripes(num_exts))
      total += striper->create_stripes(extent_stack, simulation_time);
    unlock(mtx);
    return total;
  }

  repl_costs cost_to_replace_extents(int ext_size, int exts_per_locality,
                                     double obs_data_per_locality) override {
    return striper->cost_to_replace_extents(ext_size, exts_per_locality,
                                            obs_data_per_locality);
  }
  double cost_to_write_data(int data) override { return data; }
};

class NumStripesStriper : public AbstractStriperDecorator {
public:
  int num_stripes_per_cycle;
  NumStripesStriper(int n, shared_ptr<AbstractStriper> s)
      : AbstractStriperDecorator(s), num_stripes_per_cycle(n) {}

  int num_stripes_reqd() override { return num_stripes_per_cycle; }

  str_costs create_stripes(shared_ptr<AbstractExtentStack> extent_stack,
                           float simulation_time) override {
    str_costs total = {0};
    for (int i = 0; i < this->num_stripes_per_cycle; i++)
      total += striper->create_stripes(extent_stack, simulation_time);
    return total;
  }

  str_costs create_stripe(shared_ptr<AbstractExtentStack> extent_stack,
                          float simulation_time) override {
    return striper->create_stripes(extent_stack, simulation_time);
  }

  repl_costs cost_to_replace_extents(int ext_size, int exts_per_locality,
                                     double obs_data_per_locality) override {
    return striper->cost_to_replace_extents(ext_size, exts_per_locality,
                                            obs_data_per_locality);
  }
  double cost_to_write_data(int data) override { return data; }
};

class StriperWithEC : public AbstractStriperDecorator {

protected:
  shared_ptr<AbstractStriper> striper;

public:
  StriperWithEC(shared_ptr<AbstractStriper> s, bool is_threaded = false)
      : AbstractStriperDecorator(s, is_threaded), striper(s) {}

  int num_stripes_reqd() override { return striper->num_stripes_reqd(); }

  str_costs create_stripes(shared_ptr<AbstractExtentStack> extent_stack,
                           float simulation_time) override {
    lock(mtx);
    str_costs res = striper->create_stripes(extent_stack, simulation_time);
    res.writes *= stripe_manager->coding_overhead;
    unlock(mtx);
    return res;
  }

  str_costs create_stripe(shared_ptr<AbstractExtentStack> extent_stack,
                          float simulation_time) override {
    lock(mtx);
    str_costs res = striper->create_stripe(extent_stack, simulation_time);
    res.writes *= stripe_manager->coding_overhead;
    unlock(mtx);
    return res;
  }
  repl_costs
  cost_to_replace_extents(int ext_size, vector<int> exts_per_locality,
                          vector<int> obs_data_per_locality,
                          vector<int> valid_objs_per_locality) override {
    repl_costs costs = {0, 0, 0, 0, 0, 0, 0};
    int total_exts_replaced = 0;
    lock(mtx);
    for (int i = 0; i < exts_per_locality.size(); i++) {
      total_exts_replaced += exts_per_locality[i];
    }
    if (total_exts_replaced == stripe_manager->num_data_exts_per_stripe) {
      costs.global_parity_writes =
          stripe_manager->num_global_parities * ext_size;
      costs.local_parity_writes = stripe_manager->num_local_parities * ext_size;
      return costs;
    }
    int num_exts_per_locality = stripe_manager->num_data_exts_per_locality;
    for (int i = 0; i < obs_data_per_locality.size(); i++) {
      int num_exts = exts_per_locality[i];
      if (num_exts == num_exts_per_locality) {
        costs.valid_obj_reads += valid_objs_per_locality[i];

        // read in obsolete data - needed for updating global parities
        costs.obsolete_data_reads += obs_data_per_locality[i];
        // compute new parity block and write it out
        costs.local_parity_writes += ext_size;
      } // replacing some extents in locality
      else if (num_exts != 0) {
        costs.valid_obj_reads += valid_objs_per_locality[i];
        costs.obsolete_data_reads += obs_data_per_locality[i];
        costs.local_parity_reads += ext_size;
        costs.local_parity_writes += ext_size;
      }
    }
    unlock(mtx);
    costs.global_parity_reads +=
          stripe_manager->num_global_parities * ext_size;
    costs.global_parity_writes +=
          stripe_manager->num_global_parities * ext_size;
    num_times_default += 1;
    return costs;
  }
  double cost_to_write_data(int data) override { return data; }
};

class EfficientStriperWithEC : public StriperWithEC {
  using StriperWithEC::StriperWithEC;

  repl_costs
  cost_to_replace_extents(int ext_size, vector<int> exts_per_locality,
                          vector<int> obs_data_per_locality,
                          vector<int> valid_objs_per_locality) override {
    repl_costs costs = {0, 0, 0, 0, 0, 0, 0};
    int str1_ec_reads = 0;
    int total_exts_replaced = 0;
    for (int i = 0; i < exts_per_locality.size(); i++) {
      total_exts_replaced += exts_per_locality[i];
    }
    if (total_exts_replaced == stripe_manager->num_data_exts_per_stripe) {
      costs.global_parity_writes =
          stripe_manager->num_global_parities * ext_size;
      costs.local_parity_writes = stripe_manager->num_local_parities * ext_size;
      return costs;
    }
    int num_exts_per_locality = stripe_manager->num_data_exts_per_locality;
    for (int i = 0; i < obs_data_per_locality.size(); i++) {
      int num_exts = exts_per_locality[i];
      // whole locality is replaced
      if (num_exts == num_exts_per_locality) {
        costs.valid_obj_reads += valid_objs_per_locality[i];
        // read in obsolete data - needed for updating global parities
        costs.obsolete_data_reads += obs_data_per_locality[i];
        // compute new parity block and write it out
        costs.local_parity_writes += ext_size;
      }
      // replacing some extents in locality
      else if (num_exts != 0) {
        costs.valid_obj_reads += valid_objs_per_locality[i];

        // read in obsolete data
        costs.obsolete_data_reads += obs_data_per_locality[i];
        // read in parity
        costs.local_parity_reads += ext_size;

        // in this strategy read in the extents not being gc'ed instead
        // and recompute the parities from scratch
        costs.absent_data_reads +=
            (num_exts_per_locality - num_exts) * ext_size;

        // recompute and write out parity
        costs.local_parity_writes += ext_size;
      } else if (num_exts == 0) {
        // need to bring in the extents from any locality not gc'ed to
        // compute global parities
        costs.absent_data_reads += (num_exts_per_locality)*ext_size;
      }
    }

    costs.global_parity_reads += stripe_manager->num_global_parities * ext_size;
    costs.global_parity_writes +=
        stripe_manager->num_global_parities * ext_size;
    str1_ec_reads = costs.global_parity_reads + costs.obsolete_data_reads +
                    costs.local_parity_reads;

    if (str1_ec_reads < costs.absent_data_reads) {
      num_times_default += 1;
      costs.absent_data_reads = 0;
      return costs;
    } else {
      num_times_alternatives += 1;
      costs.global_parity_reads = 0;
      costs.local_parity_reads = 0;
      costs.obsolete_data_reads = 0;
      costs.valid_obj_reads = 0;
      return costs;
    }
  }
};
