#ifndef __CONFIGS_H_
#define __CONFIGS_H_
#include "object_packer.h"
#include "data_center.h"
#include "stripers.h"
#include "extent_manager.h"
#include "striping_process_coordinator.h"
#include "object_manager.h"
#include "stripe_manager.h"
#include <memory>
#include <variant>
#include "samplers.h"
// typedef int sim_T;
template<class K>
std::tuple<shared_ptr<StripeManager>, shared_ptr<EventManager>, shared_ptr<ObjectManager>, shared_ptr<ExtentManager>> create_managers
	(const int num_data_exts, const int num_local_parities, 
	const int num_global_parities, const int num_localities, const SimpleSampler * sampler, const int ext_size,
	K (Extent::*key_fnc)(),
	const float coding_overhead=0, const bool add_noise = false)
{
	shared_ptr<StripeManager> stripe_mngr;
	if(coding_overhead != 0)
    {   
		stripe_mngr = make_shared<StripeManager>(num_data_exts,
			 num_local_parities, num_global_parities, num_localities, coding_overhead);
	}else
	{
		stripe_mngr = make_shared<StripeManager>(num_data_exts,
			 num_local_parities, num_global_parities, num_localities);
	}
    shared_ptr<EventManager> event_mngr = make_shared<EventManager>();
	shared_ptr<ObjectManager> obj_mngr = make_shared<ObjectManager>(event_mngr, sampler, add_noise);
    shared_ptr<ExtentManager> ext_mngr = key_fnc == make_shared<ExtentManager>(ext_size, key_fnc);
    return std::make_tuple(stripe_mngr, event_mngr, obj_mngr, ext_mngr);

}
template <class sim_T>
DataCenter<Extent *, int, sim_T>  stripe_level_with_no_exts_config(const unsigned long data_center_size,
			const float striping_cycle, const float simul_time, const int ext_size,
			const short primary_threshold, const short secondary_threshold,
			const SimpleSampler * sampler, const short num_stripes_per_cycle,
			const float deletion_cycle, const int num_objs)
{
	int num_data_exts = 1;
    float coding_overhead = 18/14;
    float num_global_parities = 2/14;
    float num_local_parities = 2/14;
    int num_localities = 1;
    std::tuple<shared_ptr<StripeManager>, shared_ptr<EventManager>, shared_ptr<ObjectManager>,
		 shared_ptr<ExtentManager>> mngrs 
		 = create_managers(num_data_exts, num_local_parities, num_global_parities, num_localities, sampler, 
		 ext_size, &Extent::get_default_key ,coding_overhead, false);
	shared_ptr<StripeManager> stripe_mngr = std::get<shared_ptr<StripeManager>>(mngrs);
	shared_ptr<ExtentManager> ext_mngr = std::get<shared_ptr<ExtentManager>>(mngrs);
	shared_ptr<ObjectManager> obj_mngr = std::get<shared_ptr<ObjectManager>>(mngrs);
	shared_ptr<EventManager> event_mngr = std::get<shared_ptr<EventManager>>(mngrs);

    shared_ptr<AbstractStriperDecorator> striper = make_shared<StriperWithEC>(make_shared<ExtentStackStriper>(make_shared<SimpleStriper>(stripe_mngr, ext_mngr)));
    shared_ptr<AbstractStriperDecorator> gc_striper = make_shared<StriperWithEC>(make_shared<ExtentStackStriper>(make_shared<SimpleStriper>(stripe_mngr, ext_mngr)));
    shared_ptr<SimpleObjectPacker> obj_packer = make_shared<SimpleObjectPacker>(obj_mngr, ext_mngr, num_objs, primary_threshold, true);
    shared_ptr<SimpleGCObjectPacker> gc_obj_packer = make_shared<SimpleGCObjectPacker>(obj_mngr, ext_mngr, num_objs, primary_threshold, true);
    shared_ptr<ExtentStack<Extent *, int>> extent_stack = make_shared<SingleExtentStack>(stripe_mngr);
    shared_ptr<ExtentStack<Extent *, int>> gc_extent_stack = make_shared<SingleExtentStack>(stripe_mngr);
 	shared_ptr<StripingProcessCoordinator<Extent *, int, sim_T>> coordinator = make_shared<StripingProcessCoordinator<Extent *, int, sim_T>> (obj_packer, gc_obj_packer, striper, gc_striper,extent_stack, gc_extent_stack, stripe_mngr, simul_time);
	/*TODO
	gc_strategy = StripeLevelNoExtsGCStrategy(primary_threshold, secondary_threshold, coordinator, ext_mngr, stripe_mngr, gc_striper)
	*/
	shared_ptr<GarbageCollectionStrategy> gc_strategy = nullptr;
    DataCenter<Extent *, int, sim_T>  data_center = DataCenter<Extent *, int, sim_T>(data_center_size, striping_cycle, striper, stripe_mngr, ext_mngr, obj_mngr, event_mngr, gc_strategy, coordinator, simul_time, deletion_cycle);

    return data_center;
}
template <class sim_T>
DataCenter<Extent *, int, sim_T>  no_exts_mix_objs_config(const unsigned long data_center_size,
			const float striping_cycle, const float simul_time, const int ext_size,
			const short primary_threshold, const short secondary_threshold,
			const SimpleSampler * sampler, const short num_stripes_per_cycle,
			const float deletion_cycle, const int num_objs)
{
    int num_data_exts = 1;
    float coding_overhead = 18/14;
    float num_global_parities = 2/14;
    float num_local_parities = 2/14;
    int num_localities = 1;
	std::tuple<shared_ptr<StripeManager>, shared_ptr<EventManager>, shared_ptr<ObjectManager>,
			shared_ptr<ExtentManager>> mngrs 
			= create_managers(num_data_exts, num_local_parities, num_global_parities, num_localities, sampler, 
			ext_size,&Extent::get_default_key, coding_overhead);
	shared_ptr<StripeManager> stripe_mngr = std::get<shared_ptr<StripeManager>>(mngrs);
	shared_ptr<ExtentManager> ext_mngr = std::get<shared_ptr<ExtentManager>>(mngrs);
	shared_ptr<ObjectManager> obj_mngr = std::get<shared_ptr<ObjectManager>>(mngrs);
	shared_ptr<EventManager> event_mngr = std::get<shared_ptr<EventManager>>(mngrs);
 	shared_ptr<AbstractStriperDecorator>  striper = make_shared<StriperWithEC>(make_shared<ExtentStackStriper>(make_shared<SimpleStriper>(stripe_mngr, ext_mngr)));

    shared_ptr<AbstractStriperDecorator>  gc_striper = make_shared<StriperWithEC>(make_shared<ExtentStackStriper>(make_shared<SimpleStriper>(stripe_mngr, ext_mngr)));
	shared_ptr<unordered_map<Extent_Object*, double > > obj_pool = make_shared<unordered_map<Extent_Object*, double >>();
    shared_ptr<map<int, Extent *>>  current_exts = make_shared<map<int, Extent *>>();
    current_exts->emplace(0, ext_mngr->create_extent());
    shared_ptr<SimpleObjectPacker> obj_packer = make_shared<MixedObjObjectPacker>(obj_mngr, ext_mngr, num_objs, primary_threshold,false, obj_pool, current_exts);
    shared_ptr<SimpleGCObjectPacker> gc_obj_packer = make_shared<MixedObjGCObjectPacker>(obj_mngr, ext_mngr, num_objs, primary_threshold,false, obj_pool, current_exts);
    shared_ptr<ExtentStack<Extent *, int>> extent_stack = make_shared<SingleExtentStack>(stripe_mngr);
    shared_ptr<ExtentStack<Extent *, int>> gc_extent_stack = extent_stack;
	shared_ptr<StripingProcessCoordinator<Extent *, int, sim_T>> coordinator = make_shared<StripingProcessCoordinator<Extent *, int, sim_T>>(obj_packer, gc_obj_packer, striper, gc_striper,extent_stack, gc_extent_stack, stripe_mngr, simul_time);
    /* TODO 
	gc_strategy =MixObjStripeLevelStrategy(primary_threshold, secondary_threshold, coordinator, ext_mngr, gc_striper)
	*/
	shared_ptr<GarbageCollectionStrategy> gc_strategy = nullptr;
    DataCenter<Extent *, int, sim_T>  data_center = DataCenter<Extent *, int, sim_T>(data_center_size, striping_cycle, striper, stripe_mngr, ext_mngr, obj_mngr, event_mngr, gc_strategy, coordinator, simul_time, deletion_cycle);

    return data_center;
}
template <class sim_T>
DataCenter<Extent *, int, sim_T>  stripe_level_with_extents_separate_pools_config(const unsigned long data_center_size,
			const float striping_cycle, const float simul_time, const int ext_size,
			const short primary_threshold, const short secondary_threshold,
			const SimpleSampler * sampler, const short num_stripes_per_cycle,
			const float deletion_cycle, const int num_objs)
{
	int num_data_exts = 7;
    float num_global_parities = 2;
    float num_local_parities = 2;
    int num_localities = 2;
	std::tuple<shared_ptr<StripeManager>, shared_ptr<EventManager>, shared_ptr<ObjectManager>,
			shared_ptr<ExtentManager>> mngrs 
			= create_managers(num_data_exts, num_local_parities, num_global_parities, num_localities, sampler, 
			ext_size, &Extent::get_default_key);
	shared_ptr<StripeManager> stripe_mngr = std::get<shared_ptr<StripeManager>>(mngrs);
	shared_ptr<ExtentManager> ext_mngr = std::get<shared_ptr<ExtentManager>>(mngrs);
	shared_ptr<ObjectManager> obj_mngr = std::get<shared_ptr<ObjectManager>>(mngrs);
	shared_ptr<EventManager> event_mngr = std::get<shared_ptr<EventManager>>(mngrs);

 	shared_ptr<AbstractStriperDecorator>  striper = make_shared<StriperWithEC>(make_shared<ExtentStackStriper>(make_shared<SimpleStriper>(stripe_mngr, ext_mngr)));
    shared_ptr<AbstractStriperDecorator>  gc_striper = striper;
    shared_ptr<SimpleObjectPacker> obj_packer = make_shared<SimpleObjectPacker>(obj_mngr, ext_mngr, num_objs, primary_threshold,false);
    shared_ptr<SimpleGCObjectPacker> gc_obj_packer = make_shared<SimpleGCObjectPacker>(obj_mngr, ext_mngr, num_objs, primary_threshold,false);
    shared_ptr<ExtentStack<Extent *, int>> extent_stack = make_shared<SingleExtentStack>(stripe_mngr);
    shared_ptr<ExtentStack<Extent *, int>> gc_extent_stack = extent_stack;
	shared_ptr<StripingProcessCoordinator<Extent *, int, sim_T>> coordinator = make_shared<StripingProcessCoordinator<Extent *, int, sim_T>>(obj_packer, gc_obj_packer, striper, gc_striper,extent_stack, gc_extent_stack, stripe_mngr, simul_time);
    /*TODO
	gc_strategy = StripeLevelWithExtsGCStrategy(primary_threshold, secondary_threshold, coordinator, ext_mngr, gc_striper)
	*/
	shared_ptr<GarbageCollectionStrategy> gc_strategy = nullptr;
    DataCenter<Extent *, int, sim_T>  data_center = DataCenter<Extent *, int, sim_T> (data_center_size, striping_cycle, striper, stripe_mngr, ext_mngr, obj_mngr, event_mngr, gc_strategy, coordinator, simul_time, deletion_cycle);
    return data_center;
}

