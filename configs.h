#ifndef __CONFIGS_H_
#define __CONFIGS_H_
#include "data_center.h"
#include "extent_manager.h"
#include "extent_object_stripe.h"
#include "extent_stack.h"
#include "gc_strategies.h"
#include "object_manager.h"
#include "object_packer.h"
#include "samplers.h"
#include "stripe_manager.h"
#include "stripers.h"
#include "striping_process_coordinator.h"
#include <any>
#include <memory>
#include <queue>
#include <variant>

using std::cout, std::cerr, std::endl;
using std::make_shared;
using std::map;
using std::static_pointer_cast;
using object_lst = std::vector<obj_record>;
using obj_pq = std::priority_queue<std::variant<obj_record, obj_pq_record>>;

inline std::tuple<shared_ptr<StripeManager>, shared_ptr<EventManager>,
                  shared_ptr<ObjectManager>, shared_ptr<ExtentManager>>
create_managers(const int num_data_exts, const int num_local_parities,
                const int num_global_parities, const int num_localities,
                const shared_ptr<SimpleSampler> sampler, const int ext_size,
                float (Extent::*key_fnc)(), const float coding_overhead = 0,
                const bool add_noise = true) {
  shared_ptr<StripeManager> stripe_mngr;
  if (coding_overhead != 0) {
    stripe_mngr = make_shared<StripeManager>(num_data_exts, num_local_parities,
                                             num_global_parities,
                                             num_localities, coding_overhead);
  } else {
    stripe_mngr = make_shared<StripeManager>(num_data_exts, num_local_parities,
                                             num_global_parities,
                                             num_localities, coding_overhead);
  }
  shared_ptr<EventManager> event_mngr = make_shared<EventManager>();
  shared_ptr<ObjectManager> obj_mngr =
      make_shared<ObjectManager>(event_mngr, sampler, add_noise);
  shared_ptr<ExtentManager> ext_mngr =
      make_shared<ExtentManager>(ext_size, key_fnc);
  return std::make_tuple(stripe_mngr, event_mngr, obj_mngr, ext_mngr);
}

inline DataCenter stripe_level_with_no_exts_config(
    const unsigned long data_center_size, const float striping_cycle,
    const float simul_time, const int ext_size, const short primary_threshold,
    const short secondary_threshold, shared_ptr<SimpleSampler> sampler,
    const short num_stripes_per_cycle, const float deletion_cycle,
    const int num_objs) {
  int num_data_exts = 1;
  float coding_overhead = 18.0 / 14.0;
  float num_global_parities = 2.0 / 14.0;
  float num_local_parities = 2.0 / 14.0;
  int num_localities = 1;
  std::cout << coding_overhead << " " << num_global_parities << std::endl;
  std::tuple<shared_ptr<StripeManager>, shared_ptr<EventManager>,
             shared_ptr<ObjectManager>, shared_ptr<ExtentManager>>
      mngrs = create_managers(num_data_exts, num_local_parities,
                              num_global_parities, num_localities, sampler,
                              ext_size, &Extent::get_default_key,
                              coding_overhead);
  shared_ptr<StripeManager> stripe_mngr =
      std::get<shared_ptr<StripeManager>>(mngrs);
  shared_ptr<ExtentManager> ext_mngr =
      std::get<shared_ptr<ExtentManager>>(mngrs);
  shared_ptr<ObjectManager> obj_mngr =
      std::get<shared_ptr<ObjectManager>>(mngrs);
  shared_ptr<EventManager> event_mngr =
      std::get<shared_ptr<EventManager>>(mngrs);

  shared_ptr<AbstractStriperDecorator> striper =
      make_shared<StriperWithEC>(make_shared<ExtentStackStriper>(
          make_shared<SimpleStriper>(stripe_mngr, ext_mngr)));
  shared_ptr<StriperWithEC> gc_striper =
      make_shared<StriperWithEC>(make_shared<ExtentStackStriper>(
          make_shared<SimpleStriper>(stripe_mngr, ext_mngr)));
  shared_ptr<SimpleObjectPacker> obj_packer = make_shared<SimpleObjectPacker>(
      obj_mngr, ext_mngr, object_lst(), current_extents(), num_objs,
      primary_threshold, true);
  shared_ptr<SimpleGCObjectPacker> gc_obj_packer =
      make_shared<SimpleGCObjectPacker>(obj_mngr, ext_mngr, object_lst(),
                                        current_extents(), num_objs,
                                        primary_threshold, true);
  shared_ptr<AbstractExtentStack> extent_stack =
      make_shared<SingleExtentStack<>>(stripe_mngr);
  shared_ptr<AbstractExtentStack> gc_extent_stack =
      make_shared<SingleExtentStack<>>(stripe_mngr);
  shared_ptr<StripingProcessCoordinator> coordinator =
      make_shared<StripingProcessCoordinator>(
          obj_packer, gc_obj_packer, striper, gc_striper, extent_stack,
          gc_extent_stack, stripe_mngr, simul_time);
  auto gc_strategy = make_shared<StripeLevelNoExtsGCStrategy>(
      primary_threshold, secondary_threshold, ext_mngr, coordinator, gc_striper,
      stripe_mngr);
  DataCenter data_center =
      DataCenter(data_center_size, striping_cycle, striper, stripe_mngr,
                 ext_mngr, obj_mngr, event_mngr, gc_strategy, coordinator,
                 simul_time, deletion_cycle);

  return data_center;
}

inline DataCenter no_exts_mix_objs_config(
    const unsigned long data_center_size, const float striping_cycle,
    const float simul_time, const int ext_size, const short primary_threshold,
    const short secondary_threshold, shared_ptr<SimpleSampler> sampler,
    const short num_stripes_per_cycle, const float deletion_cycle,
    const int num_objs) {
  int num_data_exts = 1;
  float coding_overhead = 18 / 14;
  float num_global_parities = 2 / 14;
  float num_local_parities = 2 / 14;
  int num_localities = 1;
  std::tuple<shared_ptr<StripeManager>, shared_ptr<EventManager>,
             shared_ptr<ObjectManager>, shared_ptr<ExtentManager>>
      mngrs =
          create_managers(num_data_exts, num_local_parities,
                          num_global_parities, num_localities, sampler,
                          ext_size, &Extent::get_default_key, coding_overhead);
  shared_ptr<StripeManager> stripe_mngr =
      std::get<shared_ptr<StripeManager>>(mngrs);
  shared_ptr<ExtentManager> ext_mngr =
      std::get<shared_ptr<ExtentManager>>(mngrs);
  shared_ptr<ObjectManager> obj_mngr =
      std::get<shared_ptr<ObjectManager>>(mngrs);
  shared_ptr<EventManager> event_mngr =
      std::get<shared_ptr<EventManager>>(mngrs);
  shared_ptr<AbstractStriperDecorator> striper =
      make_shared<StriperWithEC>(make_shared<ExtentStackStriper>(
          make_shared<SimpleStriper>(stripe_mngr, ext_mngr)));

  shared_ptr<AbstractStriperDecorator> gc_striper =
      make_shared<StriperWithEC>(make_shared<ExtentStackStriper>(
          make_shared<SimpleStriper>(stripe_mngr, ext_mngr)));
  current_extents current_exts;
  current_exts.emplace(0, ext_mngr->create_extent());
  shared_ptr<SimpleObjectPacker> obj_packer = make_shared<MixedObjObjectPacker>(
      obj_mngr, ext_mngr, object_lst(), current_exts, num_objs,
      primary_threshold, false);
  shared_ptr<SimpleGCObjectPacker> gc_obj_packer =
      make_shared<MixedObjGCObjectPacker>(obj_mngr, ext_mngr, object_lst(),
                                          current_exts, num_objs,
                                          primary_threshold, false);
  shared_ptr<AbstractExtentStack> extent_stack =
      make_shared<SingleExtentStack<>>(stripe_mngr);
  shared_ptr<AbstractExtentStack> gc_extent_stack = extent_stack;
  shared_ptr<StripingProcessCoordinator> coordinator =
      make_shared<StripingProcessCoordinator>(
          obj_packer, gc_obj_packer, striper, gc_striper, extent_stack,
          gc_extent_stack, stripe_mngr, simul_time);
  /* TODO
      gc_strategy =MixObjStripeLevelStrategy(primary_threshold,
     secondary_threshold, coordinator, ext_mngr, gc_striper)
      */
  shared_ptr<GarbageCollectionStrategy> gc_strategy = nullptr;
  DataCenter data_center =
      DataCenter(data_center_size, striping_cycle, striper, stripe_mngr,
                 ext_mngr, obj_mngr, event_mngr, gc_strategy, coordinator,
                 simul_time, deletion_cycle);

  return data_center;
}

