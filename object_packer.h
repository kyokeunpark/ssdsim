#ifndef __OBJECT_PACKER_H_
#define __OBJECT_PACKER_H_

#pragma once

#include <random>
#include <unordered_map>
#include <numeric>
#include <utility>
#include <vector>
#include <cmath>
#include <algorithm>
#include "object_manager.h"
#include "extent_manager.h"
#include "extent_stack.h"
#include "extent_object.h"
#include "extent.h"

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
	virtual void generate_exts_at_key(shared_ptr<AbstractExtentStack> extent_stack,
			int num_exts, int key)
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
	virtual void generate_stripes(shared_ptr<AbstractExtentStack> extent_stack, int simulation_time)
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
	virtual void generate_objs(int space)
	{
		while (space > 0) {
			object_lst objs = this->obj_manager.create_new_object(1);
			space -= objs[0].second;
			this->add_objs(objs);
		}
	}

	void add_obj_to_current_ext_at_key(shared_ptr<AbstractExtentStack> extent_stack,
			Extent_Object * obj, int obj_rem_size, int key)
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

	virtual void pack_objects(shared_ptr<AbstractExtentStack> extent_stack, int key=0)
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

	SimpleGCObjectPacker(ObjectManager & obj_manager, ExtentManager & ext_manager,
			object_lst obj_pool = object_lst(), current_extents current_exts = current_extents(),
			short num_objs_in_pool = 100, short threshold = 10, bool record_ext_types = false)
		: SimpleObjectPacker(obj_manager, ext_manager, obj_pool, current_exts,
				num_objs_in_pool, threshold, record_ext_types) {}

	/*
	 * Can't generate a gc stripe on-demand.
	 */
	virtual void generate_stripes(shared_ptr<AbstractExtentStack> extent_stack,
			int simulation_time)
	{
		return;
	}

	/*
	 * Repacks objects from given extent
	 */
	virtual void gc_extent(Extent * ext, shared_ptr<AbstractExtentStack> extent_stack,
			object_lst objs=object_lst())
	{
		for (auto & obj : objs)
			this->add_obj(obj.first, ext->get_obj_size(obj.first));
		this->pack_objects(extent_stack, objs);
	}

	virtual void pack_objects(shared_ptr<AbstractExtentStack> extent_stack, object_lst objs=object_lst(),
			int key=0)
	{
		while(this->obj_pool.size() > 0) {
			obj_record obj = this->obj_pool.back();
			this->obj_pool.pop_back();
			this->add_obj_to_current_ext_at_key(extent_stack, obj.first, obj.second, key);
		}
	}
};

/*
 * In this configuration, objects are divided into 4MB chunks and randomly
 * placed into extents. Pieces of the same object can end up in different
 * extents.
 */
class RandomizedObjectPacker:public SimpleObjectPacker{

public:
    using SimpleObjectPacker::SimpleObjectPacker;

	void pack_objects(shared_ptr<AbstractExtentStack> extent_stack, object_lst objs=object_lst(), int key=0)
	{
		std::vector<Extent_Object*> objs_lst = {};
		for (auto & record : this->obj_pool) {
			int rem_size = record.second;
			for (int i = 0; i < rem_size / 4; i++)
				objs_lst.emplace_back(record.first);
		}
		this->obj_pool.clear();
		std::random_shuffle(objs_lst.begin(), objs_lst.end());
		for (auto & record : objs_lst)
			this->add_obj_to_current_ext_at_key(extent_stack, record, 4, key);
	}

};

/*
 * In this configuration, objects are divided into 4MB chunks and randomly
 * placed into extents. Pieces of the same object can end up in different
 * extents.
 */
class RandomizedGCObjectPacker:public SimpleGCObjectPacker{

public:
    using SimpleGCObjectPacker::SimpleGCObjectPacker;

	void pack_objects(shared_ptr<AbstractExtentStack> extent_stack, object_lst objs=object_lst(), int key=0) override
	{
		std::vector<Extent_Object*> objs_lst = {};
		for (auto & it : this->obj_pool) {
			int rem_size = it.second;
			for (int i = 0; i < rem_size / 4; i++)
				objs_lst.emplace_back(it.first);
		}
		this->obj_pool.clear();
		std::random_shuffle(objs_lst.begin(), objs_lst.end());
		for (auto & it : objs_lst)
			this->add_obj_to_current_ext_at_key(extent_stack, it, 4, key);
	}
};

