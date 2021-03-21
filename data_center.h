#ifndef __DATA_CENTER_H_
#define __DATA_CENTER_H_
#pragma once
#include "object_manager.h"
#include "stripers.h"
#include "stripe_manager.h"
#include "extent_manager.h"
#include "object_manager.h"
#include "event_manager.h"
#include "gc_strategies.h"
#include "striping_process_coordinator.h"
template<class extent_stack_value_type,class extent_stack_key_type,class sim_T>
class DataCenter
{

	int max_size;
	float striping_cycle;

public:

	DataCenter(int max_size, int striping_cycle, shared_ptr<AbstractStriperDecorator> striper,
	 shared_ptr<StripeManager> stripe_mngr, shared_ptr<ExtentManager> ext_mngr,shared_ptr<ObjectManager> obj_mngr,
	  shared_ptr<EventManager>event_mngr, shared_ptr<GarbageCollectionStrategy> gc_strategy, shared_ptr<StripingProcessCoordinator>coordinator,
	   sim_T simul_time=365, int gc_cycle=1)
	{

	}

};

#endif // __DATA_CENTER_H_