template <class sim_T>
DataCenter<Extent *, int, sim_T>  stripe_level_with_extents_separate_pools_efficient_config(const unsigned long data_center_size,
			const float striping_cycle, const float simul_time, const int ext_size,
			const short primary_threshold, const short secondary_threshold,
			const SimpleSampler * sampler, const short num_stripes_per_cycle,
			const float deletion_cycle, const int num_objs)
{
	int num_data_exts = 7;
    float num_global_parities = 2;
    float num_local_parities = 2;
    int num_localities = 2;
	std::tuple<shared_ptr<StripeManager>, shared_ptr<EventManager>, shared_ptr<ObjectManager>,
			shared_ptr<ExtentManager>> mngrs 
			= create_managers(num_data_exts, num_local_parities, num_global_parities, num_localities, sampler, 
			ext_size, &Extent::get_default_key);
	shared_ptr<StripeManager> stripe_mngr = std::get<shared_ptr<StripeManager>>(mngrs);
	shared_ptr<ExtentManager> ext_mngr = std::get<shared_ptr<ExtentManager>>(mngrs);
	shared_ptr<ObjectManager> obj_mngr = std::get<shared_ptr<ObjectManager>>(mngrs);
 	shared_ptr<EventManager> event_mngr = std::get<shared_ptr<EventManager>>(mngrs);

	shared_ptr<AbstractStriperDecorator>  striper = make_shared<EfficientStriperWithEC>(make_shared<ExtentStackStriper>(make_shared<SimpleStriper>(stripe_mngr, ext_mngr)));
    shared_ptr<AbstractStriperDecorator>  gc_striper = striper;
    shared_ptr<SimpleObjectPacker> obj_packer = make_shared<SimpleObjectPacker>(obj_mngr, ext_mngr, num_objs);
    shared_ptr<SimpleGCObjectPacker> gc_obj_packer = make_shared<SimpleGCObjectPacker>(obj_mngr, ext_mngr, num_objs);
    shared_ptr<ExtentStack<Extent *, int>> extent_stack = make_shared<SingleExtentStack>(stripe_mngr);
    shared_ptr<ExtentStack<Extent *, int>> gc_extent_stack = extent_stack;
	shared_ptr<StripingProcessCoordinator<Extent *, int, sim_T>> coordinator = make_shared<StripingProcessCoordinator<Extent *, int, sim_T>>(obj_packer, gc_obj_packer, striper, gc_striper,extent_stack, gc_extent_stack, stripe_mngr, simul_time);
    /*TODO
	gc_strategy = StripeLevelWithExtsGCStrategy(primary_threshold, secondary_threshold, coordinator, ext_mngr, gc_striper)
	*/
	shared_ptr<GarbageCollectionStrategy> gc_strategy = nullptr;
    DataCenter<Extent *, int, sim_T>  data_center = DataCenter<Extent *, int, sim_T> (data_center_size, striping_cycle, striper, stripe_mngr, ext_mngr, obj_mngr, event_mngr, gc_strategy, coordinator, simul_time, deletion_cycle);
    return data_center;
}

template <class sim_T>
sim_T get_timestamp()
{
	return (sim_T)TIME;
}

