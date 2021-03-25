#ifndef __OBJECT_PACKER_H_
#define __OBJECT_PACKER_H_

#include <queue>
#pragma once

#include "extent_manager.h"
#include "extent_object_stripe.h"
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

bool operator<(const int v, const obj_record &record) {
    return v < record.second;
}

bool operator<(const obj_record &record, const int v) {
    return record.second < v;
}

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
    virtual void add_obj(obj_record r) = 0;

    /*
     * Method for packing objects into extents. Each packer decides on the
     * policy of how to pack objects into extents.
     */
    virtual void pack_objects(shared_ptr<AbstractExtentStack> extent_stack,
                              int key = 0){};
    virtual void pack_objects(shared_ptr<AbstractExtentStack> extent_stack,
                              std::set<ExtentObject *>){};
};

class GenericObjectPacker : public ObjectPacker {

  protected:
    shared_ptr<ObjectManager> obj_manager;
    shared_ptr<ExtentManager> ext_manager;
    std::optional<object_lst> obj_pool;
    current_extents current_exts;
    ext_types_mgr ext_types;
    short threshold, num_objs_in_pool;
    bool record_ext_types;

  public:
    GenericObjectPacker(
        shared_ptr<ObjectManager> obj_manager,
        shared_ptr<ExtentManager> ext_manager,
        std::optional<object_lst> obj_pool = std::optional<object_lst>(),
        current_extents current_exts = current_extents(),
        short num_objs_in_pool = 100, short threshold = 10,
        bool record_ext_types = false)
        : obj_manager(obj_manager), ext_manager(ext_manager),
          obj_pool(obj_pool), current_exts(current_exts),
          num_objs_in_pool(num_objs_in_pool), threshold(threshold),
          record_ext_types(record_ext_types) {
        this->ext_types = ext_types_mgr();
        srand(0);
    }
    current_extents get_current_exts() { return current_exts; }

    virtual void generate_exts() {}

    ext_types_mgr get_ext_types() { return ext_types; }
    /*
     * Add the object to the object pool. Need to specify the size since when
     * deleting from an extent only part of the object should be put back
     * in the pool and repacked.
     */
    void add_obj(obj_record r) override { obj_pool->emplace_back(r); }