/*
 * This is for 4MB mixed pools config
 */
class RandomizedMixedObjectPacker: public SimpleObjectPacker {

public:
	using SimpleObjectPacker::SimpleObjectPacker;

	/*
	 * This method will ensure that the extent stack has enough extents to
	 * create the required number of stripes. Each subclass has to dtermine
	 * how to divide the extent stack into stripes (i.e., can extents belonging
	 * to one key mix with extents from another - for example for extent size
	 * they don't, for generations and lifetime they do).
	 */
	void generate_stripes(shared_ptr<AbstractExtentStack> extent_stack,
			int simulation_time=365) override
	{
		int ave_pool_size = 46000;
		int curr_pool_size = 0;
		bool new_objs_added = false;

		for (auto & record : this->obj_pool)
			curr_pool_size += record.second;

		while(curr_pool_size < ave_pool_size) {
			object_lst objs = this->obj_manager.create_new_object();
			this->add_objs(objs);
			for (auto & record : objs)
				curr_pool_size += record.second;
			new_objs_added = true;
		}

		if (new_objs_added) {
			object_lst obj_lst = {};
			for (auto & record : this->obj_pool) {
				int rem_size = record.second;
				for (int i = 0; i < rem_size / 4 + round((rem_size % 4) / 4.0); i++)
					obj_lst.emplace_back(std::make_pair(record.first, 4));
				this->obj_pool.clear(); // TODO: Might not be necessary?
				this->obj_pool = obj_lst;
				std::random_shuffle(this->obj_pool.begin(), this->obj_pool.end());
			}
		}

		this->pack_objects(extent_stack);
	}

	void pack_objects(shared_ptr<AbstractExtentStack> extent_stack, int key=0) override
	{
		for (auto record : this->obj_pool)
			this->add_obj_to_current_ext_at_key(extent_stack, record.first, record.second, key);
		this->obj_pool.clear();
	}

	/*
	 * The method generates num_exts at the given key
	 */
	void generate_exts_at_key(shared_ptr<AbstractExtentStack> extent_stack,
			int num_exts, int key) override
	{
		// Check if the pool has enough object volume, and if not, generate
		// more fresh objects
		int ave_pool_size = 46000;
		int num_exts_at_key = extent_stack->get_length_at_key(key);
		int pool_size = 0;
		for (auto & record : this->obj_pool)
			pool_size += record.second;

		while (pool_size < ave_pool_size) {
			object_lst objs = this->obj_manager.create_new_object();
			this->add_objs(objs);
			for (auto & record : objs)
				pool_size += record.second;
		}

		// Mix valid and fresh objects
		object_lst obj_lst = {};
		for (auto & record : this->obj_pool) {
			int rem_size = record.second;
			for (int i = 0; i < rem_size / 4 + round((rem_size % 4) / 4.0); i++)
				obj_lst.emplace_back(std::make_pair(record.first, 4));
		}
		this->obj_pool.clear();
		this->obj_pool = obj_lst;
		std::random_shuffle(this->obj_pool.begin(), this->obj_pool.end());

		while (num_exts_at_key < num_exts) {
			auto obj = this->obj_pool.front();
			this->obj_pool.erase(this->obj_pool.begin());
			for (auto & record : this->obj_pool)
				this->add_obj_to_current_ext_at_key(extent_stack, record.first,
						record.second, key);
			num_exts_at_key = extent_stack->get_length_at_key(key);
		}
	}

};

class RandomizedMixedGCObjectPacker: public SimpleGCObjectPacker {

public:
	using SimpleGCObjectPacker::SimpleGCObjectPacker;

	/*
	 * Way to generate new objs during gc so that valid objs and new objs can
	 * be mixed in joined pool.
	 */
	void generate_exts() override {
		if (this->obj_pool.size() < this->num_objs_in_pool) {
			auto objs = this->obj_manager.create_new_object(this->num_objs_in_pool
					- this->obj_pool.size());
			this->add_objs(objs);
		}
	};

