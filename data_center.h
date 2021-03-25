#ifndef __DATA_CENTER_H_
#define __DATA_CENTER_H_
#pragma once
#include "event_manager.h"
#include "extent_manager.h"
#include "gc_strategies.h"
#include "object_manager.h"
#include "stripe_manager.h"
#include "stripers.h"
#include "striping_process_coordinator.h"

class DataCenter {
  public:
    int max_size;
    float striping_cycle;
    DataCenter(int data_center_size, int striping_cycle,
               shared_ptr<AbstractStriperDecorator> striper,
               shared_ptr<StripeManager> stripe_mngr,
               shared_ptr<ExtentManager> ext_mngr,
               shared_ptr<ObjectManager> obj_mngr,
               shared_ptr<EventManager> event_mngr,
               shared_ptr<GarbageCollectionStrategy> gc_strategy,
               shared_ptr<StripingProcessCoordinator> coordinator,
               float simul_time, int deletion_cycle) {}
};

#endif // __DATA_CENTER_H_