inline DataCenter stripe_level_with_extents_separate_pools_config(
    const unsigned long data_center_size, const float striping_cycle,
    const float simul_time, const int ext_size, const short primary_threshold,
    const short secondary_threshold, shared_ptr<SimpleSampler> sampler,
    const short num_stripes_per_cycle, const float deletion_cycle,
    const int num_objs) {
  int num_data_exts = 7;
  float num_global_parities = 2;
  float num_local_parities = 2;
  int num_localities = 2;
  std::tuple<shared_ptr<StripeManager>, shared_ptr<EventManager>,
             shared_ptr<ObjectManager>, shared_ptr<ExtentManager>>
      mngrs = create_managers(num_data_exts, num_local_parities,
                              num_global_parities, num_localities, sampler,
                              ext_size, &Extent::get_default_key);
  shared_ptr<StripeManager> stripe_mngr =
      std::get<shared_ptr<StripeManager>>(mngrs);
  shared_ptr<ExtentManager> ext_mngr =
      std::get<shared_ptr<ExtentManager>>(mngrs);
  shared_ptr<ObjectManager> obj_mngr =
      std::get<shared_ptr<ObjectManager>>(mngrs);
  shared_ptr<EventManager> event_mngr =
      std::get<shared_ptr<EventManager>>(mngrs);

  shared_ptr<AbstractStriperDecorator> striper =
      make_shared<StriperWithEC>(make_shared<ExtentStackStriper>(
          make_shared<SimpleStriper>(stripe_mngr, ext_mngr)));
  shared_ptr<AbstractStriperDecorator> gc_striper = striper;
  shared_ptr<SimpleObjectPacker> obj_packer = make_shared<SimpleObjectPacker>(
      obj_mngr, ext_mngr, object_lst(), current_extents(), num_objs,
      primary_threshold, false);
  shared_ptr<SimpleGCObjectPacker> gc_obj_packer =
      make_shared<SimpleGCObjectPacker>(obj_mngr, ext_mngr, object_lst(),
                                        current_extents(), num_objs,
                                        primary_threshold, false);
  shared_ptr<AbstractExtentStack> extent_stack =
      make_shared<SingleExtentStack<>>(stripe_mngr);
  shared_ptr<AbstractExtentStack> gc_extent_stack = extent_stack;
  shared_ptr<StripingProcessCoordinator> coordinator =
      make_shared<StripingProcessCoordinator>(
          obj_packer, gc_obj_packer, striper, gc_striper, extent_stack,
          gc_extent_stack, stripe_mngr, simul_time);
  /*TODO
      gc_strategy = StripeLevelWithExtsGCStrategy(primary_threshold,
     secondary_threshold, coordinator, ext_mngr, gc_striper)
      */
  shared_ptr<GarbageCollectionStrategy> gc_strategy = nullptr;
  DataCenter data_center =
      DataCenter(data_center_size, striping_cycle, striper, stripe_mngr,
                 ext_mngr, obj_mngr, event_mngr, gc_strategy, coordinator,
                 simul_time, deletion_cycle);
  return data_center;
}

inline DataCenter stripe_level_with_extents_separate_pools_efficient_config(
    const unsigned long data_center_size, const float striping_cycle,
    const float simul_time, const int ext_size, const short primary_threshold,
    const short secondary_threshold, shared_ptr<SimpleSampler> sampler,
    const short num_stripes_per_cycle, const float deletion_cycle,
    const int num_objs) {
  int num_data_exts = 7;
  float num_global_parities = 2;
  float num_local_parities = 2;
  int num_localities = 2;
  std::tuple<shared_ptr<StripeManager>, shared_ptr<EventManager>,
             shared_ptr<ObjectManager>, shared_ptr<ExtentManager>>
      mngrs = create_managers(num_data_exts, num_local_parities,
                              num_global_parities, num_localities, sampler,
                              ext_size, &Extent::get_default_key);
  shared_ptr<StripeManager> stripe_mngr =
      std::get<shared_ptr<StripeManager>>(mngrs);
  shared_ptr<ExtentManager> ext_mngr =
      std::get<shared_ptr<ExtentManager>>(mngrs);
  shared_ptr<ObjectManager> obj_mngr =
      std::get<shared_ptr<ObjectManager>>(mngrs);
  shared_ptr<EventManager> event_mngr =
      std::get<shared_ptr<EventManager>>(mngrs);

  shared_ptr<AbstractStriperDecorator> striper =
      make_shared<EfficientStriperWithEC>(make_shared<ExtentStackStriper>(
          make_shared<SimpleStriper>(stripe_mngr, ext_mngr)));
  shared_ptr<AbstractStriperDecorator> gc_striper = striper;
  shared_ptr<SimpleObjectPacker> obj_packer = make_shared<SimpleObjectPacker>(
      obj_mngr, ext_mngr, object_lst(), current_extents(), num_objs);
  shared_ptr<SimpleGCObjectPacker> gc_obj_packer =
      make_shared<SimpleGCObjectPacker>(obj_mngr, ext_mngr, object_lst(),
                                        current_extents(), num_objs);
  shared_ptr<AbstractExtentStack> extent_stack =
      make_shared<SingleExtentStack<>>(stripe_mngr);
  shared_ptr<AbstractExtentStack> gc_extent_stack = extent_stack;
  shared_ptr<StripingProcessCoordinator> coordinator =
      make_shared<StripingProcessCoordinator>(
          obj_packer, gc_obj_packer, striper, gc_striper, extent_stack,
          gc_extent_stack, stripe_mngr, simul_time);
  /*TODO
      gc_strategy = StripeLevelWithExtsGCStrategy(primary_threshold,
     secondary_threshold, coordinator, ext_mngr, gc_striper)
      */
  shared_ptr<GarbageCollectionStrategy> gc_strategy = nullptr;
  DataCenter data_center =
      DataCenter(data_center_size, striping_cycle, striper, stripe_mngr,
                 ext_mngr, obj_mngr, event_mngr, gc_strategy, coordinator,
                 simul_time, deletion_cycle);
  return data_center;
}

inline float get_timestamp() { return configtime; }