	/*
	 * Repacks objects from given extent
	 */
	void gc_extent(Extent * ext, shared_ptr<AbstractExtentStack> extent_stack,
			object_lst objs=object_lst()) override
	{
		// TODO: We are adding this object without the notion of the extent
		// 		 shard. Would this cause any problems?
		for (auto obj : *ext->objects)
			this->add_obj(obj.first, ext->get_obj_size(obj.first));
	}
};

class MixedObjObjectPacker:public SimpleObjectPacker{

public:
    using SimpleObjectPacker::SimpleObjectPacker;

	/*
	 * This method will ensure that the extent stack has enough extents to 
	 * create the required number of stripes. Each subclass has to determine 
	 * how to divide th extent stack into stripes (i.e., can extents belonging
	 * to one key mix with extents from another - for example, for extent size
	 * they don't, for generations and lifetime they do).
	 */
	void generate_stripes(shared_ptr<AbstractExtentStack> extent_stack,
			int simulation_time=365) override
	{
		if (this->obj_pool.size() < this->num_objs_in_pool) {
			auto objs = this->obj_manager.create_new_object(this->num_objs_in_pool
					- this->obj_pool.size());
			this->add_objs(objs);
		}
		this->pack_objects(extent_stack);
	}

	void pack_objects(shared_ptr<AbstractExtentStack> extent_stack, int key=0) override
	{
		std::random_shuffle(this->obj_pool.begin(), this->obj_pool.end());
		for (int i = 0; i < this->obj_pool.size(); i++) {
			auto obj = this->obj_pool.front();
			this->add_obj_to_current_ext_at_key(extent_stack, obj.first, obj.second, key);
			this->obj_pool.erase(this->obj_pool.begin());
		}
	}

	void generate_exts_at_key(shared_ptr<AbstractExtentStack> extent_stack,
			int num_exts, int key) override
	{
		int num_exts_at_key = extent_stack->get_length_at_key(key);
		std::random_shuffle(this->obj_pool.begin(), this->obj_pool.end());
		while (num_exts_at_key < num_exts) {
			auto obj = this->obj_pool.front();
			this->add_obj_to_current_ext_at_key(extent_stack, obj.first, obj.second, key);
			this->obj_pool.erase(this->obj_pool.begin());
		}
	}
};

class MixedObjGCObjectPacker:public SimpleGCObjectPacker{

public:
    using SimpleGCObjectPacker::SimpleGCObjectPacker;

	void generate_exts() override
	{
		if (this->obj_pool.size() < this->num_objs_in_pool) {
			auto objs = this->obj_manager.create_new_object(this->num_objs_in_pool
					- this->obj_pool.size());
			this->add_objs(objs);
		}
	}

	void generate_objs(int space) override
	{
		while (space > 0) {
			auto objs = this->obj_manager.create_new_object(1);
			space -= objs[0].second;
			this->add_objs(objs);
		}
	}

	/*
	 * Repacks objects from given extent.
	 */
	void gc_extent(Extent * ext, shared_ptr<AbstractExtentStack> extent_stack,
			object_lst objs=object_lst())
	{
		for (auto obj : *ext->objects)
			this->add_obj(obj.first, ext->get_obj_size(obj.first));
	}

};

class MortalImmortalObjectPacker:public SimpleObjectPacker{

	const int mortal_key = 1, immortal_key = 0;
	float percent_correct;

public:
    using SimpleObjectPacker::SimpleObjectPacker;

	MortalImmortalObjectPacker(ObjectManager & obj_manager, ExtentManager & ext_manager,
			object_lst obj_pool = object_lst(), current_extents current_exts = current_extents(),
			short num_objs_in_pool = 100, short threshold = 10, float percent_correct = 100.0)
		: SimpleObjectPacker(obj_manager, ext_manager, obj_pool, current_exts,
				num_objs_in_pool, threshold)
	{
		this->percent_correct = percent_correct / 100.0;
	}