template <class sim_T>
DataCenter<Extent *, int, sim_T>  age_based_config_no_exts(const unsigned long data_center_size,
			const float striping_cycle, const float simul_time, const int ext_size,
			const short primary_threshold, const short secondary_threshold,
			const SimpleSampler * sampler, const short num_stripes_per_cycle,
			const float deletion_cycle, const int num_objs)
{
	int num_data_exts = 1;
    float coding_overhead = 18/14;
    float num_global_parities = 2/14;
    float num_local_parities = 2/14;
    int num_localities = 1;
	time_t (Extent::*key_fnc)() = &Extent::get_timestamp;
    std::tuple<shared_ptr<StripeManager>, shared_ptr<EventManager>, shared_ptr<ObjectManager>,
		 shared_ptr<ExtentManager>> mngrs 
		 = create_managers(num_data_exts, num_local_parities, num_global_parities, num_localities, sampler, 
		 ext_size, key_fnc, coding_overhead);
	shared_ptr<StripeManager> stripe_mngr = std::get<shared_ptr<StripeManager>>(mngrs);
	shared_ptr<ExtentManager> ext_mngr = std::get<shared_ptr<ExtentManager>>(mngrs);
	shared_ptr<ObjectManager> obj_mngr = std::get<shared_ptr<ObjectManager>>(mngrs);
	shared_ptr<EventManager> event_mngr = std::get<shared_ptr<EventManager>>(mngrs);

    shared_ptr<AbstractStriperDecorator> striper = make_shared<StriperWithEC>(make_shared<ExtentStackStriper>(make_shared<SimpleStriper>(stripe_mngr, ext_mngr)));
    shared_ptr<AbstractStriperDecorator> gc_striper = striper;
    shared_ptr<SimpleObjectPacker> obj_packer = make_shared<AgeBasedObjectPacker>(obj_mngr, ext_mngr, num_objs, primary_threshold);
    shared_ptr<SimpleGCObjectPacker> gc_obj_packer = make_shared<AgeBasedGCObjectPacker>(obj_mngr, ext_mngr, num_objs, primary_threshold);
    shared_ptr<ExtentStack<Extent *, int>> extent_stack = make_shared<BestEffortExtentStack>(stripe_mngr);
    shared_ptr<ExtentStack<Extent *, int>> gc_extent_stack = make_shared<BestEffortExtentStack>(stripe_mngr);
 	shared_ptr<StripingProcessCoordinator<Extent *, int, sim_T>> coordinator = BestEffortStripingProcessCoordinator<Extent *, int,sim_T> (obj_packer, gc_obj_packer, striper, gc_striper,extent_stack, gc_extent_stack, stripe_mngr, simul_time, get_timestamp<int>);
	/*TODO
	gc_strategy = StripeLevelNoExtsGCStrategy(primary_threshold, secondary_threshold, coordinator, ext_mngr, stripe_mngr, gc_striper)
	*/
	shared_ptr<GarbageCollectionStrategy> gc_strategy = nullptr;
    DataCenter<Extent *, int, sim_T>  data_center = DataCenter<Extent *, int, sim_T> (data_center_size, striping_cycle, striper, stripe_mngr, ext_mngr, obj_mngr, event_mngr, gc_strategy, coordinator, simul_time, deletion_cycle);

    return data_center;
}

template <class sim_T>
sim_T get_time()
{
	return (sim_T)TIME;
}


template <class sim_T>
DataCenter<Extent *, int, sim_T>  age_based_config(const unsigned long data_center_size,
			const float striping_cycle, const float simul_time, const int ext_size,
			const short primary_threshold, const short secondary_threshold,
			const SimpleSampler * sampler, const short num_stripes_per_cycle,
			const float deletion_cycle, const int num_objs)
{
	int num_data_exts = 7;
    float num_global_parities = 2;
    float num_local_parities = 2;
    int num_localities = 2;
	time_t (Extent::*key_fnc)() = &Extent::get_timestamp;
	std::tuple<shared_ptr<StripeManager>, shared_ptr<EventManager>, shared_ptr<ObjectManager>,
			shared_ptr<ExtentManager>> mngrs 
			= create_managers(num_data_exts, num_local_parities, num_global_parities, num_localities, sampler, 
			ext_size, key_fnc);
	shared_ptr<StripeManager> stripe_mngr = std::get<shared_ptr<StripeManager>>(mngrs);
	shared_ptr<ExtentManager> ext_mngr = std::get<shared_ptr<ExtentManager>>(mngrs);
	shared_ptr<ObjectManager> obj_mngr = std::get<shared_ptr<ObjectManager>>(mngrs);
 	shared_ptr<EventManager> event_mngr = std::get<shared_ptr<EventManager>>(mngrs);
	shared_ptr<AbstractStriperDecorator>  striper = make_shared<StriperWithEC>(make_shared<ExtentStackStriper>(make_shared<SimpleStriper>(stripe_mngr, ext_mngr)));
    shared_ptr<AbstractStriperDecorator>  gc_striper = striper;
    shared_ptr<SimpleObjectPacker> obj_packer = make_shared<AgeBasedObjectPacker>(obj_mngr, ext_mngr, num_objs);
    shared_ptr<SimpleGCObjectPacker> gc_obj_packer = make_shared<AgeBasedGCObjectPacker>(obj_mngr, ext_mngr, num_objs);
    shared_ptr<ExtentStack<Extent *, int>> extent_stack = make_shared<BestEffortExtentStack>(stripe_mngr);
    shared_ptr<ExtentStack<Extent *, int>> gc_extent_stack = extent_stack;
 	shared_ptr<StripingProcessCoordinator<Extent *, int, sim_T>> coordinator = make_shared<BestEffortStripingProcessCoordinator<Extent *, int, sim_T>>(obj_packer, gc_obj_packer, striper, gc_striper,extent_stack, gc_extent_stack, stripe_mngr, simul_time, get_time<int>);
	/*TODO
	gc_strategy = StripeLevelNoExtsGCStrategy(primary_threshold, secondary_threshold, coordinator, ext_mngr, stripe_mngr, gc_striper)
	*/
	shared_ptr<GarbageCollectionStrategy> gc_strategy = nullptr;
    DataCenter<Extent *, int, sim_T>  data_center = DataCenter<Extent *, int, sim_T> (data_center_size, striping_cycle, striper, stripe_mngr, ext_mngr, obj_mngr, event_mngr, gc_strategy, coordinator, simul_time, deletion_cycle);
    return data_center;
}


template <class sim_T>
DataCenter<Extent *, int, sim_T>  size_based_stripe_level_no_exts_baseline_config(const unsigned long data_center_size,
			const float striping_cycle, const float simul_time, const int ext_size,
			const short primary_threshold, const short secondary_threshold,
			const SimpleSampler * sampler, const short num_stripes_per_cycle,
			const float deletion_cycle, const int num_objs)
{
	int num_data_exts = 1;
    float coding_overhead = 18/14;
    float num_global_parities = 2/14;
    float num_local_parities = 2/14;
    int num_localities = 1;
    std::tuple<shared_ptr<StripeManager>, shared_ptr<EventManager>, shared_ptr<ObjectManager>,
		 shared_ptr<ExtentManager>> mngrs 
		 = create_managers(num_data_exts, num_local_parities, num_global_parities, num_localities, sampler, 
		 ext_size, &Extent::get_default_key, coding_overhead);
	shared_ptr<StripeManager> stripe_mngr = std::get<shared_ptr<StripeManager>>(mngrs);
	shared_ptr<ExtentManager> ext_mngr = std::get<shared_ptr<ExtentManager>>(mngrs);
	shared_ptr<ObjectManager> obj_mngr = std::get<shared_ptr<ObjectManager>>(mngrs);
	shared_ptr<EventManager> event_mngr = std::get<shared_ptr<EventManager>>(mngrs);
    
	shared_ptr<AbstractStriperDecorator> striper = make_shared<StriperWithEC>(make_shared<ExtentStackStriper>(make_shared<SimpleStriper>(stripe_mngr, ext_mngr)));
    shared_ptr<AbstractStriperDecorator> gc_striper = striper;
    shared_ptr<SimpleObjectPacker> obj_packer = make_shared<SizeBasedObjectPackerBaseline>(obj_mngr, ext_mngr, num_objs, primary_threshold);
    shared_ptr<SimpleGCObjectPacker> gc_obj_packer = make_shared<SizeBasedGCObjectPackerBaseline>(obj_mngr, ext_mngr, num_objs, primary_threshold);
    shared_ptr<ExtentStack<Extent *, int>> extent_stack = make_shared<SingleExtentStack>(stripe_mngr);
    shared_ptr<ExtentStack<Extent *, int>> gc_extent_stack = make_shared<SingleExtentStack>(stripe_mngr);
 	shared_ptr<StripingProcessCoordinator<Extent *, int, sim_T>> coordinator = make_shared<StripingProcessCoordinator<Extent *, int,sim_T>> (obj_packer, gc_obj_packer, striper, gc_striper,extent_stack, gc_extent_stack, stripe_mngr, simul_time);
	/*TODO
	gc_strategy = StripeLevelNoExtsGCStrategy(primary_threshold, secondary_threshold, coordinator, ext_mngr, stripe_mngr, gc_striper)
	*/
	shared_ptr<GarbageCollectionStrategy> gc_strategy = nullptr;
    DataCenter<Extent *, int, sim_T>  data_center = DataCenter<Extent *, int, sim_T> (data_center_size, striping_cycle, striper, stripe_mngr, ext_mngr, obj_mngr, event_mngr, gc_strategy, coordinator, simul_time, deletion_cycle);

    return data_center;
}