inline DataCenter age_based_config_no_exts(
    const unsigned long data_center_size, const float striping_cycle,
    const float simul_time, const int ext_size, const short primary_threshold,
    const short secondary_threshold, shared_ptr<SimpleSampler> sampler,
    const short num_stripes_per_cycle, const float deletion_cycle,
    const int num_objs) {
  int num_data_exts = 1;
  float coding_overhead = 18 / 14;
  float num_global_parities = 2 / 14;
  float num_local_parities = 2 / 14;
  int num_localities = 1;
  std::tuple<shared_ptr<StripeManager>, shared_ptr<EventManager>,
             shared_ptr<ObjectManager>, shared_ptr<ExtentManager>>
      mngrs =
          create_managers(num_data_exts, num_local_parities,
                          num_global_parities, num_localities, sampler,
                          ext_size, &Extent::get_timestamp, coding_overhead);
  shared_ptr<StripeManager> stripe_mngr =
      std::get<shared_ptr<StripeManager>>(mngrs);
  shared_ptr<ExtentManager> ext_mngr =
      std::get<shared_ptr<ExtentManager>>(mngrs);
  shared_ptr<ObjectManager> obj_mngr =
      std::get<shared_ptr<ObjectManager>>(mngrs);
  shared_ptr<EventManager> event_mngr =
      std::get<shared_ptr<EventManager>>(mngrs);

  shared_ptr<AbstractStriperDecorator> striper =
      make_shared<StriperWithEC>(make_shared<ExtentStackStriper>(
          make_shared<SimpleStriper>(stripe_mngr, ext_mngr)));
  shared_ptr<AbstractStriperDecorator> gc_striper = striper;
  shared_ptr<SimpleObjectPacker> obj_packer = make_shared<AgeBasedObjectPacker>(
      obj_mngr, ext_mngr, obj_pq(), current_extents(), num_objs,
      primary_threshold);
  shared_ptr<SimpleGCObjectPacker> gc_obj_packer =
      make_shared<AgeBasedGCObjectPacker>(obj_mngr, ext_mngr, obj_pq(),
                                          current_extents(), num_objs,
                                          primary_threshold);
  shared_ptr<AbstractExtentStack> extent_stack =
      make_shared<BestEffortExtentStack>(stripe_mngr);
  shared_ptr<AbstractExtentStack> gc_extent_stack =
      make_shared<BestEffortExtentStack>(stripe_mngr);
  shared_ptr<StripingProcessCoordinator> coordinator =
      make_shared<BestEffortStripingProcessCoordinator>(
          obj_packer, gc_obj_packer, striper, gc_striper, extent_stack,
          gc_extent_stack, stripe_mngr, simul_time, get_timestamp);
  /*TODO
  gc_strategy = StripeLevelNoExtsGCStrategy(primary_threshold,
  secondary_threshold, coordinator, ext_mngr, stripe_mngr, gc_striper)
  */
  shared_ptr<GarbageCollectionStrategy> gc_strategy = nullptr;
  DataCenter data_center =
      DataCenter(data_center_size, striping_cycle, striper, stripe_mngr,
                 ext_mngr, obj_mngr, event_mngr, gc_strategy, coordinator,
                 simul_time, deletion_cycle);

  return data_center;
}

float get_time() { return configtime; }

inline DataCenter age_based_config(
    const unsigned long data_center_size, const float striping_cycle,
    const float simul_time, const int ext_size, const short primary_threshold,
    const short secondary_threshold, shared_ptr<SimpleSampler> sampler,
    const short num_stripes_per_cycle, const float deletion_cycle,
    const int num_objs) {
  int num_data_exts = 7;
  float num_global_parities = 2;
  float num_local_parities = 2;
  int num_localities = 2;
  std::tuple<shared_ptr<StripeManager>, shared_ptr<EventManager>,
             shared_ptr<ObjectManager>, shared_ptr<ExtentManager>>
      mngrs = create_managers(num_data_exts, num_local_parities,
                              num_global_parities, num_localities, sampler,
                              ext_size, &Extent::get_timestamp);
  shared_ptr<StripeManager> stripe_mngr =
      std::get<shared_ptr<StripeManager>>(mngrs);
  shared_ptr<ExtentManager> ext_mngr =
      std::get<shared_ptr<ExtentManager>>(mngrs);
  shared_ptr<ObjectManager> obj_mngr =
      std::get<shared_ptr<ObjectManager>>(mngrs);
  shared_ptr<EventManager> event_mngr =
      std::get<shared_ptr<EventManager>>(mngrs);
  shared_ptr<AbstractStriperDecorator> striper =
      make_shared<StriperWithEC>(make_shared<ExtentStackStriper>(
          make_shared<SimpleStriper>(stripe_mngr, ext_mngr)));
  shared_ptr<AbstractStriperDecorator> gc_striper = striper;
  shared_ptr<SimpleObjectPacker> obj_packer = make_shared<AgeBasedObjectPacker>(
      obj_mngr, ext_mngr, obj_pq(), current_extents(), num_objs);
  shared_ptr<SimpleGCObjectPacker> gc_obj_packer =
      make_shared<AgeBasedGCObjectPacker>(obj_mngr, ext_mngr, obj_pq(),
                                          current_extents(), num_objs);
  shared_ptr<AbstractExtentStack> extent_stack =
      make_shared<BestEffortExtentStack>(stripe_mngr);
  shared_ptr<AbstractExtentStack> gc_extent_stack = extent_stack;
  shared_ptr<StripingProcessCoordinator> coordinator =
      make_shared<BestEffortStripingProcessCoordinator>(
          obj_packer, gc_obj_packer, striper, gc_striper, extent_stack,
          gc_extent_stack, stripe_mngr, simul_time, get_time);
  /*TODO
  gc_strategy = StripeLevelNoExtsGCStrategy(primary_threshold,
  secondary_threshold, coordinator, ext_mngr, stripe_mngr, gc_striper)
  */
  shared_ptr<GarbageCollectionStrategy> gc_strategy = nullptr;
  DataCenter data_center =
      DataCenter(data_center_size, striping_cycle, striper, stripe_mngr,
                 ext_mngr, obj_mngr, event_mngr, gc_strategy, coordinator,
                 simul_time, deletion_cycle);
  return data_center;
}

inline DataCenter size_based_stripe_level_no_exts_baseline_config(
    const unsigned long data_center_size, const float striping_cycle,
    const float simul_time, const int ext_size, const short primary_threshold,
    const short secondary_threshold, shared_ptr<SimpleSampler> sampler,
    const short num_stripes_per_cycle, const float deletion_cycle,
    const int num_objs) {
  int num_data_exts = 1;
  float coding_overhead = 18 / 14;
  float num_global_parities = 2 / 14;
  float num_local_parities = 2 / 14;
  int num_localities = 1;
  std::tuple<shared_ptr<StripeManager>, shared_ptr<EventManager>,
             shared_ptr<ObjectManager>, shared_ptr<ExtentManager>>
      mngrs =
          create_managers(num_data_exts, num_local_parities,
                          num_global_parities, num_localities, sampler,
                          ext_size, &Extent::get_default_key, coding_overhead);
  shared_ptr<StripeManager> stripe_mngr =
      std::get<shared_ptr<StripeManager>>(mngrs);
  shared_ptr<ExtentManager> ext_mngr =
      std::get<shared_ptr<ExtentManager>>(mngrs);
  shared_ptr<ObjectManager> obj_mngr =
      std::get<shared_ptr<ObjectManager>>(mngrs);
  shared_ptr<EventManager> event_mngr =
      std::get<shared_ptr<EventManager>>(mngrs);

  shared_ptr<AbstractStriperDecorator> striper =
      make_shared<StriperWithEC>(make_shared<ExtentStackStriper>(
          make_shared<SimpleStriper>(stripe_mngr, ext_mngr)));
  shared_ptr<AbstractStriperDecorator> gc_striper = striper;
  shared_ptr<SimpleObjectPacker> obj_packer =
      make_shared<SizeBasedObjectPackerBaseline>(
          obj_mngr, ext_mngr, object_lst(), current_extents(), num_objs,
          primary_threshold);
  shared_ptr<SimpleGCObjectPacker> gc_obj_packer =
      make_shared<SizeBasedGCObjectPackerBaseline>(
          obj_mngr, ext_mngr, object_lst(), current_extents(), num_objs,
          primary_threshold);
  shared_ptr<AbstractExtentStack> extent_stack =
      make_shared<SingleExtentStack<>>(stripe_mngr);
  shared_ptr<AbstractExtentStack> gc_extent_stack =
      make_shared<SingleExtentStack<>>(stripe_mngr);
  shared_ptr<StripingProcessCoordinator> coordinator =
      make_shared<StripingProcessCoordinator>(
          obj_packer, gc_obj_packer, striper, gc_striper, extent_stack,
          gc_extent_stack, stripe_mngr, simul_time);
  /*TODO
  gc_strategy = StripeLevelNoExtsGCStrategy(primary_threshold,
  secondary_threshold, coordinator, ext_mngr, stripe_mngr, gc_striper)
  */
  shared_ptr<GarbageCollectionStrategy> gc_strategy = nullptr;
  DataCenter data_center =
      DataCenter(data_center_size, striping_cycle, striper, stripe_mngr,
                 ext_mngr, obj_mngr, event_mngr, gc_strategy, coordinator,
                 simul_time, deletion_cycle);

  return data_center;
}

