#ifndef __OBJECT_PACKER_H_
#define __OBJECT_PACKER_H_

#include <unordered_map>
#include <utility>
#include <vector>
#include "object_manager.h"
#include "extent_manager.h"
#include "extent_object.h"
#include "extent.h"

using object_lst = std::vector<std::pair<Extent_Object*, int>>;
using current_extents = std::unordered_map<int, Extent*>;

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

	ObjectManager obj_manager;
	ExtentManager ext_manager;
	object_lst obj_pool;
	current_extents current_exts;
	short threshold, num_objs_in_pool;
	bool record_ext_types;

public:

	GenericObjectPacker(ObjectManager & obj_manager, ExtentManager & ext_manager,
			object_lst obj_pool = object_lst(), current_extents current_exts = current_extents(),
			short num_objs_in_pool = 100, short threshold = 10, bool record_ext_types = false)
		: obj_manager(obj_manager), ext_manager(ext_manager), obj_pool(obj_pool),
		current_exts(current_exts), num_objs_in_pool(num_objs_in_pool),
		threshold(threshold), record_ext_types(record_ext_types) {}

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
};

#endif // __OBJECT_PACKER_H_