template <class sim_T>
DataCenter<Extent *, int, sim_T>  size_based_stripe_level_no_exts_smaller_obj_config(const unsigned long data_center_size,
			const float striping_cycle, const float simul_time, const int ext_size,
			const short primary_threshold, const short secondary_threshold,
			const SimpleSampler * sampler, const short num_stripes_per_cycle,
			const float deletion_cycle, const int num_objs)
{
	int num_data_exts = 1;
    float coding_overhead = 18/14;
    float num_global_parities = 2/14;
    float num_local_parities = 2/14;
    int num_localities = 1;
    std::tuple<shared_ptr<StripeManager>, shared_ptr<EventManager>, shared_ptr<ObjectManager>,
		 shared_ptr<ExtentManager>> mngrs 
		 = create_managers(num_data_exts, num_local_parities, num_global_parities, num_localities, sampler, 
		 ext_size, &Extent::get_default_key, coding_overhead);
	shared_ptr<StripeManager> stripe_mngr = std::get<shared_ptr<StripeManager>>(mngrs);
	shared_ptr<ExtentManager> ext_mngr = std::get<shared_ptr<ExtentManager>>(mngrs);
	shared_ptr<ObjectManager> obj_mngr = std::get<shared_ptr<ObjectManager>>(mngrs);
	shared_ptr<EventManager> event_mngr = std::get<shared_ptr<EventManager>>(mngrs);

    shared_ptr<AbstractStriperDecorator> striper = make_shared<StriperWithEC>(make_shared<ExtentStackStriper>(make_shared<SimpleStriper>(stripe_mngr, ext_mngr)));
    shared_ptr<AbstractStriperDecorator> gc_striper = striper;
    shared_ptr<SimpleObjectPacker> obj_packer = make_shared<SizeBasedObjectPackerSmallerObj>(obj_mngr, ext_mngr, num_objs, primary_threshold, true);
    shared_ptr<SimpleGCObjectPacker> gc_obj_packer = make_shared<SizeBasedGCObjectPackerSmallerObj>(obj_mngr, ext_mngr, num_objs, primary_threshold, true);
    shared_ptr<ExtentStack<Extent *, int>> extent_stack = make_shared<SingleExtentStack>(stripe_mngr);
    shared_ptr<ExtentStack<Extent *, int>> gc_extent_stack = make_shared<SingleExtentStack>(stripe_mngr);
 	shared_ptr<StripingProcessCoordinator<Extent *, int, sim_T>> coordinator = StripingProcessCoordinator<Extent *, int,sim_T> (obj_packer, gc_obj_packer, striper, gc_striper,extent_stack, gc_extent_stack, stripe_mngr, simul_time);
	/*TODO
	gc_strategy = StripeLevelNoExtsGCStrategy(primary_threshold, secondary_threshold, coordinator, ext_mngr, stripe_mngr, gc_striper)
	*/
	shared_ptr<GarbageCollectionStrategy> gc_strategy = nullptr;

    DataCenter<Extent *, int, sim_T>  data_center = DataCenter<Extent *, int, sim_T> (data_center_size, striping_cycle, striper, stripe_mngr, ext_mngr, obj_mngr, event_mngr, gc_strategy, coordinator, simul_time, deletion_cycle);

    return data_center;
}

template <class sim_T>
DataCenter<extent_stack_ext_lst, int, sim_T>  size_based_stripe_level_no_exts_dynamic_strategy_config(const unsigned long data_center_size,
			const float striping_cycle, const float simul_time, const int ext_size,
			const short primary_threshold, const short secondary_threshold,
			const SimpleSampler * sampler, const short num_stripes_per_cycle,
			const float deletion_cycle, const int num_objs)
{
	int num_data_exts = 1;
    float coding_overhead = 18/14;
    float num_global_parities = 2/14;
    float num_local_parities = 2/14;
    int num_localities = 1;
    std::tuple<shared_ptr<StripeManager>, shared_ptr<EventManager>, shared_ptr<ObjectManager>,
		 shared_ptr<ExtentManager>> mngrs 
		 = create_managers(num_data_exts, num_local_parities, num_global_parities, num_localities, sampler, 
		 ext_size, &Extent::get_default_key, coding_overhead);
	shared_ptr<StripeManager> stripe_mngr = std::get<shared_ptr<StripeManager>>(mngrs);
	shared_ptr<ExtentManager> ext_mngr = std::get<shared_ptr<ExtentManager>>(mngrs);
	shared_ptr<ObjectManager> obj_mngr = std::get<shared_ptr<ObjectManager>>(mngrs);
	shared_ptr<EventManager> event_mngr = std::get<shared_ptr<EventManager>>(mngrs);

    shared_ptr<AbstractStriperDecorator> striper = make_shared<StriperWithEC>(make_shared<ExtentStackStriper>(make_shared<SimpleStriper>(stripe_mngr, ext_mngr)));
    shared_ptr<AbstractStriperDecorator> gc_striper = striper;
    shared_ptr<SimpleObjectPacker> obj_packer = make_shared<SizeBasedObjectPackerDynamicStrategy>(obj_mngr, ext_mngr, num_objs, primary_threshold, true);
    shared_ptr<SimpleGCObjectPacker> gc_obj_packer = make_shared<SizeBasedGCObjectPackerDynamicStrategy>(obj_mngr, ext_mngr, num_objs, primary_threshold, true);
    shared_ptr<ExtentStack<extent_stack_ext_lst, int>> extent_stack = make_shared<WholeObjectExtentStack<extent_stack_ext_lst, int>>(stripe_mngr);
    shared_ptr<ExtentStack<extent_stack_ext_lst, int>> gc_extent_stack = make_shared<WholeObjectExtentStack<extent_stack_ext_lst, int>>(stripe_mngr);
 	StripingProcessCoordinator<extent_stack_ext_lst, int, sim_T> coordinator = StripingProcessCoordinator<extent_stack_ext_lst, int,sim_T> (obj_packer, gc_obj_packer, striper, gc_striper,extent_stack, gc_extent_stack, stripe_mngr, simul_time);
	/*TODO
    gc_strategy = StripeLevelNoExtsGarbageCollectionStrategy(primary_threshold, secondary_threshold, coordinator, ext_mngr, stripe_mngr, gc_striper, StripeLevelGCStrategy(primary_threshold, secondary_threshold, coordinator, ext_mngr, gc_striper))
	*/
	shared_ptr<GarbageCollectionStrategy> gc_strategy = nullptr;
    DataCenter<extent_stack_ext_lst, int, sim_T>  data_center = DataCenter<extent_stack_ext_lst, int, sim_T> (data_center_size, striping_cycle, striper, stripe_mngr, ext_mngr, obj_mngr, event_mngr, gc_strategy, coordinator, simul_time, deletion_cycle);

    return data_center;
}