DataCenter size_based_stripe_level_no_exts_smaller_obj_config(
    const unsigned long data_center_size, const float striping_cycle,
    const float simul_time, const int ext_size, const short primary_threshold,
    const short secondary_threshold, shared_ptr<SimpleSampler> sampler,
    const short num_stripes_per_cycle, const float deletion_cycle,
    const int num_objs) {
  int num_data_exts = 1;
  float coding_overhead = 18 / 14;
  float num_global_parities = 2 / 14;
  float num_local_parities = 2 / 14;
  int num_localities = 1;
  std::tuple<shared_ptr<StripeManager>, shared_ptr<EventManager>,
             shared_ptr<ObjectManager>, shared_ptr<ExtentManager>>
      mngrs =
          create_managers(num_data_exts, num_local_parities,
                          num_global_parities, num_localities, sampler,
                          ext_size, &Extent::get_default_key, coding_overhead);
  shared_ptr<StripeManager> stripe_mngr =
      std::get<shared_ptr<StripeManager>>(mngrs);
  shared_ptr<ExtentManager> ext_mngr =
      std::get<shared_ptr<ExtentManager>>(mngrs);
  shared_ptr<ObjectManager> obj_mngr =
      std::get<shared_ptr<ObjectManager>>(mngrs);
  shared_ptr<EventManager> event_mngr =
      std::get<shared_ptr<EventManager>>(mngrs);

  shared_ptr<AbstractStriperDecorator> striper =
      make_shared<StriperWithEC>(make_shared<ExtentStackStriper>(
          make_shared<SimpleStriper>(stripe_mngr, ext_mngr)));
  shared_ptr<AbstractStriperDecorator> gc_striper = striper;
  shared_ptr<SimpleObjectPacker> obj_packer =
      make_shared<SizeBasedObjectPackerSmallerObj>(
          obj_mngr, ext_mngr, object_lst(), current_extents(), num_objs,
          primary_threshold, true);
  shared_ptr<SimpleObjectPacker> gc_obj_packer =
      make_shared<SizeBasedGCObjectPackerSmallerObj>(
          obj_mngr, ext_mngr, object_lst(), current_extents(), num_objs,
          primary_threshold, true);
  shared_ptr<AbstractExtentStack> extent_stack =
      make_shared<SingleExtentStack<>>(stripe_mngr);
  shared_ptr<AbstractExtentStack> gc_extent_stack =
      make_shared<SingleExtentStack<>>(stripe_mngr);
  shared_ptr<StripingProcessCoordinator> coordinator =
      make_shared<StripingProcessCoordinator>(
          obj_packer, gc_obj_packer, striper, gc_striper, extent_stack,
          gc_extent_stack, stripe_mngr, simul_time);
  /*TODO
  gc_strategy = StripeLevelNoExtsGCStrategy(primary_threshold,
  secondary_threshold, coordinator, ext_mngr, stripe_mngr, gc_striper)
  */
  shared_ptr<GarbageCollectionStrategy> gc_strategy = nullptr;

  DataCenter data_center =
      DataCenter(data_center_size, striping_cycle, striper, stripe_mngr,
                 ext_mngr, obj_mngr, event_mngr, gc_strategy, coordinator,
                 simul_time, deletion_cycle);

  return data_center;
}

inline DataCenter size_based_stripe_level_no_exts_dynamic_strategy_config(
    const unsigned long data_center_size, const float striping_cycle,
    const float simul_time, const int ext_size, const short primary_threshold,
    const short secondary_threshold, shared_ptr<SimpleSampler> sampler,
    const short num_stripes_per_cycle, const float deletion_cycle,
    const int num_objs) {
  int num_data_exts = 1;
  float coding_overhead = 18 / 14;
  float num_global_parities = 2 / 14;
  float num_local_parities = 2 / 14;
  int num_localities = 1;
  std::tuple<shared_ptr<StripeManager>, shared_ptr<EventManager>,
             shared_ptr<ObjectManager>, shared_ptr<ExtentManager>>
      mngrs =
          create_managers(num_data_exts, num_local_parities,
                          num_global_parities, num_localities, sampler,
                          ext_size, &Extent::get_default_key, coding_overhead);
  shared_ptr<StripeManager> stripe_mngr =
      std::get<shared_ptr<StripeManager>>(mngrs);
  shared_ptr<ExtentManager> ext_mngr =
      std::get<shared_ptr<ExtentManager>>(mngrs);
  shared_ptr<ObjectManager> obj_mngr =
      std::get<shared_ptr<ObjectManager>>(mngrs);
  shared_ptr<EventManager> event_mngr =
      std::get<shared_ptr<EventManager>>(mngrs);

  shared_ptr<AbstractStriperDecorator> striper =
      make_shared<StriperWithEC>(make_shared<ExtentStackStriper>(
          make_shared<SimpleStriper>(stripe_mngr, ext_mngr)));
  shared_ptr<AbstractStriperDecorator> gc_striper = striper;
  shared_ptr<SimpleObjectPacker> obj_packer =
      make_shared<SizeBasedObjectPackerDynamicStrategy>(
          obj_mngr, ext_mngr, object_lst(), current_extents(), num_objs,
          primary_threshold, true);
  shared_ptr<SimpleObjectPacker> gc_obj_packer =
      make_shared<SizeBasedGCObjectPackerDynamicStrategy>(
          obj_mngr, ext_mngr, object_lst(), current_extents(), num_objs,
          primary_threshold, true);
  shared_ptr<AbstractExtentStack> extent_stack =
      make_shared<WholeObjectExtentStack>(stripe_mngr);
  shared_ptr<AbstractExtentStack> gc_extent_stack =
      make_shared<WholeObjectExtentStack>(stripe_mngr);
  shared_ptr<StripingProcessCoordinator> coordinator =
      make_shared<StripingProcessCoordinator>(
          obj_packer, gc_obj_packer, striper, gc_striper, extent_stack,
          gc_extent_stack, stripe_mngr, simul_time);
  /*TODO
gc_strategy = StripeLevelNoExtsGarbageCollectionStrategy(primary_threshold,
secondary_threshold, coordinator, ext_mngr, stripe_mngr, gc_striper,
StripeLevelGCStrategy(primary_threshold, secondary_threshold, coordinator,
ext_mngr, gc_striper))
  */
  shared_ptr<GarbageCollectionStrategy> gc_strategy = nullptr;
  DataCenter data_center =
      DataCenter(data_center_size, striping_cycle, striper, stripe_mngr,
                 ext_mngr, obj_mngr, event_mngr, gc_strategy, coordinator,
                 simul_time, deletion_cycle);

  return data_center;
}

