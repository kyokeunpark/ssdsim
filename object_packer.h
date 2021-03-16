#ifndef __OBJECT_PACKER_H_
#define __OBJECT_PACKER_H_

#pragma once

#include <unordered_map>
#include <utility>
#include <vector>
#include "object_manager.h"
#include "extent_manager.h"
#include "extent_stack.h"
#include "extent_object.h"
#include "extent.h"

using obj_record = std::pair<Extent_Object*, int>;
using object_lst = std::vector<obj_record>;
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
	virtual void add_obj(Extent_Object * object, int obj_size) = 0;

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

	/*
	 * Add the object to the object pool. Need to specify the size since when
	 * deleting from an extent only part of the object should be put back
	 * in the pool and repacked.
	 */
	void add_obj(Extent_Object * object, int obj_size)
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
		return ""
	}

	void update_extent_type(Extent * extent)
	{
		if (this->record_ext_types) {
		}
	}

	void add_obj_to_current_ext_at_key(ExtentStack extent_stack, Extent_Object * obj,
			int obj_rem_size, int key)
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
			}
		}
	}

	/*
	 * Method that retrieves all objects from object pool. Identifies the
	 * corresponding key for each object in current_extents and calls
	 * add_obj_to_current_ext_at_key.
	 */
	virtual void pack_objects(ExtentStack extent_stack) = 0;
	// TODO: extent_stack might need to be a pointer here
};

class SimpleObjectPacker: public GenericObjectPacker {

public:

	SimpleObjectPacker(ObjectManager & obj_manager, ExtentManager & ext_manager,
			object_lst obj_pool = object_lst(), current_extents current_exts = current_extents(),
			short num_objs_in_pool = 100, short threshold = 10, bool record_ext_types = false)
		: GenericObjectPacker(obj_manager, ext_manager, obj_pool, current_extents,
				num_objs_in_pool, threshold, record_ext_types) {}

	void pack_objects(ExtentStack extent_stack)
	{
		while(this->obj_pool.size > 0) {
			obj_record obj = this->obj_pool.pop_back();
		}
		(void) extent_stack;
	};

};

#endif // __OBJECT_PACKER_H_