template <class sim_T>
DataCenter<extent_stack_ext_lst, int, sim_T> size_based_whole_obj_config(const unsigned long data_center_size,
			const float striping_cycle, const float simul_time, const int ext_size,
			const short primary_threshold, const short secondary_threshold,
			const SimpleSampler * sampler, const short num_stripes_per_cycle,
			const float deletion_cycle, const int num_objs)
{
	int num_data_exts = 7;
    float num_global_parities = 2;
    float num_local_parities = 2;
    int num_localities = 2;
	std::tuple<shared_ptr<StripeManager>, shared_ptr<EventManager>, shared_ptr<ObjectManager>,
			shared_ptr<ExtentManager>> mngrs 
			= create_managers(num_data_exts, num_local_parities, num_global_parities, num_localities, sampler, 
			ext_size, &Extent::get_default_key);
	shared_ptr<StripeManager> stripe_mngr = std::get<shared_ptr<StripeManager>>(mngrs);
	shared_ptr<ExtentManager> ext_mngr = std::get<shared_ptr<ExtentManager>>(mngrs);
	shared_ptr<ObjectManager> obj_mngr = std::get<shared_ptr<ObjectManager>>(mngrs);
	shared_ptr<EventManager> event_mngr = std::get<shared_ptr<EventManager>>(mngrs);

 	shared_ptr<AbstractStriperDecorator>  striper = make_shared<StriperWithEC>(make_shared<ExtentStackStriper>(make_shared<SimpleStriper>(stripe_mngr, ext_mngr)));
    shared_ptr<AbstractStriperDecorator>  gc_striper = striper;
    shared_ptr<SimpleObjectPacker> obj_packer = make_shared<SizeBasedObjectPackerSmallerWholeObjFillGap>(obj_mngr, ext_mngr, num_objs, primary_threshold,true);
    shared_ptr<SimpleGCObjectPacker> gc_obj_packer = make_shared<SizeBasedGCObjectPackerSmallerWholeObjFillGap>(obj_mngr, ext_mngr, num_objs, primary_threshold,true);
    shared_ptr<ExtentStack<extent_stack_ext_lst, int>> extent_stack = make_shared<WholeObjectExtentStack<extent_stack_ext_lst, int>>(stripe_mngr);
    shared_ptr<ExtentStack<extent_stack_ext_lst, int>> gc_extent_stack = make_shared<WholeObjectExtentStack<extent_stack_ext_lst, int>>(stripe_mngr);
	StripingProcessCoordinator<extent_stack_ext_lst, int, sim_T> coordinator = StripingProcessCoordinator<extent_stack_ext_lst, int, sim_T> (obj_packer, gc_obj_packer, striper, gc_striper,extent_stack, gc_extent_stack, stripe_mngr, simul_time);
    /*TODO
    gc_strategy = StripeLevelNoExtsGarbageCollectionStrategy(primary_threshold, secondary_threshold, coordinator, ext_mngr, stripe_mngr, gc_striper, StripeLevelGCStrategy(primary_threshold, secondary_threshold, coordinator, ext_mngr, gc_striper))
	*/
	shared_ptr<GarbageCollectionStrategy> gc_strategy = nullptr;
    DataCenter<extent_stack_ext_lst, int, sim_T> data_center = DataCenter<extent_stack_ext_lst, int, sim_T>(data_center_size, striping_cycle, striper, stripe_mngr, ext_mngr, obj_mngr, event_mngr, gc_strategy, coordinator, simul_time, deletion_cycle);
    return data_center;
}


template <class sim_T>
DataCenter<extent_stack_ext_lst, int, sim_T>  size_based_stripe_level_no_exts_larger_whole_obj_config(const unsigned long data_center_size,
			const float striping_cycle, const float simul_time, const int ext_size,
			const short primary_threshold, const short secondary_threshold,
			const SimpleSampler * sampler, const short num_stripes_per_cycle,
			const float deletion_cycle, const int num_objs)
{
	int num_data_exts = 1;
    float coding_overhead = 18/14;
    float num_global_parities = 2/14;
    float num_local_parities = 2/14;
    int num_localities = 1;
    std::tuple<shared_ptr<StripeManager>, shared_ptr<EventManager>, shared_ptr<ObjectManager>,
		 shared_ptr<ExtentManager>> mngrs 
		 = create_managers(num_data_exts, num_local_parities, num_global_parities, num_localities, sampler, 
		 ext_size, &Extent::get_default_key, coding_overhead);
	shared_ptr<StripeManager> stripe_mngr = std::get<shared_ptr<StripeManager>>(mngrs);
	shared_ptr<ExtentManager> ext_mngr = std::get<shared_ptr<ExtentManager>>(mngrs);
	shared_ptr<ObjectManager> obj_mngr = std::get<shared_ptr<ObjectManager>>(mngrs);
	shared_ptr<EventManager> event_mngr = std::get<shared_ptr<EventManager>>(mngrs);

    shared_ptr<AbstractStriperDecorator> striper = make_shared<StriperWithEC>(make_shared<ExtentStackStriper>(make_shared<SimpleStriper>(stripe_mngr, ext_mngr)));
    shared_ptr<AbstractStriperDecorator> gc_striper = striper;
    shared_ptr<SimpleObjectPacker> obj_packer = make_shared<SizeBasedObjectPackerLargerWholeObj>(obj_mngr, ext_mngr, num_objs, primary_threshold);
    shared_ptr<SimpleGCObjectPacker> gc_obj_packer = make_shared<SizeBasedGCObjectPackerLargerWholeObj>(obj_mngr, ext_mngr, num_objs, primary_threshold);
    shared_ptr<ExtentStack<extent_stack_ext_lst, int>> extent_stack = make_shared<WholeObjectExtentStack<extent_stack_ext_lst, int>>(stripe_mngr);
    shared_ptr<ExtentStack<extent_stack_ext_lst, int>> gc_extent_stack = make_shared<WholeObjectExtentStack<extent_stack_ext_lst, int>>(stripe_mngr);
 	StripingProcessCoordinator<extent_stack_ext_lst, int, sim_T> coordinator = StripingProcessCoordinator<extent_stack_ext_lst, int,sim_T> (obj_packer, gc_obj_packer, striper, gc_striper,extent_stack, gc_extent_stack, stripe_mngr, simul_time);
	/*TODO
    gc_strategy = StripeLevelNoExtsGarbageCollectionStrategy(primary_threshold, secondary_threshold, coordinator, ext_mngr, stripe_mngr, gc_striper, StripeLevelGCStrategy(primary_threshold, secondary_threshold, coordinator, ext_mngr, gc_striper))
	*/
	shared_ptr<GarbageCollectionStrategy> gc_strategy = nullptr;
    DataCenter<extent_stack_ext_lst, int, sim_T>  data_center = DataCenter<extent_stack_ext_lst, int, sim_T> (data_center_size, striping_cycle, striper, stripe_mngr, ext_mngr, obj_mngr, event_mngr, gc_strategy, coordinator, simul_time, deletion_cycle);

    return data_center;
}





int get_immortal_key()
{
	return 0;
}
    