inline DataCenter size_based_whole_obj_config(
    const unsigned long data_center_size, const float striping_cycle,
    const float simul_time, const int ext_size, const short primary_threshold,
    const short secondary_threshold, shared_ptr<SimpleSampler> sampler,
    const short num_stripes_per_cycle, const float deletion_cycle,
    const int num_objs) {
  int num_data_exts = 7;
  float num_global_parities = 2;
  float num_local_parities = 2;
  int num_localities = 2;
  std::tuple<shared_ptr<StripeManager>, shared_ptr<EventManager>,
             shared_ptr<ObjectManager>, shared_ptr<ExtentManager>>
      mngrs = create_managers(num_data_exts, num_local_parities,
                              num_global_parities, num_localities, sampler,
                              ext_size, &Extent::get_default_key);
  shared_ptr<StripeManager> stripe_mngr =
      std::get<shared_ptr<StripeManager>>(mngrs);
  shared_ptr<ExtentManager> ext_mngr =
      std::get<shared_ptr<ExtentManager>>(mngrs);
  shared_ptr<ObjectManager> obj_mngr =
      std::get<shared_ptr<ObjectManager>>(mngrs);
  shared_ptr<EventManager> event_mngr =
      std::get<shared_ptr<EventManager>>(mngrs);

  shared_ptr<AbstractStriperDecorator> striper =
      make_shared<StriperWithEC>(make_shared<ExtentStackStriper>(
          make_shared<SimpleStriper>(stripe_mngr, ext_mngr)));
  shared_ptr<AbstractStriperDecorator> gc_striper = striper;
  shared_ptr<SimpleObjectPacker> obj_packer =
      make_shared<SizeBasedObjectPackerSmallerWholeObjFillGap>(
          obj_mngr, ext_mngr, object_lst(), current_extents(), num_objs,
          primary_threshold, true);
  shared_ptr<SimpleObjectPacker> gc_obj_packer =
      make_shared<SizeBasedGCObjectPackerSmallerWholeObjFillGap>(
          obj_mngr, ext_mngr, object_lst(), current_extents(), num_objs,
          primary_threshold, true);
  shared_ptr<AbstractExtentStack> extent_stack =
      static_pointer_cast<AbstractExtentStack>(
          make_shared<WholeObjectExtentStack>(stripe_mngr));
  shared_ptr<AbstractExtentStack> gc_extent_stack =
      static_pointer_cast<AbstractExtentStack>(
          make_shared<WholeObjectExtentStack>(stripe_mngr));
  shared_ptr<StripingProcessCoordinator> coordinator =
      make_shared<StripingProcessCoordinator>(
          obj_packer, gc_obj_packer, striper, gc_striper, extent_stack,
          gc_extent_stack, stripe_mngr, simul_time);
  /*TODO
  gc_strategy = StripeLevelNoExtsGarbageCollectionStrategy(primary_threshold,
  secondary_threshold, coordinator, ext_mngr, stripe_mngr, gc_striper,
  StripeLevelGCStrategy(primary_threshold, secondary_threshold, coordinator,
  ext_mngr, gc_striper))
      */
  shared_ptr<GarbageCollectionStrategy> gc_strategy = nullptr;
  DataCenter data_center =
      DataCenter(data_center_size, striping_cycle, striper, stripe_mngr,
                 ext_mngr, obj_mngr, event_mngr, gc_strategy, coordinator,
                 simul_time, deletion_cycle);
  return data_center;
}

inline DataCenter size_based_stripe_level_no_exts_larger_whole_obj_config(
    const unsigned long data_center_size, const float striping_cycle,
    const float simul_time, const int ext_size, const short primary_threshold,
    const short secondary_threshold, shared_ptr<SimpleSampler> sampler,
    const short num_stripes_per_cycle, const float deletion_cycle,
    const int num_objs) {
  int num_data_exts = 1;
  float coding_overhead = 18 / 14;
  float num_global_parities = 2 / 14;
  float num_local_parities = 2 / 14;
  int num_localities = 1;
  std::tuple<shared_ptr<StripeManager>, shared_ptr<EventManager>,
             shared_ptr<ObjectManager>, shared_ptr<ExtentManager>>
      mngrs =
          create_managers(num_data_exts, num_local_parities,
                          num_global_parities, num_localities, sampler,
                          ext_size, &Extent::get_default_key, coding_overhead);
  shared_ptr<StripeManager> stripe_mngr =
      std::get<shared_ptr<StripeManager>>(mngrs);
  shared_ptr<ExtentManager> ext_mngr =
      std::get<shared_ptr<ExtentManager>>(mngrs);
  shared_ptr<ObjectManager> obj_mngr =
      std::get<shared_ptr<ObjectManager>>(mngrs);
  shared_ptr<EventManager> event_mngr =
      std::get<shared_ptr<EventManager>>(mngrs);

  shared_ptr<AbstractStriperDecorator> striper =
      make_shared<StriperWithEC>(make_shared<ExtentStackStriper>(
          make_shared<SimpleStriper>(stripe_mngr, ext_mngr)));
  shared_ptr<AbstractStriperDecorator> gc_striper = striper;
  shared_ptr<SimpleObjectPacker> obj_packer =
      make_shared<SizeBasedObjectPackerLargerWholeObj>(
          obj_mngr, ext_mngr, object_lst(), current_extents(), num_objs,
          primary_threshold);
  shared_ptr<SimpleObjectPacker> gc_obj_packer =
      make_shared<SizeBasedGCObjectPackerLargerWholeObj>(
          obj_mngr, ext_mngr, object_lst(), current_extents(), num_objs,
          primary_threshold);
  shared_ptr<AbstractExtentStack> extent_stack =
      static_pointer_cast<AbstractExtentStack>(
          make_shared<WholeObjectExtentStack>(stripe_mngr));
  shared_ptr<AbstractExtentStack> gc_extent_stack =
      static_pointer_cast<AbstractExtentStack>(
          make_shared<WholeObjectExtentStack>(stripe_mngr));
  shared_ptr<StripingProcessCoordinator> coordinator =
      make_shared<StripingProcessCoordinator>(
          obj_packer, gc_obj_packer, striper, gc_striper, extent_stack,
          gc_extent_stack, stripe_mngr, simul_time);
  /*TODO
gc_strategy = StripeLevelNoExtsGarbageCollectionStrategy(primary_threshold,
secondary_threshold, coordinator, ext_mngr, stripe_mngr, gc_striper,
StripeLevelGCStrategy(primary_threshold, secondary_threshold, coordinator,
ext_mngr, gc_striper))
  */
  shared_ptr<GarbageCollectionStrategy> gc_strategy = nullptr;
  DataCenter data_center =
      DataCenter(data_center_size, striping_cycle, striper, stripe_mngr,
                 ext_mngr, obj_mngr, event_mngr, gc_strategy, coordinator,
                 simul_time, deletion_cycle);

  return data_center;
}

inline float get_immortal_key() { return 0; }

/*not passing percent correct in
percent_correct = 90
percent_correct = 80
percent_correct = 70
percent_correct = 60*/

