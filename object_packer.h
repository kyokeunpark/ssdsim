#ifndef __OBJECT_PACKER_H_
#define __OBJECT_PACKER_H_

#pragma once

#include <unordered_map>
#include <numeric>
#include <utility>
#include <vector>
#include <cmath>
#include "object_manager.h"
#include "extent_manager.h"
#include "extent_stack.h"
#include "extent_object_stripe.h"

using current_extents = std::unordered_map<int, Extent*>;
using ext_types_mgr = std::unordered_map<string, int>;

/*
 * Interface for ObjectPackers to follow
 */
class ObjectPacker {

public:
	/*
	 * Method for adding items to the object pool. Each object packer can
	 * decide on the best policy for adding the object into the pool.
	 */
	virtual void add_obj(ExtentObject * object, int obj_size) = 0;

	/*
	 * Method for packing objects into extents. Each packer decides on the
	 * policy of how to pack objects into extents.
	 */
	virtual void pack_objects() = 0;

};

class GenericObjectPacker: public ObjectPacker {

protected:
	ObjectManager obj_manager;
	ExtentManager ext_manager;
	object_lst obj_pool;
	current_extents current_exts;
	ext_types_mgr ext_types;
	short threshold, num_objs_in_pool;
	bool record_ext_types;

public:

	GenericObjectPacker(ObjectManager & obj_manager, ExtentManager & ext_manager,
			object_lst obj_pool = object_lst(), current_extents current_exts = current_extents(),
			short num_objs_in_pool = 100, short threshold = 10, bool record_ext_types = false)
		: obj_manager(obj_manager), ext_manager(ext_manager), obj_pool(obj_pool),
		current_exts(current_exts), num_objs_in_pool(num_objs_in_pool),
		threshold(threshold), record_ext_types(record_ext_types)
		{
			this->ext_types = ext_types_mgr();
		}
	virtual void generate_exts()
	{}

	ext_types_mgr get_ext_types()
	{
		return ext_types;
	}
	/*
	 * Add the object to the object pool. Need to specify the size since when
	 * deleting from an extent only part of the object should be put back
	 * in the pool and repacked.
	 */
	void add_obj(ExtentObject * object, int obj_size)
	{
		this->obj_pool.emplace_back(std::make_pair(object, obj_size));
	}

	/*
	 * Add the objects in obj_lst to the object pool.
	 */
	void add_objs(object_lst obj_lst)
	{
		this->obj_pool.insert(this->obj_pool.end(), obj_lst.begin(), obj_lst.end());
	}

	/*
	 * Returns true if extent is in current extents dict and false otherwise
	 */
	bool is_ext_in_current_extents(Extent * extent)
	{
		for (auto & ext : this->current_exts)
			if (ext.second == extent)
				return true;
		return false;
	}

	void remove_extent_from_current_extents(Extent * extent)
	{
		for (auto & ext : this->current_exts){
			if (ext.second == extent) {
				this->current_exts.erase(ext.first);
				return;
			}
		}
	}

	/*
	 * Returns the extent type based on its largest object. Right now large
	 * extents have objects that occupy the whole extent (i.e., object is
	 * larger than one extent). Small obj extents that have objects that are
	 * smaller than the gc_threshold. The rest are defined by the percentage
	 * occupancy of the extent by the largest object.
	 */
	string get_extent_type(Extent * extent)
	{
		// Find the largest object stored in the extent
		int largest_obj = INT32_MIN, local_max = 0, num_objs = 0;
		for (auto & tuple : extent->obj_ids_to_obj_size) {
			std::vector<int> sizes = tuple.second;
			local_max = std::accumulate(sizes.begin(), sizes.end(), 0);
			if (largest_obj < local_max) {
				largest_obj = local_max;
				num_objs = sizes.size();
			}
		}
		if (largest_obj >= this->threshold / 100.0 * extent->ext_size
				&& largest_obj < extent->ext_size) {
			double frac = largest_obj / extent->ext_size;
			return std::to_string(floor(frac) * 10) + "-"
				+ std::to_string(ceil(frac) * 10);
		} else if (largest_obj < this->threshold / 100.0 * extent->ext_size) {
			return "small";
		} else {
			return "large";
		}
	}