/*not passing percent correct in
percent_correct = 90
percent_correct = 80
percent_correct = 70
percent_correct = 60*/
template <class sim_T>
DataCenter<Extent *, int, sim_T>  mortal_immortal_no_exts_config(const unsigned long data_center_size,
			const float striping_cycle, const float simul_time, const int ext_size,
			const short primary_threshold, const short secondary_threshold,
			const SimpleSampler * sampler, const short num_stripes_per_cycle,
			const float deletion_cycle, const int num_objs, const int percent_correct)
{
	int num_data_exts = 1;
    float coding_overhead = 18/14;
    float num_global_parities = 2/14;
    float num_local_parities = 2/14;
    int num_localities = 1;
    std::tuple<shared_ptr<StripeManager>, shared_ptr<EventManager>, shared_ptr<ObjectManager>,
		 shared_ptr<ExtentManager>> mngrs 
		 = create_managers(num_data_exts, num_local_parities, num_global_parities, num_localities, sampler, 
		 ext_size, &Extent::get_default_key, coding_overhead);
	shared_ptr<StripeManager> stripe_mngr = std::get<shared_ptr<StripeManager>>(mngrs);
	shared_ptr<ExtentManager> ext_mngr = std::get<shared_ptr<ExtentManager>>(mngrs);
	shared_ptr<ObjectManager> obj_mngr = std::get<shared_ptr<ObjectManager>>(mngrs);
	shared_ptr<EventManager> event_mngr = std::get<shared_ptr<EventManager>>(mngrs);

    shared_ptr<AbstractStriperDecorator> striper = make_shared<StriperWithEC>(make_shared<ExtentStackStriper>(make_shared<SimpleStriper>(stripe_mngr, ext_mngr)));
    shared_ptr<AbstractStriperDecorator> gc_striper = striper;

    shared_ptr<SimpleObjectPacker> obj_packer = make_shared<MortalImmortalObjectPacker>(obj_mngr, ext_mngr, num_objs, percent_correct);
    shared_ptr<SimpleGCObjectPacker> gc_obj_packer = make_shared<MortalImmortalGCObjectPacker>(obj_mngr, ext_mngr, num_objs, percent_correct);
    
	shared_ptr<ExtentStack<Extent *, int>> extent_stack = make_shared<BestEffortExtentStack>(stripe_mngr);
    shared_ptr<ExtentStack<Extent *, int>> gc_extent_stack = make_shared<BestEffortExtentStack>(stripe_mngr);
 	shared_ptr<StripingProcessCoordinator<Extent *, int, sim_T>> coordinator = make_shared<StripingProcessCoordinator<Extent *, int, sim_T>>(obj_packer, gc_obj_packer, striper, gc_striper,extent_stack, gc_extent_stack, stripe_mngr, simul_time, get_immortal_key);
	/*TODO
    gc_strategy = StripeLevelNoExtsGCStrategy(primary_threshold, secondary_threshold, coordinator, ext_mngr, stripe_mngr, gc_striper)
	*/
	shared_ptr<GarbageCollectionStrategy> gc_strategy = nullptr;
    DataCenter<Extent *, int, sim_T>  data_center = DataCenter<Extent *, int, sim_T> (data_center_size, striping_cycle, striper, stripe_mngr, ext_mngr, obj_mngr, event_mngr, gc_strategy, coordinator, simul_time, deletion_cycle);

    return data_center;
}


template <class sim_T>
DataCenter<Extent *, int, sim_T>  randomized_ext_placement_joined_pools_config(const unsigned long data_center_size,
			const float striping_cycle, const float simul_time, const int ext_size,
			const short primary_threshold, const short secondary_threshold,
			const SimpleSampler * sampler, const short num_stripes_per_cycle,
			const float deletion_cycle, const int num_objs)
{
    int num_data_exts = 7;
    float num_global_parities = 2;
    float num_local_parities = 2;
    int num_localities = 2;
	std::tuple<shared_ptr<StripeManager>, shared_ptr<EventManager>, shared_ptr<ObjectManager>,
			shared_ptr<ExtentManager>> mngrs 
			= create_managers(num_data_exts, num_local_parities, num_global_parities, num_localities, sampler, 
			ext_size, &Extent::get_default_key);
	shared_ptr<StripeManager> stripe_mngr = std::get<shared_ptr<StripeManager>>(mngrs);
	shared_ptr<ExtentManager> ext_mngr = std::get<shared_ptr<ExtentManager>>(mngrs);
	shared_ptr<ObjectManager> obj_mngr = std::get<shared_ptr<ObjectManager>>(mngrs);
	shared_ptr<EventManager> event_mngr = std::get<shared_ptr<EventManager>>(mngrs);
 	shared_ptr<AbstractStriperDecorator>  striper = make_shared<StriperWithEC>(make_shared<ExtentStackStriper>(make_shared<SimpleStriper>(stripe_mngr, ext_mngr)));

    shared_ptr<AbstractStriperDecorator>  gc_striper = striper;
	shared_ptr<unordered_map<Extent_Object*, double > > obj_pool = make_shared<unordered_map<Extent_Object*, double >>();
    shared_ptr<map<int, Extent *>>  current_exts = make_shared<map<int, Extent *>>();
    current_exts->emplace(0, ext_mngr->create_extent());
    shared_ptr<SimpleObjectPacker> obj_packer = make_shared<MixedObjObjectPacker>(obj_mngr, ext_mngr, num_objs, obj_pool, current_exts);
    shared_ptr<SimpleGCObjectPacker> gc_obj_packer = make_shared<MixedObjGCObjectPacker>(obj_mngr, ext_mngr, num_objs, obj_pool, current_exts);
    shared_ptr<ExtentStack<Extent *, int>> extent_stack = make_shared<ExtentStackRandomizer<Extent *, int>>(make_shared<SingleExtentStack>(stripe_mngr));
    shared_ptr<ExtentStack<Extent *, int>> gc_extent_stack = extent_stack;
	shared_ptr<StripingProcessCoordinator<Extent *, int, sim_T>> coordinator = make_shared<StripingProcessCoordinator<Extent *, int, sim_T>>(obj_packer, gc_obj_packer, striper, gc_striper,extent_stack, gc_extent_stack, stripe_mngr, simul_time);
    /* TODO 
    gc_strategy = StripeLevelWithExtsGCStrategy(primary_threshold, secondary_threshold, coordinator, ext_mngr, gc_striper)
	*/
	shared_ptr<GarbageCollectionStrategy> gc_strategy = nullptr;
    DataCenter<Extent *, int, sim_T>  data_center = DataCenter<Extent *, int, sim_T>(data_center_size, striping_cycle, striper, stripe_mngr, ext_mngr, obj_mngr, event_mngr, gc_strategy, coordinator, simul_time, deletion_cycle);

    return data_center;
}