inline DataCenter mortal_immortal_no_exts_config(
    const unsigned long data_center_size, const float striping_cycle,
    const float simul_time, const int ext_size, const short primary_threshold,
    const short secondary_threshold, shared_ptr<SimpleSampler> sampler,
    const short num_stripes_per_cycle, const float deletion_cycle,
    const int num_objs, const int percent_correct) {
  int num_data_exts = 1;
  float coding_overhead = 18 / 14;
  float num_global_parities = 2 / 14;
  float num_local_parities = 2 / 14;
  int num_localities = 1;
  std::tuple<shared_ptr<StripeManager>, shared_ptr<EventManager>,
             shared_ptr<ObjectManager>, shared_ptr<ExtentManager>>
      mngrs =
          create_managers(num_data_exts, num_local_parities,
                          num_global_parities, num_localities, sampler,
                          ext_size, &Extent::get_default_key, coding_overhead);
  shared_ptr<StripeManager> stripe_mngr =
      std::get<shared_ptr<StripeManager>>(mngrs);
  shared_ptr<ExtentManager> ext_mngr =
      std::get<shared_ptr<ExtentManager>>(mngrs);
  shared_ptr<ObjectManager> obj_mngr =
      std::get<shared_ptr<ObjectManager>>(mngrs);
  shared_ptr<EventManager> event_mngr =
      std::get<shared_ptr<EventManager>>(mngrs);

  shared_ptr<AbstractStriperDecorator> striper =
      make_shared<StriperWithEC>(make_shared<ExtentStackStriper>(
          make_shared<SimpleStriper>(stripe_mngr, ext_mngr)));
  shared_ptr<AbstractStriperDecorator> gc_striper = striper;

  shared_ptr<SimpleObjectPacker> obj_packer =
      make_shared<MortalImmortalObjectPacker>(obj_mngr, ext_mngr, object_lst(),
                                              current_extents(), num_objs,
                                              percent_correct);
  shared_ptr<SimpleGCObjectPacker> gc_obj_packer =
      make_shared<MortalImmortalGCObjectPacker>(obj_mngr, ext_mngr,
                                                object_lst(), current_extents(),
                                                num_objs, percent_correct);

  shared_ptr<AbstractExtentStack> extent_stack =
      make_shared<BestEffortExtentStack>(stripe_mngr);
  shared_ptr<AbstractExtentStack> gc_extent_stack =
      make_shared<BestEffortExtentStack>(stripe_mngr);
  shared_ptr<StripingProcessCoordinator> coordinator =
      /*shared_ptr<SimpleObjectPacker> o_p,
shared_ptr<SimpleGCObjectPacker> gc_o_p,
shared_ptr<AbstractStriperDecorator> s,
shared_ptr<AbstractStriperDecorator> gc_s,
shared_ptr<AbstractExtentStack> e_s,
shared_ptr<AbstractExtentStack> gc_e_s, shared_ptr<StripeManager> s_m,
int s_t, float (*key)()*/
      make_shared<BestEffortStripingProcessCoordinator>(
          obj_packer, gc_obj_packer, striper, gc_striper, extent_stack,
          gc_extent_stack, stripe_mngr, simul_time, get_immortal_key);
  /*TODO
gc_strategy = StripeLevelNoExtsGCStrategy(primary_threshold,
secondary_threshold, coordinator, ext_mngr, stripe_mngr, gc_striper)
  */
  shared_ptr<GarbageCollectionStrategy> gc_strategy = nullptr;
  DataCenter data_center =
      DataCenter(data_center_size, striping_cycle, striper, stripe_mngr,
                 ext_mngr, obj_mngr, event_mngr, gc_strategy, coordinator,
                 simul_time, deletion_cycle);

  return data_center;
}

inline DataCenter randomized_ext_placement_joined_pools_config(
    const unsigned long data_center_size, const float striping_cycle,
    const float simul_time, const int ext_size, const short primary_threshold,
    const short secondary_threshold, shared_ptr<SimpleSampler> sampler,
    const short num_stripes_per_cycle, const float deletion_cycle,
    const int num_objs) {
  int num_data_exts = 7;
  float num_global_parities = 2;
  float num_local_parities = 2;
  int num_localities = 2;
  std::tuple<shared_ptr<StripeManager>, shared_ptr<EventManager>,
             shared_ptr<ObjectManager>, shared_ptr<ExtentManager>>
      mngrs = create_managers(num_data_exts, num_local_parities,
                              num_global_parities, num_localities, sampler,
                              ext_size, &Extent::get_default_key);
  shared_ptr<StripeManager> stripe_mngr =
      std::get<shared_ptr<StripeManager>>(mngrs);
  shared_ptr<ExtentManager> ext_mngr =
      std::get<shared_ptr<ExtentManager>>(mngrs);
  shared_ptr<ObjectManager> obj_mngr =
      std::get<shared_ptr<ObjectManager>>(mngrs);
  shared_ptr<EventManager> event_mngr =
      std::get<shared_ptr<EventManager>>(mngrs);
  shared_ptr<AbstractStriperDecorator> striper =
      make_shared<StriperWithEC>(make_shared<ExtentStackStriper>(
          make_shared<SimpleStriper>(stripe_mngr, ext_mngr)));

  shared_ptr<AbstractStriperDecorator> gc_striper = striper;
  current_extents current_exts;
  current_exts.emplace(0, ext_mngr->create_extent());
  shared_ptr<SimpleObjectPacker> obj_packer = make_shared<MixedObjObjectPacker>(
      obj_mngr, ext_mngr, object_lst(), current_exts, num_objs);
  shared_ptr<SimpleGCObjectPacker> gc_obj_packer =
      make_shared<MixedObjGCObjectPacker>(obj_mngr, ext_mngr, object_lst(),
                                          current_exts, num_objs);
  shared_ptr<AbstractExtentStack> extent_stack =
      make_shared<ExtentStackRandomizer>(
          make_shared<SingleExtentStack<>>(stripe_mngr));
  shared_ptr<AbstractExtentStack> gc_extent_stack = extent_stack;
  shared_ptr<StripingProcessCoordinator> coordinator =
      make_shared<StripingProcessCoordinator>(
          obj_packer, gc_obj_packer, striper, gc_striper, extent_stack,
          gc_extent_stack, stripe_mngr, simul_time);
  /* TODO
  gc_strategy = StripeLevelWithExtsGCStrategy(primary_threshold,
  secondary_threshold, coordinator, ext_mngr, gc_striper)
      */
  shared_ptr<GarbageCollectionStrategy> gc_strategy = nullptr;
  DataCenter data_center =
      DataCenter(data_center_size, striping_cycle, striper, stripe_mngr,
                 ext_mngr, obj_mngr, event_mngr, gc_strategy, coordinator,
                 simul_time, deletion_cycle);

  return data_center;
}

