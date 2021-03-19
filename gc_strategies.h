#ifndef __GC_STRATEGIES_H_
#define __GC_STRATEGIES_H_

#include "extent_manager.h"
#include "stripers.h"
#include <unordered_map>

class GarbageCollectionStrategy {

	short primary_threshold, secondary_threshold;
	ExtentManager extent_manager;
	short num_gc_cycles, num_exts_gced, num_localities_in_gc;
	// TODO: Need gc_striper and striping process coordinator implementation
	// GCStriper

public:
	GarbageCollectionStrategy(short p_thresh, short s_thresh, ExtentManager & eman)
			: primary_threshold(p_thresh), secondary_threshold(s_thresh),
			extent_manager(eman)
	{
		this->num_gc_cycles = 0;

		// Need to divide by number of gc cycles to get the avg number of
		// exts gced per cycle and the number of localities per cycle
		this->num_exts_gced = 0;
		this->num_localities_in_gc = 0;
	}

	void add_num_gc_cycles(const int & num)
	{
		this->num_gc_cycles += num;
	}

	void add_num_exts_gced(const int & num)
	{
		this->num_exts_gced += num;
	}

	void add_localities_in_gc(const int & num)
	{
		this->num_localities_in_gc += num;
	}

	// Defines the strategy for gc on a single stripe
	virtual void stripe_gc() = 0;
	// Mechanism for determining which stripes are ready for gc
	virtual void gc_handler() = 0;
};

#endif // __GC_STRATEGIES_H_