    /*
     * Add the objects in obj_lst to the object pool.
     */
    void add_objs(object_lst obj_lst) {
        obj_pool->insert(obj_pool->end(), obj_lst.begin(), obj_lst.end());
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
        if (obj_pool->size() < this->num_objs_in_pool) {
            object_lst objs = this->obj_manager->create_new_object(
                this->num_objs_in_pool - obj_pool->size());
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

    virtual void
    add_obj_to_current_ext_at_key(shared_ptr<AbstractExtentStack> extent_stack,
                                  ExtentObject *obj, int obj_rem_size,
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
    using GenericObjectPacker::GenericObjectPacker;

    void pack_objects(shared_ptr<AbstractExtentStack> extent_stack,
                      int key = 0) override {
        while (obj_pool->size() > 0) {
            obj_record obj = obj_pool->back();
            obj_pool->pop_back();
            this->add_obj_to_current_ext_at_key(extent_stack, obj.first,
                                                obj.second, key);
        }
    };
};

class SimpleGCObjectPacker : public SimpleObjectPacker {
  public:
    SimpleGCObjectPacker() = delete;
    using SimpleObjectPacker::SimpleObjectPacker;

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
    virtual void
    gc_extent(Extent *ext, shared_ptr<AbstractExtentStack> extent_stack,
              std::set<ExtentObject *> objs = std::set<ExtentObject *>()) {
        for (auto &obj : objs)
            this->add_obj(obj_record(obj, ext->get_obj_size(obj)));
        this->pack_objects(extent_stack);
    }

    void pack_objects(shared_ptr<AbstractExtentStack> extent_stack,
                      int key = 0) override {
        while (obj_pool->size() > 0) {
            obj_record obj = obj_pool->back();
            obj_pool->pop_back();
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
        std::vector<ExtentObject *> objs_lst = {};
        for (auto &record : *obj_pool) {
            int rem_size = record.second;
            for (int i = 0; i < rem_size / 4; i++)
                objs_lst.emplace_back(record.first);
        }
        obj_pool->clear();

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
                      std::set<ExtentObject *> objs, int key = 0) override {
        std::vector<ExtentObject *> objs_lst = {};
        for (auto &it : *obj_pool) {
            int rem_size = it.second;
            for (int i = 0; i < rem_size / 4; i++)
                objs_lst.emplace_back(it.first);
        }
        obj_pool->clear();
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

        for (auto &record : *obj_pool)
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
            for (auto &record : *obj_pool) {
                int rem_size = record.second;
                for (int i = 0; i < rem_size / 4 + round((rem_size % 4) / 4.0);
                     i++)
                    obj_lst.emplace_back(std::make_pair(record.first, 4));
                obj_pool->clear(); // TODO: Might not be necessary?
                this->obj_pool = obj_lst;
                std::shuffle(obj_pool->begin(), obj_pool->end(),
                             std::default_random_engine(0));
            }
        }

        this->pack_objects(extent_stack);
    }

    void pack_objects(shared_ptr<AbstractExtentStack> extent_stack,
                      int key = 0) override {
        for (auto record : *obj_pool)
            this->add_obj_to_current_ext_at_key(extent_stack, record.first,
                                                record.second, key);
        obj_pool->clear();
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
        for (auto &record : *obj_pool)
            pool_size += record.second;

        while (pool_size < ave_pool_size) {
            object_lst objs = this->obj_manager->create_new_object();
            this->add_objs(objs);
            for (auto &record : objs)
                pool_size += record.second;
        }

        // Mix valid and fresh objects
        object_lst obj_lst = {};
        for (auto &record : *obj_pool) {
            int rem_size = record.second;
            for (int i = 0; i < rem_size / 4 + round((rem_size % 4) / 4.0); i++)
                obj_lst.emplace_back(std::make_pair(record.first, 4));
        }
        obj_pool->clear();
        this->obj_pool = obj_lst;
        std::shuffle(obj_pool->begin(), obj_pool->end(),
                     std::default_random_engine(0));

        while (num_exts_at_key < num_exts) {
            auto obj = obj_pool->front();
            obj_pool->erase(obj_pool->begin());
            for (auto &record : *obj_pool)
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
        if (obj_pool->size() < this->num_objs_in_pool) {
            auto objs = this->obj_manager->create_new_object(
                this->num_objs_in_pool - obj_pool->size());
            this->add_objs(objs);
        }
    };

    /*
     * Repacks objects from given extent
     */
    void gc_extent(Extent *ext, shared_ptr<AbstractExtentStack> extent_stack,
                   std::set<ExtentObject *> objs) override {
        // TODO: We are adding this object without the notion of the extent
        // 		 shard. Would this cause any problems?
        for (auto obj : *ext->objects)
            this->add_obj(obj_record(obj.first, ext->get_obj_size(obj.first)));
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
        if (obj_pool->size() < this->num_objs_in_pool) {
            auto objs = this->obj_manager->create_new_object(
                this->num_objs_in_pool - obj_pool->size());
            this->add_objs(objs);
        }
        this->pack_objects(extent_stack);
    }

    void pack_objects(shared_ptr<AbstractExtentStack> extent_stack,
                      int key = 0) override {
        shuffle(obj_pool->begin(), obj_pool->end(),
                std::default_random_engine(0));
        for (int i = 0; i < obj_pool->size(); i++) {
            auto obj = obj_pool->front();
            this->add_obj_to_current_ext_at_key(extent_stack, obj.first,
                                                obj.second, key);
            obj_pool->erase(obj_pool->begin());
        }
    }

    void generate_exts_at_key(shared_ptr<AbstractExtentStack> extent_stack,
                              int num_exts, int key) override {
        int num_exts_at_key = extent_stack->get_length_at_key(key);
        std::shuffle(obj_pool->begin(), obj_pool->end(),
                     std::default_random_engine(0));
        while (num_exts_at_key < num_exts) {
            auto obj = obj_pool->front();
            this->add_obj_to_current_ext_at_key(extent_stack, obj.first,
                                                obj.second, key);
            obj_pool->erase(obj_pool->begin());
        }
    }
};

class MixedObjGCObjectPacker : public SimpleGCObjectPacker {

  public:
    using SimpleGCObjectPacker::SimpleGCObjectPacker;

    void generate_exts() override {
        if (obj_pool->size() < this->num_objs_in_pool) {
            auto objs = this->obj_manager->create_new_object(
                this->num_objs_in_pool - obj_pool->size());
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
            this->add_obj(obj_record(obj.first, ext->get_obj_size(obj.first)));
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

        while (obj_pool->size() > 0) {
            int key = immortal_key;
            auto obj = obj_pool->front();
            obj_pool->erase(obj_pool->begin());
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

        while (obj_pool->size() > 0) {
            int key = immortal_key;
            auto obj = obj_pool->front();
            obj_pool->erase(obj_pool->begin());
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
            auto obj_it = std::upper_bound(obj_pool->begin(), obj_pool->end(),
                                           free_space);
            ind = obj_it - obj_pool->begin() - 1;
            this->adjust_index(ind, obj_it->second);
        }
        return ind;
    }

    int get_larger_obj_index(int ext_size, int free_space) {
        int ind;
        if (ext_size == free_space) {
            ind = -1;
        } else {
            auto obj_it = std::lower_bound(obj_pool->begin(), obj_pool->end(),
                                           free_space);
            ind = obj_it - obj_pool->begin() - 1;
            this->adjust_index(ind, obj_it->second);
        }
        return ind;
    }

    void insert_obj_back_into_pool(ExtentObject *obj, int obj_size) {
        if (obj_pool->size() == 0) {
            obj_pool->emplace_back(std::make_pair(obj, obj_size));
            return;
        }

        int ind;
        auto obj_it =
            std::lower_bound(obj_pool->begin(), obj_pool->end(), obj_size);
        ind = obj_it - obj_pool->begin();
        this->adjust_index(ind, obj_it->second);

        obj_pool->insert(obj_pool->begin() + ind, *obj_it);
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
                           ExtentObject *obj, int obj_rem_size, int key) {
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
        shared_ptr<WholeObjectExtentStack> extent_stack, ExtentObject *obj,
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
        std::sort(obj_pool->begin(), obj_pool->end(), obj_compare);

        while (obj_pool->size() > 0) {
            if (this->current_exts.find(key) == this->current_exts.end())
                this->current_exts[key] = this->ext_manager->create_extent();

            auto current_ext = this->current_exts[key];
            int free_space = current_ext->free_space;
            int ind =
                this->get_smaller_obj_index(current_ext->ext_size, free_space);

            auto obj = (*obj_pool)[ind];
            obj_pool->erase(obj_pool->begin() + ind);

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
                      std::set<ExtentObject *> objs, int key = 0) override {
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
        std::sort(SimpleGCObjectPacker::obj_pool->begin(),
                  SimpleGCObjectPacker::obj_pool->end(), obj_compare);

        while (SimpleGCObjectPacker::obj_pool->size() > 0) {
            if (SimpleGCObjectPacker::current_exts.find(key) ==
                SimpleGCObjectPacker::current_exts.end())
                SimpleGCObjectPacker::current_exts[key] =
                    SimpleGCObjectPacker::ext_manager->create_extent();

            auto current_ext = SimpleGCObjectPacker::current_exts[key];
            int free_space = current_ext->free_space;
            int ind =
                this->get_smaller_obj_index(current_ext->ext_size, free_space);

            auto obj = (*SimpleGCObjectPacker::obj_pool)[ind];
            SimpleGCObjectPacker::obj_pool->erase(
                SimpleGCObjectPacker::obj_pool->begin() + ind);

            SimpleGCObjectPacker::add_obj_to_current_ext_at_key(
                abs_ext_stack, obj.first, obj.second, key);
        }

        for (auto mapping : obj_ids_to_exts)
            abs_ext_stack->add_extent(mapping.second);
    }
};
inline bool greater(const obj_record &p1, const obj_record p2) {
    return p1.second > p2.second;
};
class SizeBasedObjectPackerBaseline : public SimpleObjectPacker {

  public:
    using SimpleObjectPacker::SimpleObjectPacker;
    void pack_objects(shared_ptr<AbstractExtentStack> extent_stack,
                      int key = 0) override {
        std::sort(obj_pool->begin(), obj_pool->end(), greater);
    };
};
class SizeBasedGCObjectPackerBaseline : public SimpleGCObjectPacker {
  public:
    using SimpleGCObjectPacker::SimpleGCObjectPacker;
    void pack_objects(shared_ptr<AbstractExtentStack> extent_stack,
                      std::set<ExtentObject *> objs, int key = 0) override {
        std::sort(obj_pool->begin(), obj_pool->end(), greater);
        while (!obj_pool->empty()) {
            obj_record record = obj_pool->back();
            obj_pool->pop_back();
            add_obj_to_current_ext_at_key(extent_stack, record.first,
                                          record.second, key);
        }
    };
};
typedef std::tuple<float, ExtentObject *, int> obj_pq_record;
class KeyBasedObjectPacker : public SimpleObjectPacker {
  protected:
    float (ExtentObject::*obj_key_fnc)();
    float (Extent::*ext_key_fnc)();
    std::priority_queue<obj_pq_record> obj_queue;

  public:
    KeyBasedObjectPacker(shared_ptr<ObjectManager> obj_manager,
                         shared_ptr<ExtentManager> ext_manager,
                         std::priority_queue<obj_pq_record> obj_q =
                             std::priority_queue<obj_pq_record>(),
                         current_extents current_exts = current_extents(),
                         short num_objs_in_pool = 100, short threshold = 10,
                         bool record_ext_types = false,
                         float (ExtentObject::*o_k_f)() = nullptr,
                         float (Extent::*e_k_f)() = nullptr)
        : SimpleObjectPacker(obj_manager, ext_manager,
                             std::optional<object_lst>(), current_exts,
                             num_objs_in_pool, threshold, record_ext_types),
          obj_key_fnc(o_k_f), ext_key_fnc(e_k_f), obj_queue(obj_q){};

    void generate_exts_at_key(shared_ptr<AbstractExtentStack> extent_stack,
                              int num_exts, int key) override {
        int num_exts_at_key = extent_stack->get_length_at_key(key);
        while (num_exts_at_key < num_exts) {
            object_lst objs = this->obj_manager->create_new_object();
            for (auto r : objs) {
                obj_queue.push(obj_pq_record(std::invoke(obj_key_fnc, r.first),
                                             r.first, r.first->size));
            }
        }
    }

    void add_obj(obj_record r) override {
        obj_queue.push(obj_pq_record(std::invoke(obj_key_fnc, r.first), r.first,
                                     r.first->size));
    }

    void generate_stripes(shared_ptr<AbstractExtentStack> extent_stack,
                          int simulation_time) override {
        if (obj_queue.size() < num_objs_in_pool) {
            object_lst objs = obj_manager->create_new_object(num_objs_in_pool -
                                                             obj_queue.size());
            for (obj_record r : objs) {
                add_obj(r);
            }
        }
    }

    void
    add_obj_to_current_ext_at_key(shared_ptr<AbstractExtentStack> extent_stack,
                                  ExtentObject *obj, int obj_rem_size,
                                  int key) override {
        int temp = 0;
        if (current_exts.find(key) == current_exts.end()) {
            current_exts[key] = ext_manager->create_extent();
        }
        Extent *current_extent = current_exts[key];
        int rem_size = obj_rem_size;
        while (rem_size > 0 ||
               (rem_size == 0 && current_extent->free_space == 0)) {
            temp = current_extent->add_object(obj, obj_rem_size);
            rem_size = rem_size - temp;
            if (rem_size > 0 ||
                (rem_size == 0 && current_extent->free_space == 0)) {
                extent_stack->add_extent(
                    std::invoke(ext_key_fnc, current_extent), current_extent);
                update_extent_type(current_extent);
                current_extent->type = get_extent_type(current_extent);
                current_extent = ext_manager->create_extent();
                current_exts[key] = current_extent;
            }
        }
    }
};

class KeyBasedGCObjectPacker : public SimpleGCObjectPacker {
  protected:
    float (ExtentObject::*obj_key_fnc)();
    float (Extent::*ext_key_fnc)();
    std::priority_queue<obj_pq_record> obj_queue;

  public:
    KeyBasedGCObjectPacker(shared_ptr<ObjectManager> obj_manager,
                           shared_ptr<ExtentManager> ext_manager,
                           std::priority_queue<obj_pq_record> obj_q =
                               std::priority_queue<obj_pq_record>(),
                           current_extents current_exts = current_extents(),
                           short num_objs_in_pool = 100, short threshold = 10,
                           bool record_ext_types = false,
                           float (ExtentObject::*o_k_f)() = nullptr,
                           float (Extent::*e_k_f)() = nullptr)
        : SimpleGCObjectPacker(obj_manager, ext_manager,
                               std::optional<object_lst>(), current_exts,
                               num_objs_in_pool, threshold, record_ext_types),
          obj_key_fnc(o_k_f), ext_key_fnc(e_k_f),
          obj_queue(std::priority_queue<obj_pq_record>()){};

    void generate_exts_at_key(shared_ptr<AbstractExtentStack> extent_stack,
                              int num_exts, int key) override {
        int num_exts_at_key = extent_stack->get_length_at_key(key);
        while (num_exts_at_key < num_exts) {
            object_lst objs = this->obj_manager->create_new_object();
            for (auto r : objs) {
                obj_queue.push(obj_pq_record(std::invoke(obj_key_fnc, r.first),
                                             r.first, r.first->size));
            }
        }
    }
    void gc_extent(
        Extent *ext, shared_ptr<AbstractExtentStack> extent_stack,
        std::set<ExtentObject *> objs = std::set<ExtentObject *>()) override {
        for (auto &obj : objs)
            this->add_obj(obj_record(obj, ext->get_obj_size(obj)));
        this->pack_objects(extent_stack, objs);
    }

    void add_obj(obj_record r) override {
        obj_queue.push(obj_pq_record(std::invoke(obj_key_fnc, r.first), r.first,
                                     r.first->size));
    }

    void generate_stripes(shared_ptr<AbstractExtentStack> extent_stack,
                          int simulation_time) override {}

    void
    add_obj_to_current_ext_at_key(shared_ptr<AbstractExtentStack> extent_stack,
                                  ExtentObject *obj, int obj_rem_size,
                                  int key) override {
        int temp = 0;
        if (current_exts.find(key) == current_exts.end()) {
            current_exts[key] = ext_manager->create_extent();
        }
        Extent *current_extent = current_exts[key];
        int rem_size = obj_rem_size;
        while (rem_size > 0 ||
               (rem_size == 0 && current_extent->free_space == 0)) {
            temp = current_extent->add_object(obj, obj_rem_size);
            rem_size = rem_size - temp;
            if (rem_size > 0 ||
                (rem_size == 0 && current_extent->free_space == 0)) {
                extent_stack->add_extent(
                    std::invoke(ext_key_fnc, current_extent), current_extent);
                update_extent_type(current_extent);
                current_extent->type = get_extent_type(current_extent);
                current_extent = ext_manager->create_extent();
                current_exts[key] = current_extent;
            }
        }
    }
};

class AgeBasedObjectPacker : public KeyBasedObjectPacker {
  public:
    AgeBasedObjectPacker(shared_ptr<ObjectManager> obj_manager,
                         shared_ptr<ExtentManager> ext_manager,
                         std::priority_queue<obj_pq_record> obj_pool,
                         current_extents current_exts = current_extents(),
                         short num_objs_in_pool = 100, short threshold = 10,
                         bool record_ext_types = false)
        : KeyBasedObjectPacker(obj_manager, ext_manager, obj_pool, current_exts,
                               num_objs_in_pool, threshold, record_ext_types,
                               &ExtentObject::get_timestamp,
                               &Extent::get_timestamp) {}
    void pack_objects(shared_ptr<AbstractExtentStack> extent_stack,
                      std::set<ExtentObject *> objs) override {
        while (!obj_queue.empty()) {
            obj_pq_record r = obj_queue.top();
            int key = 0;
            obj_queue.pop();
            add_obj_to_current_ext_at_key(extent_stack, std::get<1>(r),
                                          std::get<2>(r), key);
        }
    };
};

class AgeBasedGCObjectPacker : public KeyBasedGCObjectPacker {
  public:
    AgeBasedGCObjectPacker(shared_ptr<ObjectManager> obj_manager,
                           shared_ptr<ExtentManager> ext_manager,
                           std::priority_queue<obj_pq_record> obj_pool,
                           current_extents current_exts = current_extents(),
                           short num_objs_in_pool = 100, short threshold = 10,
                           bool record_ext_types = false)
        : KeyBasedGCObjectPacker(obj_manager, ext_manager, obj_pool,
                                 current_exts, num_objs_in_pool, threshold,
                                 record_ext_types, &ExtentObject::get_timestamp,
                                 &Extent::get_timestamp) {}
    void pack_objects(shared_ptr<AbstractExtentStack> extent_stack,
                      int key = 0) override {
        while (!obj_queue.empty()) {
            obj_pq_record r = obj_queue.top();
            obj_queue.pop();
            int key = 0;
            add_obj_to_current_ext_at_key(extent_stack, std::get<1>(r),
                                          std::get<2>(r), key);
        }
    };
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

class AgeBasedRandomizedObjectPacker : public AgeBasedObjectPacker {
  public:
    using AgeBasedObjectPacker::AgeBasedObjectPacker;
    void pack_objects(shared_ptr<AbstractExtentStack> extent_stack,
                      int key = 0) override {
        std::priority_queue<obj_pq_record> objs_lst =
            std::priority_queue<obj_pq_record>();
    }
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