inline DataCenter randomized_obj_placement_joined_pools_config(
    const unsigned long data_center_size, const float striping_cycle,
    const float simul_time, const int ext_size, const short primary_threshold,
    const short secondary_threshold, shared_ptr<SimpleSampler> sampler,
    const short num_stripes_per_cycle, const float deletion_cycle,
    const int num_objs) {
  int num_data_exts = 7;
  float num_global_parities = 2;
  float num_local_parities = 2;
  int num_localities = 2;
  std::tuple<shared_ptr<StripeManager>, shared_ptr<EventManager>,
             shared_ptr<ObjectManager>, shared_ptr<ExtentManager>>
      mngrs = create_managers(num_data_exts, num_local_parities,
                              num_global_parities, num_localities, sampler,
                              ext_size, &Extent::get_default_key);
  shared_ptr<StripeManager> stripe_mngr =
      std::get<shared_ptr<StripeManager>>(mngrs);
  shared_ptr<ExtentManager> ext_mngr =
      std::get<shared_ptr<ExtentManager>>(mngrs);
  shared_ptr<ObjectManager> obj_mngr =
      std::get<shared_ptr<ObjectManager>>(mngrs);
  shared_ptr<EventManager> event_mngr =
      std::get<shared_ptr<EventManager>>(mngrs);

  shared_ptr<AbstractStriperDecorator> striper =
      make_shared<StriperWithEC>(make_shared<ExtentStackStriper>(
          make_shared<SimpleStriper>(stripe_mngr, ext_mngr)));
  shared_ptr<AbstractStriperDecorator> gc_striper = striper;
  shared_ptr<SimpleObjectPacker> obj_packer =
      make_shared<RandomizedObjectPacker>(obj_mngr, ext_mngr, object_lst(),
                                          current_extents(), num_objs,
                                          primary_threshold);
  shared_ptr<SimpleGCObjectPacker> gc_obj_packer =
      make_shared<RandomizedGCObjectPacker>(obj_mngr, ext_mngr, object_lst(),
                                            current_extents(), num_objs,
                                            primary_threshold);
  shared_ptr<AbstractExtentStack> extent_stack =
      make_shared<SingleExtentStack<>>(stripe_mngr);
  shared_ptr<AbstractExtentStack> gc_extent_stack =
      make_shared<SingleExtentStack<>>(stripe_mngr);
  shared_ptr<StripingProcessCoordinator> coordinator =
      make_shared<StripingProcessCoordinator>(
          obj_packer, gc_obj_packer, striper, gc_striper, extent_stack,
          gc_extent_stack, stripe_mngr, simul_time);
  /* TODO
  gc_strategy =
  StripeLevelWithExtsGarbageCollectionStrategy(primary_threshold,
  secondary_threshold, coordinator, ext_mngr, gc_striper,
  StripeLevelGCStrategy(primary_threshold, secondary_threshold, coordinator,
  ext_mngr, gc_striper))
      */
  shared_ptr<GarbageCollectionStrategy> gc_strategy = nullptr;
  DataCenter data_center =
      DataCenter(data_center_size, striping_cycle, striper, stripe_mngr,
                 ext_mngr, obj_mngr, event_mngr, gc_strategy, coordinator,
                 simul_time, deletion_cycle);

  return data_center;
}

inline DataCenter randomized_objs_no_exts_config(
    const unsigned long data_center_size, const float striping_cycle,
    const float simul_time, const int ext_size, const short primary_threshold,
    const short secondary_threshold, shared_ptr<SimpleSampler> sampler,
    const short num_stripes_per_cycle, const float deletion_cycle,
    const int num_objs) {
  int num_data_exts = 1;
  float coding_overhead = 18 / 14;
  float num_global_parities = 2 / 14;
  float num_local_parities = 2 / 14;
  int num_localities = 1;
  std::tuple<shared_ptr<StripeManager>, shared_ptr<EventManager>,
             shared_ptr<ObjectManager>, shared_ptr<ExtentManager>>
      mngrs =
          create_managers(num_data_exts, num_local_parities,
                          num_global_parities, num_localities, sampler,
                          ext_size, &Extent::get_default_key, coding_overhead);
  shared_ptr<StripeManager> stripe_mngr =
      std::get<shared_ptr<StripeManager>>(mngrs);
  shared_ptr<ExtentManager> ext_mngr =
      std::get<shared_ptr<ExtentManager>>(mngrs);
  shared_ptr<ObjectManager> obj_mngr =
      std::get<shared_ptr<ObjectManager>>(mngrs);
  shared_ptr<EventManager> event_mngr =
      std::get<shared_ptr<EventManager>>(mngrs);

  shared_ptr<AbstractStriperDecorator> striper =
      make_shared<StriperWithEC>(make_shared<ExtentStackStriper>(
          make_shared<SimpleStriper>(stripe_mngr, ext_mngr)));
  shared_ptr<AbstractStriperDecorator> gc_striper =
      make_shared<StriperWithEC>(make_shared<ExtentStackStriper>(
          make_shared<SimpleStriper>(stripe_mngr, ext_mngr)));

  shared_ptr<SimpleObjectPacker> obj_packer =
      make_shared<RandomizedObjectPacker>(obj_mngr, ext_mngr, object_lst(),
                                          current_extents(), num_objs,
                                          primary_threshold);
  shared_ptr<SimpleGCObjectPacker> gc_obj_packer =
      make_shared<RandomizedGCObjectPacker>(obj_mngr, ext_mngr, object_lst(),
                                            current_extents(), num_objs,
                                            primary_threshold);

  shared_ptr<AbstractExtentStack> extent_stack =
      make_shared<BestEffortExtentStack>(stripe_mngr);
  shared_ptr<AbstractExtentStack> gc_extent_stack =
      make_shared<BestEffortExtentStack>(stripe_mngr);
  shared_ptr<StripingProcessCoordinator> coordinator =
      make_shared<BestEffortStripingProcessCoordinator>(
          obj_packer, gc_obj_packer, striper, gc_striper, extent_stack,
          gc_extent_stack, stripe_mngr, simul_time, get_immortal_key);
  /*TODO
gc_strategy = StripeLevelNoExtsGCStrategy(primary_threshold,
secondary_threshold, coordinator, ext_mngr, stripe_mngr, gc_striper)
  */
  shared_ptr<GarbageCollectionStrategy> gc_strategy = nullptr;
  DataCenter data_center =
      DataCenter(data_center_size, striping_cycle, striper, stripe_mngr,
                 ext_mngr, obj_mngr, event_mngr, gc_strategy, coordinator,
                 simul_time, deletion_cycle);

  return data_center;
}

inline DataCenter randomized_objs_no_exts_mix_objs_config(
    const unsigned long data_center_size, const float striping_cycle,
    const float simul_time, const int ext_size, const short primary_threshold,
    const short secondary_threshold, shared_ptr<SimpleSampler> sampler,
    const short num_stripes_per_cycle, const float deletion_cycle,
    const int num_objs, const int percent_correct) {
  int num_data_exts = 1;
  float coding_overhead = 18 / 14;
  float num_global_parities = 2 / 14;
  float num_local_parities = 2 / 14;
  int num_localities = 1;
  std::tuple<shared_ptr<StripeManager>, shared_ptr<EventManager>,
             shared_ptr<ObjectManager>, shared_ptr<ExtentManager>>
      mngrs =
          create_managers(num_data_exts, num_local_parities,
                          num_global_parities, num_localities, sampler,
                          ext_size, &Extent::get_default_key, coding_overhead);
  shared_ptr<StripeManager> stripe_mngr =
      std::get<shared_ptr<StripeManager>>(mngrs);
  shared_ptr<ExtentManager> ext_mngr =
      std::get<shared_ptr<ExtentManager>>(mngrs);
  shared_ptr<ObjectManager> obj_mngr =
      std::get<shared_ptr<ObjectManager>>(mngrs);
  shared_ptr<EventManager> event_mngr =
      std::get<shared_ptr<EventManager>>(mngrs);

  shared_ptr<AbstractStriperDecorator> striper =
      make_shared<StriperWithEC>(make_shared<ExtentStackStriper>(
          make_shared<SimpleStriper>(stripe_mngr, ext_mngr)));
  shared_ptr<AbstractStriperDecorator> gc_striper = striper;
  current_extents current_exts;
  current_exts.emplace(0, ext_mngr->create_extent());

  shared_ptr<SimpleObjectPacker> obj_packer =
      make_shared<RandomizedObjectPacker>(obj_mngr, ext_mngr, object_lst(),
                                          current_exts, num_objs,
                                          primary_threshold);
  shared_ptr<SimpleGCObjectPacker> gc_obj_packer =
      make_shared<RandomizedGCObjectPacker>(obj_mngr, ext_mngr, object_lst(),
                                            current_exts, num_objs,
                                            primary_threshold);

  shared_ptr<AbstractExtentStack> extent_stack =
      make_shared<SingleExtentStack<>>(stripe_mngr);
  shared_ptr<AbstractExtentStack> gc_extent_stack = extent_stack;
  shared_ptr<StripingProcessCoordinator> coordinator =
      make_shared<StripingProcessCoordinator>(
          obj_packer, gc_obj_packer, striper, gc_striper, extent_stack,
          gc_extent_stack, stripe_mngr, simul_time);
  // TODO
  // gc_strategy = StripeLevelNoExtsGCStrategy(primary_threshold,
  //                                           secondary_threshold,
  //                                           coordinator, ext_mngr,
  //                                           stripe_mngr, gc_striper)
  shared_ptr<GarbageCollectionStrategy> gc_strategy = nullptr;
  DataCenter data_center =
      DataCenter(data_center_size, striping_cycle, striper, stripe_mngr,
                 ext_mngr, obj_mngr, event_mngr, gc_strategy, coordinator,
                 simul_time, deletion_cycle);

  return data_center;
}