	void pack_objects(shared_ptr<AbstractExtentStack> extent_stack, int key=0) override
	{
		std::mt19937_64 rd;
		auto time_seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
		rd.seed(time_seed);
		std::uniform_real_distribution<float> unif(0, 1);
		float p;

		while (this->obj_pool.size() > 0) {
			int key = immortal_key;
			auto obj = this->obj_pool.front();
			this->obj_pool.erase(this->obj_pool.begin());
			p = unif(rd);

			if ((obj.first->life <= 365 && p <= this->percent_correct)
					|| obj.first->life > 365 && p > this->percent_correct)
				key = mortal_key;

			this->add_obj_to_current_ext_at_key(extent_stack, obj.first, obj.second, key);
		}
	}
};

class MortalImmortalGCObjectPacker:public SimpleGCObjectPacker{

	const int mortal_key = 1, immortal_key = 0;
	float percent_correct;

public:
    using SimpleGCObjectPacker::SimpleGCObjectPacker;

	MortalImmortalGCObjectPacker(ObjectManager & obj_manager, ExtentManager & ext_manager,
			object_lst obj_pool = object_lst(), current_extents current_exts = current_extents(),
			short num_objs_in_pool = 100, short threshold = 10, float percent_correct = 100.0)
		: SimpleGCObjectPacker(obj_manager, ext_manager, obj_pool, current_exts,
				num_objs_in_pool, threshold)
	{
		this->percent_correct = percent_correct / 100.0;
	}

	void pack_objects(shared_ptr<AbstractExtentStack> extent_stack, int key=0) override
	{
		std::mt19937_64 rd;
		auto time_seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
		rd.seed(time_seed);
		std::uniform_real_distribution<float> unif(0, 1);
		float p;

		while (this->obj_pool.size() > 0) {
			int key = immortal_key;
			auto obj = this->obj_pool.front();
			this->obj_pool.erase(this->obj_pool.begin());
			p = unif(rd);

			if ((obj.first->life <= 365 && p <= this->percent_correct)
					|| obj.first->life > 365 && p > this->percent_correct)
				key = mortal_key;

			this->add_obj_to_current_ext_at_key(extent_stack, obj.first, obj.second, key);
		}
	}

};

class SizeBasedObjectPacker:public SimpleObjectPacker{

public:
    using SimpleObjectPacker::SimpleObjectPacker;

	void adjust_index(int & ind, int & length)
	{
		ind = min(ind, length - 1);
		ind = max(0, ind);
	}

	int get_smaller_obj_index(int ext_size, int free_space)
	{
		int ind;
		if (ext_size == free_space) {
			ind = -1;
		} else {
			// TODO: Need to make sure that the upper_bound and lower_bound
			// 		 calls returns same indices as Python's bisect_left and 
			//		 bisect_right function call
			auto obj_it = std::upper_bound(this->obj_pool.begin(), this->obj_pool.end(), free_space);
			ind = obj_it - this->obj_pool.begin() - 1;
			this->adjust_index(ind, obj_it->second);
		}
		return ind;
	}

	int get_larger_obj_index(int ext_size, int free_space)
	{
		int ind;
		if (ext_size == free_space) {
			ind = -1;
		} else {
			auto obj_it = std::lower_bound(this->obj_pool.begin(), this->obj_pool.end(), free_space);
			ind = obj_it - this->obj_pool.begin() - 1;
			this->adjust_index(ind, obj_it->second);
		}
		return ind;
	}

	void insert_obj_back_into_pool(Extent_Object * obj, int obj_size)
	{
		if (this->obj_pool.size() == 0) {
			this->obj_pool.emplace_back(std::make_pair(obj, obj_size));
			return;
		}

		int ind;
		auto obj_it = std::lower_bound(this->obj_pool.begin(), this->obj_pool.end(), obj_size);
		ind = obj_it - this->obj_pool.begin();
		this->adjust_index(ind, obj_it->second);

		this->obj_pool.insert(this->obj_pool.begin() + ind, *obj_it);
	}
};

class SizeBasedGCObjectPackerBaseline:public SimpleGCObjectPacker{
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

#endif // __OBJECT_PACKER_H_