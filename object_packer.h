#ifndef __OBJECT_PACKER_H_
#define __OBJECT_PACKER_H_

#pragma once

#include "extent.h"
#include "extent_manager.h"
#include "extent_object.h"
#include "extent_stack.h"
#include "object_manager.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

using current_extents = std::unordered_map<int, Extent *>;
using ext_types_mgr = std::unordered_map<string, int>;

bool operator<(const int v, obj_record &record) { return v < record.second; }

bool operator<(obj_record &record, const int v) { return record.second < v; }

/*
 * Interface for ObjectPackers to follow
 */
class ObjectPacker {

  public:
	virtual ~ObjectPacker() {}

    /*
     * Method for adding items to the object pool. Each object packer can
     * decide on the best policy for adding the object into the pool.
     */
    virtual void add_obj(Extent_Object *object, int obj_size) = 0;

    /*
     * Method for packing objects into extents. Each packer decides on the
     * policy of how to pack objects into extents.
     */
    virtual void pack_objects() {};
};

class GenericObjectPacker : public ObjectPacker {

  protected:
    shared_ptr<ObjectManager> obj_manager;
    shared_ptr<ExtentManager> ext_manager;
    object_lst obj_pool;
    current_extents current_exts;
    ext_types_mgr ext_types;
    short threshold, num_objs_in_pool;
    bool record_ext_types;

  public:
    GenericObjectPacker(shared_ptr<ObjectManager> obj_manager,
                        shared_ptr<ExtentManager> ext_manager,
                        object_lst obj_pool = object_lst(),
                        current_extents current_exts = current_extents(),
                        short num_objs_in_pool = 100, short threshold = 10,
                        bool record_ext_types = false)
        : obj_manager(obj_manager), ext_manager(ext_manager),
          obj_pool(obj_pool), current_exts(current_exts),
          num_objs_in_pool(num_objs_in_pool), threshold(threshold),
          record_ext_types(record_ext_types) {
        this->ext_types = ext_types_mgr();
    }

    current_extents get_current_exts() { return current_exts; }
    object_lst get_obj_pool() { return obj_pool; }

    virtual void generate_exts() {}

    ext_types_mgr get_ext_types() { return ext_types; }
    /*
     * Add the object to the object pool. Need to specify the size since when
     * deleting from an extent only part of the object should be put back
     * in the pool and repacked.
     */
    void add_obj(Extent_Object *object, int obj_size) {
        this->obj_pool.emplace_back(std::make_pair(object, obj_size));
    }

    /*
     * Add the objects in obj_lst to the object pool.
     */
    void add_objs(object_lst obj_lst) {
        this->obj_pool.insert(this->obj_pool.end(), obj_lst.begin(),
                              obj_lst.end());
    }

    /*
     * Returns true if extent is in current extents dict and false otherwise
     */
    bool is_ext_in_current_extents(Extent *extent) {
        for (auto &ext : this->current_exts)
            if (ext.second == extent)
                return true;
        return false;
    }

