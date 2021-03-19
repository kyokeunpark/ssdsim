#ifndef __STRIPING_PROCESS_COORDINATOR_H_
#define __STRIPING_PROCESS_COORDINATOR_H_

#include "object_packer.h"
#include "extent_stack.h"
#include "stripers.h"
#include "stripe_manager.h"

class StripingProcessCoordinator {

    GenericObjectPacker* obj_packer;
    SimpleGCObjectPacker* gc_object_packer;
    AbstractStriper* striper;

};

#endif // __STRIPING_PROCESS_COORDINATOR_H_