template <class sim_T>
DataCenter<Extent *, int, sim_T>  randomized_obj_placement_joined_pools_config(const unsigned long data_center_size,
			const float striping_cycle, const float simul_time, const int ext_size,
			const short primary_threshold, const short secondary_threshold,
			const SimpleSampler * sampler, const short num_stripes_per_cycle,
			const float deletion_cycle, const int num_objs)
{
    int num_data_exts = 7;
    float num_global_parities = 2;
    float num_local_parities = 2;
    int num_localities = 2;
	std::tuple<shared_ptr<StripeManager>, shared_ptr<EventManager>, shared_ptr<ObjectManager>,
			shared_ptr<ExtentManager>> mngrs 
			= create_managers(num_data_exts, num_local_parities, num_global_parities, num_localities, sampler, 
			ext_size, &Extent::get_default_key);
	shared_ptr<StripeManager> stripe_mngr = std::get<shared_ptr<StripeManager>>(mngrs);
	shared_ptr<ExtentManager> ext_mngr = std::get<shared_ptr<ExtentManager>>(mngrs);
	shared_ptr<ObjectManager> obj_mngr = std::get<shared_ptr<ObjectManager>>(mngrs);
	shared_ptr<EventManager> event_mngr = std::get<shared_ptr<EventManager>>(mngrs);

 	shared_ptr<AbstractStriperDecorator>  striper = make_shared<StriperWithEC>(make_shared<ExtentStackStriper>(make_shared<SimpleStriper>(stripe_mngr, ext_mngr)));
    shared_ptr<AbstractStriperDecorator>  gc_striper = striper;
    shared_ptr<SimpleObjectPacker> obj_packer = make_shared<RandomizedObjectPacker>(obj_mngr, ext_mngr, num_objs, primary_threshold);
    shared_ptr<SimpleGCObjectPacker> gc_obj_packer = make_shared<RandomizedGCObjectPacker>(obj_mngr, ext_mngr, num_objs, primary_threshold);
    shared_ptr<ExtentStack<Extent *, int>> extent_stack = make_shared<SingleExtentStack>(stripe_mngr);
    shared_ptr<ExtentStack<Extent *, int>> gc_extent_stack = make_shared<SingleExtentStack>(stripe_mngr);
	shared_ptr<StripingProcessCoordinator<Extent *, int, sim_T>> coordinator = make_shared<StripingProcessCoordinator<Extent *, int, sim_T>>(obj_packer, gc_obj_packer, striper, gc_striper,extent_stack, gc_extent_stack, stripe_mngr, simul_time);
    /* TODO 
    gc_strategy = StripeLevelWithExtsGarbageCollectionStrategy(primary_threshold, secondary_threshold, coordinator, ext_mngr, gc_striper, StripeLevelGCStrategy(primary_threshold, secondary_threshold, coordinator, ext_mngr, gc_striper))
	*/
	shared_ptr<GarbageCollectionStrategy> gc_strategy = nullptr;
    DataCenter<Extent *, int, sim_T>  data_center = DataCenter<Extent *, int, sim_T>(data_center_size, striping_cycle, striper, stripe_mngr, ext_mngr, obj_mngr, event_mngr, gc_strategy, coordinator, simul_time, deletion_cycle);

    return data_center;
}

template <class sim_T>
DataCenter<Extent *, int, sim_T>  randomized_objs_no_exts_config(const unsigned long data_center_size,
			const float striping_cycle, const float simul_time, const int ext_size,
			const short primary_threshold, const short secondary_threshold,
			const SimpleSampler * sampler, const short num_stripes_per_cycle,
			const float deletion_cycle, const int num_objs, const int percent_correct)
{
	int num_data_exts = 1;
    float coding_overhead = 18/14;
    float num_global_parities = 2/14;
    float num_local_parities = 2/14;
    int num_localities = 1;
    std::tuple<shared_ptr<StripeManager>, shared_ptr<EventManager>, shared_ptr<ObjectManager>,
		 shared_ptr<ExtentManager>> mngrs 
		 = create_managers(num_data_exts, num_local_parities, num_global_parities, num_localities, sampler, 
		 ext_size, &Extent::get_default_key, coding_overhead);
	shared_ptr<StripeManager> stripe_mngr = std::get<shared_ptr<StripeManager>>(mngrs);
	shared_ptr<ExtentManager> ext_mngr = std::get<shared_ptr<ExtentManager>>(mngrs);
	shared_ptr<ObjectManager> obj_mngr = std::get<shared_ptr<ObjectManager>>(mngrs);
	shared_ptr<EventManager> event_mngr = std::get<shared_ptr<EventManager>>(mngrs);

    shared_ptr<AbstractStriperDecorator> striper = make_shared<StriperWithEC>(make_shared<ExtentStackStriper>(make_shared<SimpleStriper>(stripe_mngr, ext_mngr)));
    shared_ptr<AbstractStriperDecorator> gc_striper = make_shared<StriperWithEC>(make_shared<ExtentStackStriper>(make_shared<SimpleStriper>(stripe_mngr, ext_mngr)));

    shared_ptr<SimpleObjectPacker> obj_packer = make_shared<RandomizedObjectPacker>(obj_mngr, ext_mngr, num_objs, primary_threshold);
    shared_ptr<SimpleGCObjectPacker> gc_obj_packer = make_shared<RandomizedGCObjectPacker>(obj_mngr, ext_mngr, num_objs, primary_threshold);
    
	shared_ptr<ExtentStack<Extent *, int>> extent_stack = make_shared<BestEffortExtentStack>(stripe_mngr);
    shared_ptr<ExtentStack<Extent *, int>> gc_extent_stack = make_shared<BestEffortExtentStack>(stripe_mngr);
 	shared_ptr<StripingProcessCoordinator<Extent *, int, sim_T>> coordinator = make_shared<StripingProcessCoordinator<Extent *, int, sim_T>>(obj_packer, gc_obj_packer, striper, gc_striper,extent_stack, gc_extent_stack, stripe_mngr, simul_time, get_immortal_key);
	/*TODO
    gc_strategy = StripeLevelNoExtsGCStrategy(primary_threshold, secondary_threshold, coordinator, ext_mngr, stripe_mngr, gc_striper)
	*/
	shared_ptr<GarbageCollectionStrategy> gc_strategy = nullptr;
    DataCenter<Extent *, int, sim_T>  data_center = DataCenter<Extent *, int, sim_T> (data_center_size, striping_cycle, striper, stripe_mngr, ext_mngr, obj_mngr, event_mngr, gc_strategy, coordinator, simul_time, deletion_cycle);

    return data_center;
}

template <class sim_T>
DataCenter<Extent *, int, sim_T>  randomized_objs_no_exts_mix_objs_config(const unsigned long data_center_size,
			const float striping_cycle, const float simul_time, const int ext_size,
			const short primary_threshold, const short secondary_threshold,
			const SimpleSampler * sampler, const short num_stripes_per_cycle,
			const float deletion_cycle, const int num_objs, const int percent_correct)
{
	int num_data_exts = 1;
    float coding_overhead = 18/14;
    float num_global_parities = 2/14;
    float num_local_parities = 2/14;
    int num_localities = 1;
    std::tuple<shared_ptr<StripeManager>, shared_ptr<EventManager>, shared_ptr<ObjectManager>,
		 shared_ptr<ExtentManager>> mngrs 
		 = create_managers(num_data_exts, num_local_parities, num_global_parities, num_localities, sampler, 
		 ext_size,&Extent::get_default_key, coding_overhead);
	shared_ptr<StripeManager> stripe_mngr = std::get<shared_ptr<StripeManager>>(mngrs);
	shared_ptr<ExtentManager> ext_mngr = std::get<shared_ptr<ExtentManager>>(mngrs);
	shared_ptr<ObjectManager> obj_mngr = std::get<shared_ptr<ObjectManager>>(mngrs);
	shared_ptr<EventManager> event_mngr = std::get<shared_ptr<EventManager>>(mngrs);

    shared_ptr<AbstractStriperDecorator> striper = make_shared<StriperWithEC>(make_shared<ExtentStackStriper>(make_shared<SimpleStriper>(stripe_mngr, ext_mngr)));
    shared_ptr<AbstractStriperDecorator> gc_striper = striper;

	shared_ptr<unordered_map<Extent_Object*, double > > obj_pool = make_shared<unordered_map<Extent_Object*, double >>();
    shared_ptr<map<int, Extent *>>  current_exts = make_shared<map<int, Extent *>>();
    current_exts->emplace(0, ext_mngr->create_extent());

    shared_ptr<SimpleObjectPacker> obj_packer = make_shared<RandomizedObjectPacker>(obj_mngr, ext_mngr, num_objs, obj_pool, primary_threshold, current_exts);
    shared_ptr<SimpleGCObjectPacker> gc_obj_packer = make_shared<RandomizedGCObjectPacker>(obj_mngr, ext_mngr, num_objs, obj_pool, primary_threshold, current_exts);
    
	shared_ptr<ExtentStack<Extent *, int>> extent_stack = make_shared<SingleExtentStack>(stripe_mngr);
    shared_ptr<ExtentStack<Extent *, int>> gc_extent_stack = extent_stack;
 	shared_ptr<StripingProcessCoordinator<Extent *, int, sim_T>> coordinator = make_shared<StripingProcessCoordinator<Extent *, int, sim_T>>(obj_packer, gc_obj_packer, striper, gc_striper,extent_stack, gc_extent_stack, stripe_mngr, simul_time);
	/*TODO
    gc_strategy = StripeLevelNoExtsGCStrategy(primary_threshold, secondary_threshold, coordinator, ext_mngr, stripe_mngr, gc_striper)
	*/
	shared_ptr<GarbageCollectionStrategy> gc_strategy = nullptr;
    DataCenter<Extent *, int, sim_T>  data_center = DataCenter<Extent *, int, sim_T> (data_center_size, striping_cycle, striper, stripe_mngr, ext_mngr, obj_mngr, event_mngr, gc_strategy, coordinator, simul_time, deletion_cycle);

    return data_center;
}