    void remove_extent_from_current_extents(Extent *extent) {
        for (auto &ext : this->current_exts) {
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
    string get_extent_type(Extent *extent) {
        // Find the largest object stored in the extent
        int largest_obj = INT32_MIN, local_max = 0, num_objs = 0;
        for (auto &tuple : extent->obj_ids_to_obj_size) {
            std::vector<int> sizes = tuple.second;
            local_max = std::accumulate(sizes.begin(), sizes.end(), 0);
            if (largest_obj < local_max) {
                largest_obj = local_max;
                num_objs = sizes.size();
            }
        }
        if (largest_obj >= this->threshold / 100.0 * extent->ext_size &&
            largest_obj < extent->ext_size) {
            double frac = largest_obj / extent->ext_size;
            return std::to_string(floor(frac) * 10) + "-" +
                   std::to_string(ceil(frac) * 10);
        } else if (largest_obj < this->threshold / 100.0 * extent->ext_size) {
            return "small";
        } else {
            return "large";
        }
    }

    void update_extent_type(Extent *extent) {
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
    virtual void
    generate_exts_at_key(shared_ptr<AbstractExtentStack> extent_stack,
                         int num_exts, int key) {
        int num_exts_at_key = extent_stack->get_length_at_key(key);
        while (num_exts_at_key < num_exts) {
            object_lst objs = this->obj_manager->create_new_object();
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
    virtual void generate_stripes(shared_ptr<AbstractExtentStack> extent_stack,
                                  int simulation_time) {
        if (this->obj_pool.size() < this->num_objs_in_pool) {
            object_lst objs = this->obj_manager->create_new_object(
                this->num_objs_in_pool - this->obj_pool.size());
            this->add_objs(objs);
        }
        this->pack_objects(extent_stack);
    }

    /*
     * Creates objects to fill the provided amount of space.
     */
    virtual void generate_objs(int space) {
        while (space > 0) {
            object_lst objs = this->obj_manager->create_new_object(1);
            space -= objs[0].second;
            this->add_objs(objs);
        }
    }

    void
    add_obj_to_current_ext_at_key(shared_ptr<AbstractExtentStack> extent_stack,
                                  Extent_Object *obj, int obj_rem_size,
                                  int key) {
        int temp = 0;
        if (this->current_exts.find(key) == this->current_exts.end())
            this->current_exts[key] = this->ext_manager->create_extent();
        Extent *current_ext = this->current_exts[key];

        while (obj_rem_size > 0) {
            temp = current_ext->add_object(obj, obj_rem_size);
            obj_rem_size -= temp;

            // Seal the extent if the extent is full
            if (obj_rem_size > 0 ||
                (obj_rem_size == 0 && current_ext->free_space == 0)) {
                this->update_extent_type(current_ext);
                current_ext->type = this->get_extent_type(current_ext);

                extent_stack->add_extent(key, this->current_exts[key]);
                current_ext = this->ext_manager->create_extent();
                this->current_exts[key] = current_ext;
            }
        }
    }

    /*
     * Method that retrieves all objects from object pool. Identifies the
     * corresponding key for each object in current_extents and calls
     * add_obj_to_current_ext_at_key.
     */
    virtual void pack_objects(shared_ptr<AbstractExtentStack> extent_stack) {};
    // TODO: extent_stack might need to be a pointer here
};

/*
 * Packs objects into extents. For now I am only making a place-holder for the
 * actual object that would pack objects based on a given optimal policy. In
 * this simple code I only add objects from the object_pool, to the
 * current_extents in a fifo scheme. As a result, this is equivalent to
 * having only a single current extent at a time.
 */
class SimpleObjectPacker : public GenericObjectPacker {

  public:
    SimpleObjectPacker() = delete;
    SimpleObjectPacker(shared_ptr<ObjectManager> obj_manager,
                       shared_ptr<ExtentManager> ext_manager,
                       object_lst obj_pool = object_lst(),
                       current_extents current_exts = current_extents(),
                       short num_objs_in_pool = 100, short threshold = 10,
                       bool record_ext_types = false)
        : GenericObjectPacker(obj_manager, ext_manager, obj_pool, current_exts,
                              num_objs_in_pool, threshold, record_ext_types) {}

    virtual void pack_objects(shared_ptr<AbstractExtentStack> extent_stack,
                              int key = 0) {
        while (this->obj_pool.size() > 0) {
            obj_record obj = this->obj_pool.back();
            this->obj_pool.pop_back();
            this->add_obj_to_current_ext_at_key(extent_stack, obj.first,
                                                obj.second, key);
        }
    };
};

class SimpleGCObjectPacker : public SimpleObjectPacker {
  public:
    SimpleGCObjectPacker() = delete;
    SimpleGCObjectPacker(shared_ptr<ObjectManager> obj_manager,
                         shared_ptr<ExtentManager> ext_manager,
                         object_lst obj_pool = object_lst(),
                         current_extents current_exts = current_extents(),
                         short num_objs_in_pool = 100, short threshold = 10,
                         bool record_ext_types = false)
        : SimpleObjectPacker(obj_manager, ext_manager, obj_pool, current_exts,
                             num_objs_in_pool, threshold, record_ext_types) {}

    /*
     * Can't generate a gc stripe on-demand.
     */
    virtual void generate_stripes(shared_ptr<AbstractExtentStack> extent_stack,
                                  int simulation_time) {
        return;
    }

    /*
     * Repacks objects from given extent
     */
    virtual void gc_extent(Extent *ext,
                           shared_ptr<AbstractExtentStack> extent_stack,
                           set<Extent_Object *> objs = set<Extent_Object *>()) {
        for (auto &obj : objs)
            this->add_obj(obj, ext->get_obj_size(obj));
        this->pack_objects(extent_stack, objs);
    }

    virtual void
    pack_objects(shared_ptr<AbstractExtentStack> extent_stack,
                 set<Extent_Object *> objs = set<Extent_Object *>(),
                 int key = 0) {
        while (this->obj_pool.size() > 0) {
            obj_record obj = this->obj_pool.back();
            this->obj_pool.pop_back();
            this->add_obj_to_current_ext_at_key(extent_stack, obj.first,
                                                obj.second, key);
        }
    }
};

/*
 * In this configuration, objects are divided into 4MB chunks and randomly
 * placed into extents. Pieces of the same object can end up in different
 * extents.
 */
class RandomizedObjectPacker : public SimpleObjectPacker {

  public:
    using SimpleObjectPacker::SimpleObjectPacker;

    void pack_objects(shared_ptr<AbstractExtentStack> extent_stack,
                      object_lst objs = object_lst(), int key = 0) {
        std::vector<Extent_Object *> objs_lst = {};
        for (auto &record : this->obj_pool) {
            int rem_size = record.second;
            for (int i = 0; i < rem_size / 4; i++)
                objs_lst.emplace_back(record.first);
        }
        this->obj_pool.clear();

        std::shuffle(objs_lst.begin(), objs_lst.end(),
                     std::default_random_engine(0));
        for (auto &record : objs_lst)
            this->add_obj_to_current_ext_at_key(extent_stack, record, 4, key);
    }
};

/*
 * In this configuration, objects are divided into 4MB chunks and randomly
 * placed into extents. Pieces of the same object can end up in different
 * extents.
 */
class RandomizedGCObjectPacker : public SimpleGCObjectPacker {

  public:
    using SimpleGCObjectPacker::SimpleGCObjectPacker;

    void pack_objects(shared_ptr<AbstractExtentStack> extent_stack,
                      set<Extent_Object *> objs, int key = 0) override {
        std::vector<Extent_Object *> objs_lst = {};
        for (auto &it : this->obj_pool) {
            int rem_size = it.second;
            for (int i = 0; i < rem_size / 4; i++)
                objs_lst.emplace_back(it.first);
        }
        this->obj_pool.clear();
        std::shuffle(objs_lst.begin(), objs_lst.end(),
                     std::default_random_engine(0));
        for (auto &it : objs_lst)
            this->add_obj_to_current_ext_at_key(extent_stack, it, 4, key);
    }
};

/*
 * This is for 4MB mixed pools config
 */
class RandomizedMixedObjectPacker : public SimpleObjectPacker {

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
                          int simulation_time = 365) override {
        int ave_pool_size = 46000;
        int curr_pool_size = 0;
        bool new_objs_added = false;

        for (auto &record : this->obj_pool)
            curr_pool_size += record.second;

        while (curr_pool_size < ave_pool_size) {
            object_lst objs = this->obj_manager->create_new_object();
            this->add_objs(objs);
            for (auto &record : objs)
                curr_pool_size += record.second;
            new_objs_added = true;
        }

        if (new_objs_added) {
            object_lst obj_lst = {};
            for (auto &record : this->obj_pool) {
                int rem_size = record.second;
                for (int i = 0; i < rem_size / 4 + round((rem_size % 4) / 4.0);
                     i++)
                    obj_lst.emplace_back(std::make_pair(record.first, 4));
                this->obj_pool.clear(); // TODO: Might not be necessary?
                this->obj_pool = obj_lst;
                std::shuffle(this->obj_pool.begin(), this->obj_pool.end(),
                             std::default_random_engine(0));
            }
        }

        this->pack_objects(extent_stack);
    }

    void pack_objects(shared_ptr<AbstractExtentStack> extent_stack,
                      int key = 0) override {
        for (auto record : this->obj_pool)
            this->add_obj_to_current_ext_at_key(extent_stack, record.first,
                                                record.second, key);
        this->obj_pool.clear();
    }

    /*
     * The method generates num_exts at the given key
     */
    void generate_exts_at_key(shared_ptr<AbstractExtentStack> extent_stack,
                              int num_exts, int key) override {
        // Check if the pool has enough object volume, and if not, generate
        // more fresh objects
        int ave_pool_size = 46000;
        int num_exts_at_key = extent_stack->get_length_at_key(key);
        int pool_size = 0;
        for (auto &record : this->obj_pool)
            pool_size += record.second;

        while (pool_size < ave_pool_size) {
            object_lst objs = this->obj_manager->create_new_object();
            this->add_objs(objs);
            for (auto &record : objs)
                pool_size += record.second;
        }

        // Mix valid and fresh objects
        object_lst obj_lst = {};
        for (auto &record : this->obj_pool) {
            int rem_size = record.second;
            for (int i = 0; i < rem_size / 4 + round((rem_size % 4) / 4.0); i++)
                obj_lst.emplace_back(std::make_pair(record.first, 4));
        }
        this->obj_pool.clear();
        this->obj_pool = obj_lst;
        std::shuffle(this->obj_pool.begin(), this->obj_pool.end(),
                     std::default_random_engine(0));

        while (num_exts_at_key < num_exts) {
            auto obj = this->obj_pool.front();
            this->obj_pool.erase(this->obj_pool.begin());
            for (auto &record : this->obj_pool)
                this->add_obj_to_current_ext_at_key(extent_stack, record.first,
                                                    record.second, key);
            num_exts_at_key = extent_stack->get_length_at_key(key);
        }
    }
};

class RandomizedMixedGCObjectPacker : public SimpleGCObjectPacker {

  public:
    using SimpleGCObjectPacker::SimpleGCObjectPacker;

    /*
     * Way to generate new objs during gc so that valid objs and new objs can
     * be mixed in joined pool.
     */
    void generate_exts() override {
        if (this->obj_pool.size() < this->num_objs_in_pool) {
            auto objs = this->obj_manager->create_new_object(
                this->num_objs_in_pool - this->obj_pool.size());
            this->add_objs(objs);
        }
    };

    /*
     * Repacks objects from given extent
     */
    void gc_extent(Extent *ext, shared_ptr<AbstractExtentStack> extent_stack,
                   set<Extent_Object *> objs) override {
        // TODO: We are adding this object without the notion of the extent
        // 		 shard. Would this cause any problems?
        for (auto obj : *ext->objects)
            this->add_obj(obj.first, ext->get_obj_size(obj.first));
    }
};

class MixedObjObjectPacker : public SimpleObjectPacker {
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
                          int simulation_time = 365) override {
        if (this->obj_pool.size() < this->num_objs_in_pool) {
            auto objs = this->obj_manager->create_new_object(
                this->num_objs_in_pool - this->obj_pool.size());
            this->add_objs(objs);
        }
        this->pack_objects(extent_stack);
    }

    void pack_objects(shared_ptr<AbstractExtentStack> extent_stack,
                      int key = 0) override {
        shuffle(this->obj_pool.begin(), this->obj_pool.end(),
                std::default_random_engine(0));
        for (int i = 0; i < this->obj_pool.size(); i++) {
            auto obj = this->obj_pool.front();
            this->add_obj_to_current_ext_at_key(extent_stack, obj.first,
                                                obj.second, key);
            this->obj_pool.erase(this->obj_pool.begin());
        }
    }

    void generate_exts_at_key(shared_ptr<AbstractExtentStack> extent_stack,
                              int num_exts, int key) override {
        int num_exts_at_key = extent_stack->get_length_at_key(key);
        std::shuffle(this->obj_pool.begin(), this->obj_pool.end(),
                     std::default_random_engine(0));
        while (num_exts_at_key < num_exts) {
            auto obj = this->obj_pool.front();
            this->add_obj_to_current_ext_at_key(extent_stack, obj.first,
                                                obj.second, key);
            this->obj_pool.erase(this->obj_pool.begin());
        }
    }
};

class MixedObjGCObjectPacker : public SimpleGCObjectPacker {

  public:
    using SimpleGCObjectPacker::SimpleGCObjectPacker;

    void generate_exts() override {
        if (this->obj_pool.size() < this->num_objs_in_pool) {
            auto objs = this->obj_manager->create_new_object(
                this->num_objs_in_pool - this->obj_pool.size());
            this->add_objs(objs);
        }
    }

    void generate_objs(int space) override {
        while (space > 0) {
            auto objs = this->obj_manager->create_new_object(1);
            space -= objs[0].second;
            this->add_objs(objs);
        }
    }

    /*
     * Repacks objects from given extent.
     */
    void gc_extent(Extent *ext, shared_ptr<AbstractExtentStack> extent_stack,
                   object_lst objs = object_lst()) {
        for (auto obj : *ext->objects)
            this->add_obj(obj.first, ext->get_obj_size(obj.first));
    }
};

class MortalImmortalObjectPacker : public SimpleObjectPacker {

    const int mortal_key = 1, immortal_key = 0;
    float percent_correct;

  public:
    using SimpleObjectPacker::SimpleObjectPacker;

    MortalImmortalObjectPacker(shared_ptr<ObjectManager> obj_manager,
                               shared_ptr<ExtentManager> ext_manager,
                               object_lst obj_pool = object_lst(),
                               current_extents current_exts = current_extents(),
                               short num_objs_in_pool = 100,
                               short threshold = 10,
                               float percent_correct = 100.0)
        : SimpleObjectPacker(obj_manager, ext_manager, obj_pool, current_exts,
                             num_objs_in_pool, threshold) {
        this->percent_correct = percent_correct / 100.0;
    }

    void pack_objects(shared_ptr<AbstractExtentStack> extent_stack,
                      int key = 0) override {
        std::mt19937_64 rd;
        auto time_seed = std::chrono::high_resolution_clock::now()
                             .time_since_epoch()
                             .count();
        rd.seed(time_seed);
        std::uniform_real_distribution<float> unif(0, 1);
        float p;

        while (this->obj_pool.size() > 0) {
            int key = immortal_key;
            auto obj = this->obj_pool.front();
            this->obj_pool.erase(this->obj_pool.begin());
            p = unif(rd);

            if ((obj.first->life <= 365 && p <= this->percent_correct) ||
                obj.first->life > 365 && p > this->percent_correct)
                key = mortal_key;

            this->add_obj_to_current_ext_at_key(extent_stack, obj.first,
                                                obj.second, key);
        }
    }
};

class MortalImmortalGCObjectPacker : public SimpleGCObjectPacker {

    const int mortal_key = 1, immortal_key = 0;
    float percent_correct;

  public:
    MortalImmortalGCObjectPacker(
        shared_ptr<ObjectManager> obj_manager,
        shared_ptr<ExtentManager> ext_manager,
        object_lst obj_pool = object_lst(),
        current_extents current_exts = current_extents(),
        short num_objs_in_pool = 100, short threshold = 10,
        float percent_correct = 100.0)
        : SimpleGCObjectPacker(obj_manager, ext_manager, obj_pool, current_exts,
                               num_objs_in_pool, threshold) {
        this->percent_correct = percent_correct / 100.0;
    }

    void pack_objects(shared_ptr<AbstractExtentStack> extent_stack,
                      int key = 0) override {
        std::mt19937_64 rd;
        auto time_seed = std::chrono::high_resolution_clock::now()
                             .time_since_epoch()
                             .count();
        rd.seed(time_seed);
        std::uniform_real_distribution<float> unif(0, 1);
        float p;

        while (this->obj_pool.size() > 0) {
            int key = immortal_key;
            auto obj = this->obj_pool.front();
            this->obj_pool.erase(this->obj_pool.begin());
            p = unif(rd);

            if ((obj.first->life <= 365 && p <= this->percent_correct) ||
                obj.first->life > 365 && p > this->percent_correct)
                key = mortal_key;

            this->add_obj_to_current_ext_at_key(extent_stack, obj.first,
                                                obj.second, key);
        }
    }
};

class SizeBasedObjectPacker : public virtual SimpleObjectPacker {

  public:
    using SimpleObjectPacker::SimpleObjectPacker;

    void adjust_index(int &ind, int &length) {
        ind = min(ind, length - 1);
        ind = max(0, ind);
    }

    int get_smaller_obj_index(int ext_size, int free_space) {
        int ind;
        if (ext_size == free_space) {
            ind = -1;
        } else {
            // TODO: Need to make sure that the upper_bound and lower_bound
            // 		 calls returns same indices as Python's bisect_left and
            //		 bisect_right function call
            auto obj_it = std::upper_bound(this->obj_pool.begin(),
                                           this->obj_pool.end(), free_space);
            ind = obj_it - this->obj_pool.begin() - 1;
            this->adjust_index(ind, obj_it->second);
        }
        return ind;
    }

    int get_larger_obj_index(int ext_size, int free_space) {
        int ind;
        if (ext_size == free_space) {
            ind = -1;
        } else {
            auto obj_it = std::lower_bound(this->obj_pool.begin(),
                                           this->obj_pool.end(), free_space);
            ind = obj_it - this->obj_pool.begin() - 1;
            this->adjust_index(ind, obj_it->second);
        }
        return ind;
    }

    void insert_obj_back_into_pool(Extent_Object *obj, int obj_size) {
        if (this->obj_pool.size() == 0) {
            this->obj_pool.emplace_back(std::make_pair(obj, obj_size));
            return;
        }

        int ind;
        auto obj_it = std::lower_bound(this->obj_pool.begin(),
                                       this->obj_pool.end(), obj_size);
        ind = obj_it - this->obj_pool.begin();
        this->adjust_index(ind, obj_it->second);

        this->obj_pool.insert(this->obj_pool.begin() + ind, *obj_it);
    }
};

/*
 * Keeps the objects together as much as possible. When selecting an object,
 * select the object slightly smaller than the available space in the extent.
 * If an object get split multiple extents, keep the object together
 * in a stripe for cross-extent erasure coding.
 */
class SizeBasedObjectPackerSmallerWholeObj : public SizeBasedObjectPacker {

  public:
    using SizeBasedObjectPacker::SizeBasedObjectPacker;

    std::pair<int, Extent *>
    add_obj_to_current_ext(shared_ptr<WholeObjectExtentStack> extent_stack,
                           Extent_Object *obj, int obj_rem_size, int key) {
        auto current_ext = this->current_exts[key];
        auto tmp = current_ext->add_object(obj, obj_rem_size);

        obj_rem_size = obj_rem_size - tmp;
        // seal the extent if the extent is full
        if (obj_rem_size > 0 || (!obj_rem_size && !current_ext->free_space)) {
            this->current_exts[key] = this->ext_manager->create_extent();
            this->update_extent_type(current_ext);

            current_ext->type = this->get_extent_type(current_ext);
            return std::make_pair(obj_rem_size, current_ext);
        }
        return std::make_pair(obj_rem_size, nullptr);
    }

    stack_val add_obj_to_current_ext_at_key(
        shared_ptr<WholeObjectExtentStack> extent_stack, Extent_Object *obj,
        int obj_rem_size, int key, ext_stack obj_ids_to_exts) {
        stack_val exts = stack_val();
        if (this->current_exts.find(key) == this->current_exts.end())
            this->current_exts[key] = this->ext_manager->create_extent();
        auto current_ext = this->current_exts[key];

        while (obj_rem_size >= current_ext->ext_size) {
            auto rem_size_and_ext = this->add_obj_to_current_ext(
                extent_stack, obj, obj_rem_size, key);
            current_ext = rem_size_and_ext.second;
            if (current_ext)
                exts.emplace_back(current_ext);
        }

        if (exts.size() > 0) {
            int obj_id = obj->id;
            if (obj_ids_to_exts.find(obj_id) == obj_ids_to_exts.end())
                obj_ids_to_exts[obj_id].insert(obj_ids_to_exts[obj_id].end(),
                                               exts.begin(), exts.end());
            else
                obj_ids_to_exts[obj_id] = exts;
        }

        auto rem_size_and_ext =
            this->add_obj_to_current_ext(extent_stack, obj, obj_rem_size, key);
        current_ext = rem_size_and_ext.second;

        if (current_ext) {
            exts.emplace_back(current_ext);
            int obj_id = current_ext->objects->begin()->first->id;
            if (obj_ids_to_exts.find(obj_id) != obj_ids_to_exts.end())
                obj_ids_to_exts[obj_id].insert(obj_ids_to_exts[obj_id].end(),
                                               exts.begin(), exts.end());
            else
                obj_ids_to_exts[obj_id] = exts;
        }

        if (obj_rem_size > 0)
            this->insert_obj_back_into_pool(obj, obj_rem_size);
        return exts;
    }

    void pack_objects(shared_ptr<AbstractExtentStack> extent_stack,
                      int key = 0) override {
        auto abs_ext_stack =
            std::static_pointer_cast<WholeObjectExtentStack>(extent_stack);
        stack_val exts = stack_val();
        ext_stack obj_ids_to_exts = ext_stack();

        // If there are any object in the current extent, place them
        // back in the pool.
        if (this->current_exts.find(key) != this->current_exts.end()) {
            auto objs = this->current_exts[key]->delete_ext();
            this->add_objs(objs);
        }

        // Lambda function used to sort the objects in the obj_pool.
        // It will try to sort by the  their size. If they are equivalent,
        // it will try to sort by the id.
        // TODO: From my interpretation of the original code, this is the
        // 		 intended method. But we should test this to ensure that
        // 		 we are getting the same result.
        auto obj_compare = [](obj_record o1, obj_record o2) {
            if (o1.second == o2.second)
                return *o1.first < *o2.first;
            return o1.second < o2.second;
        };
        std::sort(this->obj_pool.begin(), this->obj_pool.end(), obj_compare);

        while (this->obj_pool.size() > 0) {
            if (this->current_exts.find(key) == this->current_exts.end())
                this->current_exts[key] = this->ext_manager->create_extent();

            auto current_ext = this->current_exts[key];
            int free_space = current_ext->free_space;
            int ind =
                this->get_smaller_obj_index(current_ext->ext_size, free_space);

            auto obj = this->obj_pool[ind];
            this->obj_pool.erase(this->obj_pool.begin() + ind);

            exts = this->add_obj_to_current_ext_at_key(
                abs_ext_stack, obj.first, obj.second, key, obj_ids_to_exts);
        }

        for (auto mapping : obj_ids_to_exts)
            abs_ext_stack->add_extent(mapping.second);
    }
};

class SizeBasedGCObjectPackerSmallerWholeObj
    : public SizeBasedObjectPackerSmallerWholeObj,
      public SimpleGCObjectPacker {

  public:
    using SimpleGCObjectPacker::SimpleGCObjectPacker;

    void pack_objects(shared_ptr<AbstractExtentStack> extent_stack,
                      set<Extent_Object *> objs, int key = 0) override {
        auto abs_ext_stack =
            std::static_pointer_cast<WholeObjectExtentStack>(extent_stack);
        stack_val exts = stack_val();
        ext_stack obj_ids_to_exts = ext_stack();

        // If there are any object in the current extent, place them
        // back in the pool.
        if (SimpleGCObjectPacker::current_exts.find(key) !=
            SimpleGCObjectPacker::current_exts.end()) {
            auto objs = SimpleGCObjectPacker::current_exts[key]->delete_ext();
            SimpleGCObjectPacker::add_objs(objs);
        }

        // Lambda function used to sort the objects in the obj_pool.
        // It will try to sort by the  their size. If they are equivalent,
        // it will try to sort by the id.
        // TODO: From my interpretation of the original code, this is the
        // 		 intended method. But we should test this to ensure that
        // 		 we are getting the same result.
        auto obj_compare = [](obj_record o1, obj_record o2) {
            if (o1.second == o2.second)
                return *o1.first < *o2.first;
            return o1.second < o2.second;
        };
        std::sort(SimpleGCObjectPacker::obj_pool.begin(),
                  SimpleGCObjectPacker::obj_pool.end(), obj_compare);

        while (SimpleGCObjectPacker::obj_pool.size() > 0) {
            if (SimpleGCObjectPacker::current_exts.find(key) ==
                SimpleGCObjectPacker::current_exts.end())
                SimpleGCObjectPacker::current_exts[key] =
                    SimpleGCObjectPacker::ext_manager->create_extent();

            auto current_ext = SimpleGCObjectPacker::current_exts[key];
            int free_space = current_ext->free_space;
            int ind =
                this->get_smaller_obj_index(current_ext->ext_size, free_space);

            auto obj = SimpleGCObjectPacker::obj_pool[ind];
            SimpleGCObjectPacker::obj_pool.erase(
                SimpleGCObjectPacker::obj_pool.begin() + ind);

            SimpleGCObjectPacker::add_obj_to_current_ext_at_key(
                abs_ext_stack, obj.first, obj.second, key);
        }

        for (auto mapping : obj_ids_to_exts)
            abs_ext_stack->add_extent(mapping.second);
    }
};
class SizeBasedObjectPackerBaseline : public SimpleObjectPacker {
  public:
    using SimpleObjectPacker::SimpleObjectPacker;
};
class SizeBasedGCObjectPackerBaseline : public SimpleGCObjectPacker {
  public:
    using SimpleGCObjectPacker::SimpleGCObjectPacker;
};

class AgeBasedObjectPacker : public SimpleObjectPacker {
  public:
    using SimpleObjectPacker::SimpleObjectPacker;
};

class AgeBasedGCObjectPacker : public SimpleGCObjectPacker {
  public:
    using SimpleGCObjectPacker::SimpleGCObjectPacker;
};

class SizeBasedObjectPackerSmallerObj : public SimpleObjectPacker {
  public:
    using SimpleObjectPacker::SimpleObjectPacker;
};
class SizeBasedGCObjectPackerSmallerObj : public SimpleGCObjectPacker {
  public:
    using SimpleGCObjectPacker::SimpleGCObjectPacker;
};
class SizeBasedObjectPackerDynamicStrategy : public SimpleObjectPacker {
  public:
    using SimpleObjectPacker::SimpleObjectPacker;
};
class SizeBasedGCObjectPackerDynamicStrategy : public SimpleGCObjectPacker {
  public:
    using SimpleGCObjectPacker::SimpleGCObjectPacker;
};

class SizeBasedObjectPackerSmallerWholeObjFillGap : public SimpleObjectPacker {
  public:
    using SimpleObjectPacker::SimpleObjectPacker;
};
class SizeBasedGCObjectPackerSmallerWholeObjFillGap
    : public SimpleGCObjectPacker {
  public:
    using SimpleGCObjectPacker::SimpleGCObjectPacker;
};

class SizeBasedObjectPackerLargerWholeObj : public SimpleObjectPacker {
  public:
    using SimpleObjectPacker::SimpleObjectPacker;
};

class SizeBasedGCObjectPackerLargerWholeObj : public SimpleGCObjectPacker {
  public:
    using SimpleGCObjectPacker::SimpleGCObjectPacker;
};

class AgeBasedRandomizedObjectPacker : public SimpleObjectPacker {
  public:
    using SimpleObjectPacker::SimpleObjectPacker;
};

class AgeBasedRandomizedGCObjectPacker : public SimpleGCObjectPacker {
  public:
    using SimpleGCObjectPacker::SimpleGCObjectPacker;
};

class GenerationBasedObjectPacker : public SimpleObjectPacker {
  public:
    using SimpleObjectPacker::SimpleObjectPacker;
};

class GenerationBasedGCObjectPacker : public SimpleGCObjectPacker {
  public:
    using SimpleGCObjectPacker::SimpleGCObjectPacker;
};

#endif // __OBJECT_PACKER_H_