inline DataCenter age_based_rand_config_no_exts(
    const unsigned long data_center_size, const float striping_cycle,
    const float simul_time, const int ext_size, const short primary_threshold,
    const short secondary_threshold, shared_ptr<SimpleSampler> sampler,
    const short num_stripes_per_cycle, const float deletion_cycle,
    const int num_objs) {
  int num_data_exts = 1;
  float coding_overhead = 18 / 14;
  float num_global_parities = 2 / 14;
  float num_local_parities = 2 / 14;
  int num_localities = 1;
  std::tuple<shared_ptr<StripeManager>, shared_ptr<EventManager>,
             shared_ptr<ObjectManager>, shared_ptr<ExtentManager>>
      mngrs =
          create_managers(num_data_exts, num_local_parities,
                          num_global_parities, num_localities, sampler,
                          ext_size, &Extent::get_timestamp, coding_overhead);
  shared_ptr<StripeManager> stripe_mngr =
      std::get<shared_ptr<StripeManager>>(mngrs);
  shared_ptr<ExtentManager> ext_mngr =
      std::get<shared_ptr<ExtentManager>>(mngrs);
  shared_ptr<ObjectManager> obj_mngr =
      std::get<shared_ptr<ObjectManager>>(mngrs);
  shared_ptr<EventManager> event_mngr =
      std::get<shared_ptr<EventManager>>(mngrs);

  shared_ptr<AbstractStriperDecorator> striper =
      make_shared<StriperWithEC>(make_shared<ExtentStackStriper>(
          make_shared<SimpleStriper>(stripe_mngr, ext_mngr)));
  shared_ptr<AbstractStriperDecorator> gc_striper = striper;

  shared_ptr<SimpleObjectPacker> obj_packer =
      make_shared<AgeBasedRandomizedObjectPacker>(obj_mngr, ext_mngr, obj_pq(),
                                                  current_extents(), num_objs,
                                                  primary_threshold);
  shared_ptr<SimpleGCObjectPacker> gc_obj_packer =
      make_shared<AgeBasedRandomizedGCObjectPacker>(
          obj_mngr, ext_mngr, obj_pq(), current_extents(), num_objs,
          primary_threshold);

  shared_ptr<AbstractExtentStack> extent_stack =
      make_shared<BestEffortExtentStack>(stripe_mngr);
  shared_ptr<AbstractExtentStack> gc_extent_stack =
      make_shared<BestEffortExtentStack>(stripe_mngr);
  shared_ptr<StripingProcessCoordinator> coordinator =
      make_shared<BestEffortStripingProcessCoordinator>(
          obj_packer, gc_obj_packer, striper, gc_striper, extent_stack,
          gc_extent_stack, stripe_mngr, simul_time, get_timestamp);
  /*TODO
gc_strategy = StripeLevelNoExtsGCStrategy(primary_threshold,
secondary_threshold, coordinator, ext_mngr, stripe_mngr, gc_striper)
  */
  shared_ptr<GarbageCollectionStrategy> gc_strategy = nullptr;
  DataCenter data_center =
      DataCenter(data_center_size, striping_cycle, striper, stripe_mngr,
                 ext_mngr, obj_mngr, event_mngr, gc_strategy, coordinator,
                 simul_time, deletion_cycle);

  return data_center;
}

int default_key() { return 0; }

inline DataCenter generational_config(
    const unsigned long data_center_size, const float striping_cycle,
    const float simul_time, const int ext_size, const short primary_threshold,
    const short secondary_threshold, shared_ptr<SimpleSampler> sampler,
    const short num_stripes_per_cycle, const float deletion_cycle,
    const int num_objs) {
  int num_data_exts = 7;
  float num_global_parities = 2;
  float num_local_parities = 2;
  int num_localities = 2;
  std::tuple<shared_ptr<StripeManager>, shared_ptr<EventManager>,
             shared_ptr<ObjectManager>, shared_ptr<ExtentManager>>
      mngrs = create_managers(num_data_exts, num_local_parities,
                              num_global_parities, num_localities, sampler,
                              ext_size, &Extent::get_generation);
  shared_ptr<StripeManager> stripe_mngr =
      std::get<shared_ptr<StripeManager>>(mngrs);
  shared_ptr<ExtentManager> ext_mngr =
      std::get<shared_ptr<ExtentManager>>(mngrs);
  shared_ptr<ObjectManager> obj_mngr =
      std::get<shared_ptr<ObjectManager>>(mngrs);
  shared_ptr<EventManager> event_mngr =
      std::get<shared_ptr<EventManager>>(mngrs);

  shared_ptr<AbstractStriperDecorator> striper =
      make_shared<StriperWithEC>(make_shared<ExtentStackStriper>(
          make_shared<SimpleStriper>(stripe_mngr, ext_mngr)));
  shared_ptr<AbstractStriperDecorator> gc_striper = striper;

  shared_ptr<SimpleObjectPacker> obj_packer =
      make_shared<GenerationBasedObjectPacker>(obj_mngr, ext_mngr, obj_pq(),
                                               current_extents(), num_objs,
                                               primary_threshold);
  shared_ptr<SimpleGCObjectPacker> gc_obj_packer =
      make_shared<GenerationBasedGCObjectPacker>(obj_mngr, ext_mngr, obj_pq(),
                                                 current_extents(), num_objs,
                                                 primary_threshold);
  shared_ptr<AbstractExtentStack> extent_stack =
      make_shared<BestEffortExtentStack>(stripe_mngr);
  shared_ptr<AbstractExtentStack> gc_extent_stack =
      make_shared<BestEffortExtentStack>(stripe_mngr);
  shared_ptr<StripingProcessCoordinator> coordinator =
      make_shared<BestEffortStripingProcessCoordinator>(
          obj_packer, gc_obj_packer, striper, gc_striper, extent_stack,
          gc_extent_stack, stripe_mngr, simul_time, get_timestamp);
  /*TODO
gc_strategy = StripeLevelNoExtsGCStrategy(primary_threshold,
secondary_threshold, coordinator, ext_mngr, stripe_mngr, gc_striper)
  */
  shared_ptr<GarbageCollectionStrategy> gc_strategy = nullptr;
  DataCenter data_center =
      DataCenter(data_center_size, striping_cycle, striper, stripe_mngr,
                 ext_mngr, obj_mngr, event_mngr, gc_strategy, coordinator,
                 simul_time, deletion_cycle);

  return data_center;
}
#endif // __CONFIGS_H_