template <class sim_T>
DataCenter<Extent *, int, sim_T>  age_based_rand_config_no_exts(const unsigned long data_center_size,
			const float striping_cycle, const float simul_time, const int ext_size,
			const short primary_threshold, const short secondary_threshold,
			const SimpleSampler * sampler, const short num_stripes_per_cycle,
			const float deletion_cycle, const int num_objs, const int percent_correct)
{
	int num_data_exts = 1;
    float coding_overhead = 18/14;
    float num_global_parities = 2/14;
    float num_local_parities = 2/14;
    int num_localities = 1;
	time_t (Extent::*key_fnc)() = &Extent::get_timestamp;
    std::tuple<shared_ptr<StripeManager>, shared_ptr<EventManager>, shared_ptr<ObjectManager>,
		 shared_ptr<ExtentManager>> mngrs 
		 = create_managers(num_data_exts, num_local_parities, num_global_parities, num_localities, sampler, 
		 ext_size, key_fnc, coding_overhead);
	shared_ptr<StripeManager> stripe_mngr = std::get<shared_ptr<StripeManager>>(mngrs);
	shared_ptr<ExtentManager> ext_mngr = std::get<shared_ptr<ExtentManager>>(mngrs);
	shared_ptr<ObjectManager> obj_mngr = std::get<shared_ptr<ObjectManager>>(mngrs);
	shared_ptr<EventManager> event_mngr = std::get<shared_ptr<EventManager>>(mngrs);

    shared_ptr<AbstractStriperDecorator> striper = make_shared<StriperWithEC>(make_shared<ExtentStackStriper>(make_shared<SimpleStriper>(stripe_mngr, ext_mngr)));
    shared_ptr<AbstractStriperDecorator> gc_striper = striper;

    shared_ptr<SimpleObjectPacker> obj_packer = make_shared<AgeBasedRandomizedObjectPacker>(obj_mngr, ext_mngr, num_objs, primary_threshold);
    shared_ptr<SimpleGCObjectPacker> gc_obj_packer = make_shared<AgeBasedRandomizedGCObjectPacker>(obj_mngr, ext_mngr, num_objs, primary_threshold);
    
	shared_ptr<ExtentStack<Extent *, int>> extent_stack = make_shared<BestEffortExtentStack>(stripe_mngr);
    shared_ptr<ExtentStack<Extent *, int>> gc_extent_stack = make_shared<BestEffortExtentStack>(stripe_mngr);
 	shared_ptr<StripingProcessCoordinator<Extent *, int, sim_T>> coordinator = make_shared<BestEffortStripingProcessCoordinator<Extent *, int, sim_T>>(obj_packer, gc_obj_packer, striper, gc_striper,extent_stack, gc_extent_stack, stripe_mngr, simul_time, get_timestamp<int>);
	/*TODO
    gc_strategy = StripeLevelNoExtsGCStrategy(primary_threshold, secondary_threshold, coordinator, ext_mngr, stripe_mngr, gc_striper)
	*/
	shared_ptr<GarbageCollectionStrategy> gc_strategy = nullptr;
    DataCenter<Extent *, int, sim_T>  data_center = DataCenter<Extent *, int, sim_T> (data_center_size, striping_cycle, striper, stripe_mngr, ext_mngr, obj_mngr, event_mngr, gc_strategy, coordinator, simul_time, deletion_cycle);

    return data_center;
}


int default_key()
{
	return 0;
}
    
template <class sim_T>
DataCenter<Extent *, int, sim_T>  generational_config(const unsigned long data_center_size,
			const float striping_cycle, const float simul_time, const int ext_size,
			const short primary_threshold, const short secondary_threshold,
			const SimpleSampler * sampler, const short num_stripes_per_cycle,
			const float deletion_cycle, const int num_objs, const int percent_correct)
{
	int num_data_exts = 7;
    float num_global_parities = 2;
    float num_local_parities = 2;
    int num_localities = 2;
	int (Extent::*key_fnc)() = &Extent::get_generation;
    std::tuple<shared_ptr<StripeManager>, shared_ptr<EventManager>, shared_ptr<ObjectManager>,
		 shared_ptr<ExtentManager>> mngrs 
		 = create_managers(num_data_exts, num_local_parities, num_global_parities, num_localities, sampler, 
		 ext_size, key_fnc);
	shared_ptr<StripeManager> stripe_mngr = std::get<shared_ptr<StripeManager>>(mngrs);
	shared_ptr<ExtentManager> ext_mngr = std::get<shared_ptr<ExtentManager>>(mngrs);
	shared_ptr<ObjectManager> obj_mngr = std::get<shared_ptr<ObjectManager>>(mngrs);
	shared_ptr<EventManager> event_mngr = std::get<shared_ptr<EventManager>>(mngrs);

    shared_ptr<AbstractStriperDecorator> striper = make_shared<StriperWithEC>(make_shared<ExtentStackStriper>(make_shared<SimpleStriper>(stripe_mngr, ext_mngr)));
    shared_ptr<AbstractStriperDecorator> gc_striper = striper;

    shared_ptr<SimpleObjectPacker> obj_packer = make_shared<GenerationBasedObjectPacker>(obj_mngr, ext_mngr, num_objs, primary_threshold);
    shared_ptr<SimpleGCObjectPacker> gc_obj_packer = make_shared<GenerationBasedGCObjectPacker>(obj_mngr, ext_mngr, num_objs, primary_threshold);
    shared_ptr<unordered_map<Extent_Object*, double > > obj_pool = make_shared<unordered_map<Extent_Object*, double >>();
    shared_ptr<map<int, Extent *>>  current_exts = make_shared<map<int, Extent *>>();
    current_exts->emplace(0, ext_mngr->create_extent());
	shared_ptr<ExtentStack<Extent *, int>> extent_stack = make_shared<BestEffortExtentStack>(stripe_mngr);
    shared_ptr<ExtentStack<Extent *, int>> gc_extent_stack = make_shared<BestEffortExtentStack>(stripe_mngr);
 	shared_ptr<StripingProcessCoordinator<Extent *, int, sim_T>> coordinator = make_shared<BestEffortStripingProcessCoordinator<Extent *, int, sim_T>>(obj_packer, gc_obj_packer, striper, gc_striper,extent_stack, gc_extent_stack, stripe_mngr, simul_time, get_timestamp<int>);
	/*TODO
    gc_strategy = StripeLevelNoExtsGCStrategy(primary_threshold, secondary_threshold, coordinator, ext_mngr, stripe_mngr, gc_striper)
	*/
	shared_ptr<GarbageCollectionStrategy> gc_strategy = nullptr;
    DataCenter<Extent *, int, sim_T>  data_center = DataCenter<Extent *, int, sim_T> (data_center_size, striping_cycle, striper, stripe_mngr, ext_mngr, obj_mngr, event_mngr, gc_strategy, coordinator, simul_time, deletion_cycle);

    return data_center;
}
#endif // __CONFIGS_H_