	void update_extent_type(Extent * extent)
	{
		if (this->record_ext_types) {
			string ext_type = this->get_extent_type(extent);

			if (this->ext_types.find(ext_type) != this->ext_types.end())
				this->ext_types[ext_type] += 1;
			else
				this->ext_types[ext_type] = 1;
		}
	}

	/*
	 * The method generates num_exts at the extent list at the given key
	 */
	void generate_exts_at_key(shared_ptr<AbstractExtentStack> extent_stack, int num_exts, int key)
	{
		int num_exts_at_key = extent_stack->get_length_at_key(key);
		while (num_exts_at_key < num_exts) {
			object_lst objs = this->obj_manager.create_new_object();
			this->add_objs(objs);
			this->pack_objects(extent_stack);
			num_exts_at_key = extent_stack->get_length_at_key(key);
		}
	}

	/*
	 * This method will ensure that the extnet stack has enough extents to
	 * create the required number of stripes. Each subclass has to
	 * determine how to divide the extent stack into stripes (i.e., can extents
	 * belonging to one key mix with extents from another)
	 */
	void generate_stripes(shared_ptr<AbstractExtentStack> extent_stack, int simulation_time)
	{
		if (this->obj_pool.size() < this->num_objs_in_pool) {
			object_lst objs = this->obj_manager.create_new_object(this->num_objs_in_pool - this->obj_pool.size());
			this->add_objs(objs);
		}
		this->pack_objects(extent_stack);
	}

	/*
	 * Creates objects to fill the provided amount of space.
	 */
	void generate_objs(int space)
	{
		while (space > 0) {
			object_lst objs = this->obj_manager.create_new_object(1);
			space -= objs[0].second;
			this->add_objs(objs);
		}
	}

	void add_obj_to_current_ext_at_key(shared_ptr<AbstractExtentStack> extent_stack,
			ExtentObject * obj, int obj_rem_size, int key)
	{
		int temp = 0;
		if (this->current_exts.find(key) == this->current_exts.end())
			this->current_exts[key] = this->ext_manager.create_extent();
		Extent * current_ext = this->current_exts[key];

		while (obj_rem_size > 0) {
			temp = current_ext->add_object(obj, obj_rem_size);
			obj_rem_size -= temp;

			// Seal the extent if the extent is full
			if (obj_rem_size > 0 || (obj_rem_size == 0 && current_ext->free_space == 0)) {
				this->update_extent_type(current_ext);
				current_ext->type = this->get_extent_type(current_ext);

				extent_stack->add_extent(key, this->current_exts[key]);
				current_ext = this->ext_manager.create_extent();
				this->current_exts[key] = current_ext;
			}
		}
	}

	/*
	 * Method that retrieves all objects from object pool. Identifies the
	 * corresponding key for each object in current_extents and calls
	 * add_obj_to_current_ext_at_key.
	 */
	virtual void pack_objects(shared_ptr<AbstractExtentStack> extent_stack) = 0;
	// TODO: extent_stack might need to be a pointer here
};

/*
 * Packs objects into extents. For now I am only making a place-holder for the
 * actual object that would pack objects based on a given optimal policy. In
 * this simple code I only add objects from the object_pool, to the
 * current_extents in a fifo scheme. As a result, this is equivalent to
 * having only a single current extent at a time.
 */
class SimpleObjectPacker: public GenericObjectPacker {

public:

	SimpleObjectPacker(ObjectManager & obj_manager, ExtentManager & ext_manager,
			object_lst obj_pool = object_lst(), current_extents current_exts = current_extents(),
			short num_objs_in_pool = 100, short threshold = 10, bool record_ext_types = false)
		: GenericObjectPacker(obj_manager, ext_manager, obj_pool, current_exts,
				num_objs_in_pool, threshold, record_ext_types) {}

	void pack_objects(shared_ptr<AbstractExtentStack> extent_stack, int key=0)
	{
		while(this->obj_pool.size() > 0) {
			obj_record obj = this->obj_pool.back();
			this->obj_pool.pop_back();
			this->add_obj_to_current_ext_at_key(extent_stack, obj.first, obj.second, key);
		}
	};

};

class SimpleGCObjectPacker: public SimpleObjectPacker {
public:

	/*
	 * Can't generate a gc stripe on-demand.
	 */
	void generate_stripes(shared_ptr<AbstractExtentStack> extent_stack, int simulation_time)
	{
		return;
	}

	/*
	 * Repacks objects from given extent
	 */
	void gc_extent(Extent * ext, shared_ptr<AbstractExtentStack> extent_stack,
			object_lst objs=object_lst())
	{
		for (auto & obj : objs)
			this->add_obj(obj.first, ext->get_obj_size(obj.first));
		this->pack_objects(extent_stack, objs);
	}

	void pack_objects(shared_ptr<AbstractExtentStack> extent_stack, object_lst objs=object_lst(),
			int key=0)
	{
		while(this->obj_pool.size() > 0) {
			obj_record obj = this->obj_pool.back();
			this->obj_pool.pop_back();
			this->add_obj_to_current_ext_at_key(extent_stack, obj.first, obj.second, key);
		}
	}
};

#endif // __OBJECT_PACKER_H_

class MixedObjObjectPacker:public SimpleObjectPacker{
    public:
    using SimpleObjectPacker::SimpleObjectPacker;
};

class MixedObjGCObjectPacker:public SimpleGCObjectPacker{
    public:
    using SimpleGCObjectPacker::SimpleGCObjectPacker;
};

class AgeBasedObjectPacker:public SimpleObjectPacker{
    public:
    using SimpleObjectPacker::SimpleObjectPacker;
};

class AgeBasedGCObjectPacker:public SimpleGCObjectPacker{
    public:
    using SimpleGCObjectPacker::SimpleGCObjectPacker;
};

class SizeBasedObjectPackerBaseline:public SimpleObjectPacker{
    public:
    using SimpleObjectPacker::SimpleObjectPacker;
};

class SizeBasedGCObjectPackerBaseline:public SimpleGCObjectPacker{
    public:
    using SimpleGCObjectPacker::SimpleGCObjectPacker;
};

class SizeBasedObjectPackerSmallerObj:public SimpleObjectPacker{
    public:
    using SimpleObjectPacker::SimpleObjectPacker;
};

class SizeBasedGCObjectPackerSmallerObj:public SimpleGCObjectPacker{
    public:
    using SimpleGCObjectPacker::SimpleGCObjectPacker;
};
class SizeBasedObjectPackerDynamicStrategy:public SimpleObjectPacker{
    public:
    using SimpleObjectPacker::SimpleObjectPacker;
};
class SizeBasedGCObjectPackerDynamicStrategy:public SimpleGCObjectPacker{
    public:
    using SimpleGCObjectPacker::SimpleGCObjectPacker;
};

class SizeBasedObjectPackerSmallerWholeObjFillGap:public SimpleObjectPacker{
    public:
    using SimpleObjectPacker::SimpleObjectPacker;
};
class SizeBasedGCObjectPackerSmallerWholeObjFillGap:public SimpleGCObjectPacker{
    public:
    using SimpleGCObjectPacker::SimpleGCObjectPacker;
};

class SizeBasedObjectPackerLargerWholeObj:public SimpleObjectPacker{
    public:
    using SimpleObjectPacker::SimpleObjectPacker;
};

class SizeBasedGCObjectPackerLargerWholeObj:public SimpleGCObjectPacker{
    public:
    using SimpleGCObjectPacker::SimpleGCObjectPacker;
};

class MortalImmortalObjectPacker:public SimpleObjectPacker{
    public:
    using SimpleObjectPacker::SimpleObjectPacker;
};

class MortalImmortalGCObjectPacker:public SimpleGCObjectPacker{
    public:
    using SimpleGCObjectPacker::SimpleGCObjectPacker;
};

class RandomizedObjectPacker:public SimpleObjectPacker{
    public:
    using SimpleObjectPacker::SimpleObjectPacker;
};

class RandomizedGCObjectPacker:public SimpleGCObjectPacker{
    public:
    using SimpleGCObjectPacker::SimpleGCObjectPacker;
};

class AgeBasedRandomizedObjectPacker:public SimpleObjectPacker{
    public:
    using SimpleObjectPacker::SimpleObjectPacker;
};

class AgeBasedRandomizedGCObjectPacker:public SimpleGCObjectPacker{
    public:
    using SimpleGCObjectPacker::SimpleGCObjectPacker;
};

class GenerationBasedObjectPacker:public SimpleObjectPacker{
    public:
    using SimpleObjectPacker::SimpleObjectPacker;
};

class GenerationBasedGCObjectPacker:public SimpleGCObjectPacker{
    public:
    using SimpleGCObjectPacker::SimpleGCObjectPacker;
};