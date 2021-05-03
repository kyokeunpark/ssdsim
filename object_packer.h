#ifndef __OBJECT_PACKER_H_
#define __OBJECT_PACKER_H_

#include <cstdio>
#include <iostream>
#include <memory>
#pragma once

#include "extent_manager.h"
#include "extent_object_stripe.h"
#include "extent_stack.h"
#include "object_manager.h"
#include "stripers.h"
#include "lock.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <queue>
#include <set>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

typedef std::tuple<float, obj_ptr, int> obj_pq_record;
using obj_pq = std::priority_queue<std::variant<obj_record, obj_pq_record>>;
using current_extents = std::unordered_map<int, ext_ptr >;
using ext_types_mgr = std::unordered_map<string, int>;

inline bool operator>(const obj_record &p1, const obj_record p2) {
  return p1.second > p2.second;
};
inline bool operator<(const obj_record &p1, const obj_record p2) {
  return p1.second < p2.second;
};

inline bool operator<(const float v, const obj_record &record) {
  return v < record.second;
}

inline bool operator<(const obj_record &record, const float v) {
  return record.second < v;
}

inline bool lower_bound_cmpr(const float v, const obj_record &record) {
  return v < record.second;
}
inline bool upper_bound_cmpr(const float v, const obj_record &record) {
  return v > record.second;
}
inline bool obj_record_asc_rem_size(const obj_record &p1, const obj_record p2) {
  return p1.second > p2.second;
};
inline bool obj_record_asc_rem_size_extent(const obj_record &p1, const obj_record p2) {
  if(p1.second == p2.second)
  {
    return p1.first > p2.first;
  }
  return p1.second > p2.second;
};

inline bool obj_record_desc_rem_size(const obj_record &p1, const obj_record p2) {
  return p1.second < p2.second;
};
inline bool obj_record_desc_rem_size_extent(const obj_record &p1, const obj_record p2) {
  if(p1.second == p2.second)
  {
    return p1.first < p2.first;
  }
  return p1.second < p2.second;
};
/* Interface for ObjectPackers to follow
 */
class ObjectPacker {
protected:
  shared_ptr<mutex> mtx = nullptr;

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
                            std::set<obj_ptr>& objs, float key = 0){};
};

class GenericObjectPacker : public ObjectPacker {

protected:
  shared_ptr<ObjectManager> obj_manager;
  shared_ptr<ExtentManager> ext_manager;
  shared_ptr<object_lst> obj_pool;
  shared_ptr<current_extents> current_exts;
  ext_types_mgr ext_types;
  short threshold, num_objs_in_pool;
  bool record_ext_types;

public:
  GenericObjectPacker(
      shared_ptr<ObjectManager> obj_manager,
      shared_ptr<ExtentManager> ext_manager,
      shared_ptr<object_lst> obj_pool,
      shared_ptr<current_extents> current_exts,
      short num_objs_in_pool = 100, short threshold = 10,
      bool record_ext_types = false, bool is_threaded = false)
      : obj_manager(obj_manager), ext_manager(ext_manager), obj_pool(obj_pool),
        current_exts(current_exts), num_objs_in_pool(num_objs_in_pool),
        threshold(threshold), record_ext_types(record_ext_types) {
    this->ext_types = ext_types_mgr();
    srand(0);
    if (is_threaded)
      this->mtx = make_shared<mutex>();
  }
  shared_ptr<current_extents> get_current_exts() { return current_exts; }

  virtual void generate_exts() {std::cerr<<"should never be called GenericObjectPacker generatae_exts()"<<std::endl;}

  ext_types_mgr get_ext_types() { return ext_types; }
  /*
   * Add the object to the object pool. Need to specify the size since when
   * deleting from an extent only part of the object should be put back
   * in the pool and repacked.
   */
  void add_obj(obj_record r) override {
    obj_pool->emplace_back(r);
  }

  /*
   * Add the objects in obj_lst to the object pool.
   */
  virtual void add_objs(object_lst obj_lst) {
    obj_pool->reserve( obj_pool->size() + obj_lst.size() );
    obj_pool->insert(obj_pool->end(), obj_lst.begin(), obj_lst.end());
  }

  /*
   * Returns true if extent is in current extents dict and false otherwise
   */
  bool is_ext_in_current_extents(ext_ptr extent) {
    lock(this->mtx);
    for (auto &ext : *this->current_exts) {
      if (ext.second == extent) {
        unlock(this->mtx);
        return true;
      }
    }
    unlock(this->mtx);
    return false;
  }

  void remove_extent_from_current_extents(ext_ptr extent) {
    lock(this->mtx);
    for (auto &ext : *this->current_exts) {
      if (ext.second == extent) {
        (*current_exts)[ext.first] = ext_manager->create_extent();
        unlock(this->mtx);
        return;
      }
    }
    unlock(this->mtx);
  }

  /*
   * Returns the extent type based on its largest object. Right now large
   * extents have objects that occupy the whole extent (i.e., object is
   * larger than one extent). Small obj extents that have objects that are
   * smaller than the gc_threshold. The rest are defined by the percentage
   * occupancy of the extent by the largest object.
   */
  string get_extent_type(ext_ptr extent) {
    // Find the largest object stored in the extent
    float largest_obj = -1, local_max = 0;
    for (auto &tuple : extent->objects) {
      std::vector<float> sizes = tuple.second;
      local_max = std::accumulate(sizes.begin(), sizes.end(), 0);
      if (largest_obj < local_max) {
        largest_obj = local_max;
      }
    }
    if (largest_obj >= this->threshold / 100.0 * extent->ext_size &&
        largest_obj < extent->ext_size) {
      double frac = largest_obj / extent->ext_size * 10;
      return std::to_string(int(floor(frac) * 10)) + "-" +
             std::to_string(int(ceil(frac) * 10));
    } else if (largest_obj < this->threshold / 100.0 * extent->ext_size) {
      return "small";
    } else {
      return "large";
    }
  }

  void update_extent_type(ext_ptr extent) {
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
                       int num_exts, float key) {
    int num_exts_at_key = extent_stack->get_length_at_key(key);
    while (num_exts_at_key < num_exts) {
      object_lst objs = this->obj_manager->create_new_object();
      lock(this->mtx);
      this->add_objs(objs);
      unlock(this->mtx);
      auto temp = std::set<obj_ptr>();
      this->pack_objects(extent_stack, temp);
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
                                float simulation_time) {
    lock(this->mtx);
    if (obj_pool->size() < this->num_objs_in_pool) {
      object_lst objs = this->obj_manager->create_new_object(
          this->num_objs_in_pool - obj_pool->size());
      this->add_objs(objs);
    }
    unlock(this->mtx);
    auto temp = std::set<obj_ptr>();
    this->pack_objects(extent_stack, temp);
  }

  /*
   * Creates objects to fill the provided amount of space.
   */
  virtual void generate_objs(double space) {
    while (space > 0) {
      lock(this->mtx);
      object_lst objs = this->obj_manager->create_new_object(1);
      space -= objs[0].second;
      this->add_objs(objs);
      unlock(this->mtx);
    }
  }

  /*
   * The caller should be holding onto the mtx lock
   */
  virtual void
  add_obj_to_current_ext_at_key(shared_ptr<AbstractExtentStack> extent_stack,
                                obj_ptr obj, float obj_rem_size, float key) {
    float temp = 0;
    if (this->current_exts->find(key) == this->current_exts->end())
      this->current_exts->emplace(key, this->ext_manager->create_extent());
    ext_ptr current_ext = (*this->current_exts)[key];

    while (obj_rem_size > 0) {
      temp = current_ext->add_object(obj, obj_rem_size);
      obj_rem_size -= temp;

      // Seal the extent if the extent is full
      if (obj_rem_size > 0 ||
          (obj_rem_size == 0 && current_ext->free_space <= 0)) {
        this->update_extent_type(current_ext);
        current_ext->type = this->get_extent_type(current_ext);
        extent_stack->add_extent(key, (*this->current_exts)[key]);
        current_ext = this->ext_manager->create_extent();
        (*this->current_exts)[key] = current_ext;
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

  void pack_objects(shared_ptr<AbstractExtentStack> extent_stack, std::set<obj_ptr>& objs,
                    float key = 0) override {
    lock(this->mtx);
    while (obj_pool->size() > 0) {
      obj_record obj = obj_pool->back();
      obj_pool->pop_back();
      this->add_obj_to_current_ext_at_key(extent_stack, obj.first,
                                          obj.second, key);
    }
    unlock(this->mtx);
  };
  virtual void
  gc_extent(ext_ptr ext, shared_ptr<AbstractExtentStack> extent_stack,
            std::set<obj_ptr>& objs) {
    std::cerr << "simple object packer virutla gc for easy calling. "
              << "shouldnt be triggered!";
  }
};

class SimpleGCObjectPacker : public SimpleObjectPacker {
public:
  SimpleGCObjectPacker() = delete;
  using SimpleObjectPacker::SimpleObjectPacker;

  /*
   * Can't generate a gc stripe on-demand.
   */
  virtual void generate_stripes(shared_ptr<AbstractExtentStack> extent_stack,
                                float simulation_time) override {
    return;
  }

  /*
   * Repacks objects from given extent
   */
  virtual void
  gc_extent(ext_ptr ext, shared_ptr<AbstractExtentStack> extent_stack,
            std::set<obj_ptr>& objs) override {
    lock(this->mtx);
    for (auto obj_kv : ext->objects) {
      auto obj = obj_kv.first;
      float size = ext->get_obj_size(obj);
      this->add_obj(obj_record(obj, size));
    }
    this->pack_objects(extent_stack, objs);
    unlock(this->mtx);
  }

  void pack_objects(shared_ptr<AbstractExtentStack> extent_stack,
                    std::set<obj_ptr>& objs, float key = 0) override {
    while (obj_pool->size() > 0) {
      obj_record obj = obj_pool->back();
      obj_pool->pop_back();
      this->add_obj_to_current_ext_at_key(extent_stack, obj.first, obj.second,
                                          key);
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
                    std::set<obj_ptr>& objs, float key = 0) override {
    vector<obj_ptr> objs_lst = {};
    for (auto &record : *obj_pool) {
      float rem_size = record.second;
      for (int i = 0; i < rem_size / 4; i++)
        objs_lst.emplace_back(record.first);
    }
    object_lst empty_lst;
    obj_pool->swap(empty_lst);

    std::shuffle(objs_lst.begin(), objs_lst.end(), generator);
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
                    std::set<obj_ptr>& objs, float key = 0) override {
    std::vector<obj_ptr> objs_lst = {};
    for (auto &it : *obj_pool) {
      float rem_size = it.second;
      for (int i = 0; i < rem_size / 4; i++)
        objs_lst.emplace_back(it.first);
    }
    object_lst empty_lst;
    obj_pool->swap(empty_lst);
    std::shuffle(objs_lst.begin(), objs_lst.end(), generator);
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
   * how to divide the extent stack into stripes (i.e., can extents
   * belonging to one key mix with extents from another - for example for
   * extent size they don't, for generations and lifetime they do).
   */
  void generate_stripes(shared_ptr<AbstractExtentStack> extent_stack,
                        float simulation_time = 365) override {
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
      object_lst* obj_lst =new object_lst();
      for (auto &record : *obj_pool) {
        float rem_size = record.second;
        for (int i = 0; i < rem_size / 4 + round((fmod(rem_size, 4)) / 4.0); i++)
          obj_lst->emplace_back(std::make_pair(record.first, 4));
        obj_pool->clear(); // TODO: Might not be necessary?
        this->obj_pool.reset(obj_lst);
        std::shuffle(obj_pool->begin(), obj_pool->end(),
                     generator);
      }
    }
    auto temp = std::set<obj_ptr>();
    this->pack_objects(extent_stack, temp);
  }

  void pack_objects(shared_ptr<AbstractExtentStack> extent_stack, std::set<obj_ptr>& objs,
                    float key = 0) override {
    for (auto record : *obj_pool)
      this->add_obj_to_current_ext_at_key(extent_stack, record.first,
                                          record.second, key);
    obj_pool->clear();
  }

  /*
   * The method generates num_exts at the given key
   */
  void generate_exts_at_key(shared_ptr<AbstractExtentStack> extent_stack,
                            int num_exts, float key) override {
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
    object_lst * obj_lst = new object_lst();
    for (auto &record : *obj_pool) {
      float rem_size = record.second;
      for (int i = 0; i < rem_size / 4.0 + round(fmod(rem_size, 4) / 4.0); i++)
        obj_lst->emplace_back(std::make_pair(record.first, 4));
    }
    obj_pool->clear();
    this->obj_pool.reset(obj_lst);
    std::shuffle(obj_pool->begin(), obj_pool->end(),
                 generator);

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
   * Way to generate new objs during gc so that valid objs and new objs
   * can be mixed in joined pool.
   */
  void generate_exts() override {
    if (obj_pool->size() < this->num_objs_in_pool) {
      auto objs = this->obj_manager->create_new_object(this->num_objs_in_pool -
                                                       obj_pool->size());
      this->add_objs(objs);
    }
  };

  /*
   * Repacks objects from given extent
   */
  void gc_extent(ext_ptr ext, shared_ptr<AbstractExtentStack> extent_stack,
                 std::set<obj_ptr>& objs) override {
    // TODO: We are adding this object without the notion of the extent
    // 		 shard. Would this cause any problems?
    for (auto obj : ext->objects)
      this->add_obj(obj_record(obj.first, ext->get_obj_size(obj.first)));
  }
};

class MixedObjObjectPacker : public SimpleObjectPacker {
public:
  using SimpleObjectPacker::SimpleObjectPacker;

  /*
   * This method will ensure that the extent stack has enough extents to
   * create the required number of stripes. Each subclass has to determine
   * how to divide th extent stack into stripes (i.e., can extents
   * belonging to one key mix with extents from another - for example, for
   * extent size they don't, for generations and lifetime they do).
   */
  void generate_stripes(shared_ptr<AbstractExtentStack> extent_stack,
                        float simulation_time = 365) override {
    if (obj_pool->size() < this->num_objs_in_pool) {
      auto objs = this->obj_manager->create_new_object(this->num_objs_in_pool -
                                                       obj_pool->size());
      this->add_objs(objs);
    }
    // std::cout << "obj_pool_size" << obj_pool->size() << std::endl;
    auto temp = std::set<obj_ptr>();
    this->pack_objects(extent_stack, temp);
  }

  void pack_objects(shared_ptr<AbstractExtentStack> extent_stack, std::set<obj_ptr>& objs,
                    float key = 0) override {
    shuffle(obj_pool->begin(), obj_pool->end(), generator);
    while(!obj_pool->empty()){
      auto obj = obj_pool->front();
      this->add_obj_to_current_ext_at_key(extent_stack, obj.first, obj.second,
                                          key);
      obj_pool->erase(obj_pool->begin());
    }
  }
 
  void generate_exts_at_key(shared_ptr<AbstractExtentStack> extent_stack,
                            int num_exts, float key) override {
    int num_exts_at_key = extent_stack->get_length_at_key(key);
    std::shuffle(obj_pool->begin(), obj_pool->end(),
                 generator);
    // std::cout << "obj_pool_size before generate_exts_at_key" << obj_pool->size() << std::endl;
    // std::cout << "num_exts_at_key" << num_exts_at_key << " " << key << std::endl;
    // std::cout << "num_exts" << num_exts << std::endl;
    while (num_exts_at_key < num_exts) {
      auto obj = obj_pool->front();
      this->add_obj_to_current_ext_at_key(extent_stack, obj.first, obj.second,
                                          key);
      obj_pool->erase(obj_pool->begin());
      num_exts_at_key = extent_stack->get_length_at_key(key);
    }
  }
};

class MixedObjGCObjectPacker : public SimpleGCObjectPacker {

public:
  using SimpleGCObjectPacker::SimpleGCObjectPacker;

  void generate_exts() override {
    if (obj_pool->size() < this->num_objs_in_pool) {
      auto objs = this->obj_manager->create_new_object(this->num_objs_in_pool -
                                                       obj_pool->size());
      this->add_objs(objs);
    }
  }

  void generate_objs(double space) override {
    while (space > 0) {
      auto objs = this->obj_manager->create_new_object(1);
      space -= objs[0].second;
      this->add_objs(objs);
    }
  }

  /*
   * Repacks objects from given extent.
   */
    void gc_extent(ext_ptr ext, shared_ptr<AbstractExtentStack> extent_stack,
            std::set<obj_ptr>& objs) override {
    for (auto obj : ext->objects)
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
                             shared_ptr<object_lst> obj_pool,
                             shared_ptr<current_extents> current_exts,
                             short num_objs_in_pool = 100, short threshold = 10,
                             float percent_correct = 100.0)
      : SimpleObjectPacker(obj_manager, ext_manager, obj_pool, current_exts,
                           num_objs_in_pool, threshold) {
    this->percent_correct = percent_correct / 100.0;
  }

  void pack_objects(shared_ptr<AbstractExtentStack> extent_stack, std::set<obj_ptr>& objs,
                    float key = 0) override {
    std::uniform_real_distribution<float> unif(0, 1);
    float p;

    while (obj_pool->size() > 0) {
      float key = immortal_key;
      auto obj = obj_pool->front();
      obj_pool->erase(obj_pool->begin());
      p = unif(generator);

      if ((obj.first->life <= 365 && p <= this->percent_correct) ||
          obj.first->life > 365 && p > this->percent_correct)
        key = mortal_key;

      this->add_obj_to_current_ext_at_key(extent_stack, obj.first, obj.second,
                                          key);
    }
  }
};

class MortalImmortalGCObjectPacker : public SimpleGCObjectPacker {

  const int mortal_key = 1, immortal_key = 0;
  float percent_correct;

public:
  MortalImmortalGCObjectPacker(shared_ptr<ObjectManager> obj_manager,
                               shared_ptr<ExtentManager> ext_manager,
                               shared_ptr<object_lst> obj_pool,
                               shared_ptr<current_extents> current_exts,
                               short num_objs_in_pool = 100,
                               short threshold = 10,
                               float percent_correct = 100.0)
      : SimpleGCObjectPacker(obj_manager, ext_manager, obj_pool, current_exts,
                             num_objs_in_pool, threshold) {
    this->percent_correct = percent_correct / 100.0;
  }

  void pack_objects(shared_ptr<AbstractExtentStack> extent_stack, std::set<obj_ptr>& objs,
                    float key = 0) override {
   
    std::uniform_real_distribution<float> unif(0, 1);
    float p;

    while (obj_pool->size() > 0) {
      float key = immortal_key;
      auto obj = obj_pool->front();
      obj_pool->erase(obj_pool->begin());
      p = unif(generator);

      if ((obj.first->life <= 365 && p <= this->percent_correct) ||
          obj.first->life > 365 && p > this->percent_correct)
        key = mortal_key;

      this->add_obj_to_current_ext_at_key(extent_stack, obj.first, obj.second,
                                          key);
    }
  }
};

class SizeBasedObjectPacker : public virtual SimpleObjectPacker {

public:
  using SimpleObjectPacker::SimpleObjectPacker;

  void adjust_index(int &ind, int length) {
    ind = ind <= length - 1 ?ind:length - 1;
    ind = ind >= 0 ? ind: 0;
  }

  int get_smaller_obj_index(int ext_size, float free_space) {
    int ind;
    if (ext_size == free_space) {
      ind = -1;
    } else {
      // TODO: Need to make sure that the upper_bound and lower_bound
      // 		 calls returns same indices as Python's bisect_left
      // and
      //		 bisect_right function call
      auto obj_it =
          std::upper_bound(obj_pool->begin(), obj_pool->end(), free_space);
      while(obj_it != obj_pool->begin() && obj_it->second > free_space)
      {
        obj_it--;
      }
      ind = obj_it - obj_pool->begin();
      this->adjust_index(ind, obj_pool->size());
      // std::cout << "ind" << ind << "free_space" << free_space << "obj_size" << (obj_pool->begin() + ind)->second << "obj_pool size" << obj_pool->size() << std::endl;
    }
    return ind;
  }

  int get_larger_obj_index(int ext_size, float free_space) {
    int ind;
    if (ext_size == free_space) {
      ind = -1;
    } else {
      auto obj_it =
          std::lower_bound(obj_pool->begin(), obj_pool->end(), free_space);
      ind = obj_it - obj_pool->begin();
      this->adjust_index(ind, obj_pool->size());
    }
    return ind;
  }

  void insert_obj_back_into_pool(obj_ptr obj, float obj_size) {
    if (obj_pool->size() == 0) {
      obj_pool->emplace_back(std::make_pair(obj, obj_size));
      return;
    }

    int ind;
    auto obj_it =
        std::lower_bound(obj_pool->begin(), obj_pool->end(), obj_size);
    ind = obj_it - obj_pool->begin();
    this->adjust_index(ind, obj_it->second);
    obj_pool->insert(obj_pool->begin() + ind, obj_record(obj, obj_size));
  }
};

/*
 * Keeps the objects together as much as possible. When selecting an object,
 * select the object slightly smaller than the available space in the
 * extent. If an object get split multiple extents, keep the object together
 * in a stripe for cross-extent erasure coding.
 */
class SizeBasedObjectPackerSmallerWholeObj : public SizeBasedObjectPacker {

public:
  using SizeBasedObjectPacker::SizeBasedObjectPacker;

  std::pair<float, ext_ptr >
  add_obj_to_current_ext(shared_ptr<AbstractExtentStack> extent_stack,
                         obj_ptr obj, float obj_rem_size, float key) {
    auto current_ext = (*this->current_exts)[key];
    auto tmp = current_ext->add_object(obj, obj_rem_size);

    obj_rem_size = obj_rem_size - tmp;
    // seal the extent if the extent is full
    if (obj_rem_size > 0 || (obj_rem_size == 0 && current_ext->free_space == 0)) {
      (*this->current_exts)[key] = this->ext_manager->create_extent();
      this->update_extent_type(current_ext);

      current_ext->type = this->get_extent_type(current_ext);
      return std::make_pair(obj_rem_size, current_ext);
    }
    return std::make_pair(obj_rem_size, nullptr);
  }

  stack_val
  add_obj_to_current_ext_at_key(shared_ptr<AbstractExtentStack> extent_stack,
                                obj_ptr obj, float rem_size, float key,
                                ext_stack& obj_ids_to_exts) {
    stack_val exts = stack_val();
    if (this->current_exts->find(key) == this->current_exts->end())
      (*this->current_exts)[key] = this->ext_manager->create_extent();
    auto current_ext = (*this->current_exts)[key];
    float obj_rem_size = rem_size;
    while (obj_rem_size >= current_ext->ext_size) {
      auto rem_size_and_ext =
          this->add_obj_to_current_ext(extent_stack, obj, obj_rem_size, key);
      obj_rem_size = rem_size_and_ext.first;
      current_ext = rem_size_and_ext.second;
      if (current_ext != nullptr)
        exts.emplace_back(current_ext);
    }
    // if(current_ext == nullptr)
    // {
    //   std::cout << "current_ext nullptr" << std::endl;
    // }
    if (exts.size() > 0) {
      int obj_id = obj->id;
      if (obj_ids_to_exts.find(obj_id) != obj_ids_to_exts.end())
        obj_ids_to_exts[obj_id].insert(obj_ids_to_exts[obj_id].end(),
                                       exts.begin(), exts.end());
      else
        obj_ids_to_exts[obj_id] = exts;
    }

    auto rem_size_and_ext =
        this->add_obj_to_current_ext(extent_stack, obj, obj_rem_size, key);
    current_ext = rem_size_and_ext.second;
    obj_rem_size = rem_size_and_ext.first;
    if (current_ext != nullptr) {
      exts.emplace_back(current_ext);
      int obj_id = current_ext->objects.begin()->first->id;
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

  void pack_objects(shared_ptr<AbstractExtentStack> extent_stack, std::set<obj_ptr>& objs,
                    float key = 0) override {
    
    stack_val exts = stack_val();
    ext_stack obj_ids_to_exts = ext_stack();

    // If there are any object in the current extent, place them
    // back in the pool.
    if (this->current_exts->find(key) != this->current_exts->end()) {
      auto objs = (*this->current_exts)[key]->delete_ext();
      this->add_objs(objs);
    }
    std::sort(obj_pool->begin(), obj_pool->end(), obj_record_desc_rem_size_extent);

    while (obj_pool->size() > 0) {
      if (this->current_exts->find(key) == this->current_exts->end())
        (*this->current_exts)[key] = this->ext_manager->create_extent();

      auto current_ext = (*this->current_exts)[key];
      float free_space = current_ext->free_space;
      int ind = this->get_smaller_obj_index(current_ext->ext_size, free_space);
      if(ind == -1)
      {
        ind = obj_pool->size() - 1;
      }
      auto obj = (*obj_pool)[ind];
      obj_pool->erase(obj_pool->begin() + ind);

      exts = this->add_obj_to_current_ext_at_key(
          extent_stack, obj.first, obj.second, key, obj_ids_to_exts);
    }

    for (auto mapping : obj_ids_to_exts)
      extent_stack->add_extent(mapping.second);
  }
};

class SizeBasedGCObjectPackerSmallerWholeObj
    : public SizeBasedObjectPackerSmallerWholeObj {

public:
  using SizeBasedObjectPackerSmallerWholeObj::
      SizeBasedObjectPackerSmallerWholeObj;

  void pack_objects(shared_ptr<AbstractExtentStack> extent_stack, std::set<obj_ptr>& objs,
                    float key = 0) override {
    auto abs_ext_stack = extent_stack;
    stack_val exts = stack_val();
    ext_stack obj_ids_to_exts = ext_stack();

    // If there are any object in the current extent, place them
    // back in the pool.
    if (current_exts->find(key) != current_exts->end()) {
      auto objs = (*current_exts)[key]->delete_ext();
      add_objs(objs);
    }

    // Lambda function used to sort the objects in the obj_pool->
    // It will try to sort by the  their size. If they are equivalent,
    // it will try to sort by the id.
    // TODO: From my interpretation of the original code, this is the
    // 		 intended method. But we should test this to ensure that
    // 		 we are getting the same result.
    std::sort(obj_pool->begin(), obj_pool->end(), obj_record_desc_rem_size_extent);

    while (obj_pool->size() > 0) {
      if (current_exts->find(key) == current_exts->end())
        (*current_exts)[key] = ext_manager->create_extent();

      auto current_ext = (*current_exts)[key];
      float free_space = current_ext->free_space;
      int ind = this->get_smaller_obj_index(current_ext->ext_size, free_space);
      if(ind == -1)
      {
        ind = obj_pool->size() - 1;
      }
      
      auto obj = (*obj_pool)[ind];
      obj_pool->erase(obj_pool->begin() + ind);
      add_obj_to_current_ext_at_key(abs_ext_stack, obj.first, obj.second, key,
                                    obj_ids_to_exts);
    }

    for (auto mapping : obj_ids_to_exts)
      abs_ext_stack->add_extent(mapping.second);
  }
  void generate_stripes(shared_ptr<AbstractExtentStack> extent_stack,
                                float simulation_time) override {
    return;
  }
  void
  gc_extent(ext_ptr ext, shared_ptr<AbstractExtentStack> extent_stack,
            std::set<obj_ptr>& objs) override {
    for (auto obj_kv : ext->objects) {
      auto obj = obj_kv.first;
      float size = ext->get_obj_size(obj);
      this->add_obj(obj_record(obj, size));
    }
    this->pack_objects(extent_stack, objs);
  }
};

class SizeBasedObjectPackerBaseline : public SimpleObjectPacker {

public:
  using SimpleObjectPacker::SimpleObjectPacker;
  void pack_objects(shared_ptr<AbstractExtentStack> extent_stack, std::set<obj_ptr>& objs,
                    float key = 0) override {
    std::sort(obj_pool->begin(), obj_pool->end(), obj_record_asc_rem_size);
    while (!obj_pool->empty()) {
      obj_record record = obj_pool->back();
      obj_pool->pop_back();
      add_obj_to_current_ext_at_key(extent_stack, record.first, record.second,
                                    key);
    }
  };
};
class SizeBasedGCObjectPackerBaseline : public SimpleGCObjectPacker {
public:
  using SimpleGCObjectPacker::SimpleGCObjectPacker;
  void pack_objects(shared_ptr<AbstractExtentStack> extent_stack, std::set<obj_ptr>& objs,
                    float key = 0) override {
    std::sort(obj_pool->begin(), obj_pool->end(), obj_record_asc_rem_size);
    while (!obj_pool->empty()) {
      obj_record record = obj_pool->back();
      obj_pool->pop_back();
      add_obj_to_current_ext_at_key(extent_stack, record.first, record.second,
                                    key);
    }
  };
};

class SizeBasedObjectPackerSmallerObj
    : public SizeBasedObjectPackerSmallerWholeObj {
public:
  using SizeBasedObjectPackerSmallerWholeObj::
      SizeBasedObjectPackerSmallerWholeObj;

  void pack_objects(shared_ptr<AbstractExtentStack> extent_stack, std::set<obj_ptr>& objs,
                    float key = 0) override {
    if (current_exts->find(key) != current_exts->end()) {
      object_lst objs = (*current_exts)[key]->delete_ext();
      add_objs(objs);
    }
    std::sort(obj_pool->begin(), obj_pool->end(), obj_record_desc_rem_size_extent);
    ext_stack * obj_ids_to_exts = new ext_stack();
    while (!obj_pool->empty()) {
      if (current_exts->find(key) == current_exts->end())
      { (*current_exts)[key] = ext_manager->create_extent(); }

      ext_ptr current_ext = (*current_exts)[key];
      double free_space = current_ext->free_space;
      int ind = get_smaller_obj_index(current_ext->ext_size, free_space);
      if(ind == -1)
      {
        ind = obj_pool->size() - 1;
      }
      // std::cout << "obj_pool size " << obj_pool->size() << std::endl;
      // std::cout << "ind SizeBasedObjectPackerSmallerWholeObj " << ind << std::endl;
      auto r = (*obj_pool)[ind];
      obj_pool->erase(obj_pool->begin() + ind);
      float obj_rem_size = r.second;
      // std::cout <<"smaller ind" << ind << "rem_size" << obj_rem_size <<std::endl;
      if (obj_rem_size < current_ext->ext_size &&
          obj_rem_size > (1 - threshold / 100.0) * current_ext->ext_size) {
        float obj_original_size = obj_rem_size;
        obj_rem_size = (1 - threshold / 100.0) * current_ext->ext_size;
        // std::cout <<"inserting back" << r.first->id << "rem_size" << obj_original_size - obj_rem_size <<std::endl;
        insert_obj_back_into_pool(r.first, obj_original_size - obj_rem_size);
      }
      add_obj_to_current_ext_at_key(
            extent_stack, r.first, obj_rem_size, key, *obj_ids_to_exts);
    }
    for (auto &kv : *obj_ids_to_exts) {
      for (ext_ptr ext : kv.second) {
        extent_stack->add_extent(key, ext);
      }
    }
    delete obj_ids_to_exts;
  };
};
class SizeBasedGCObjectPackerSmallerObj
    : public SizeBasedGCObjectPackerSmallerWholeObj {
public:
  using SizeBasedGCObjectPackerSmallerWholeObj::
      SizeBasedGCObjectPackerSmallerWholeObj;
  void pack_objects(shared_ptr<AbstractExtentStack> extent_stack, std::set<obj_ptr>& objs,
                    float key = 0) override {
    if (current_exts->find(key) != current_exts->end()) {
      object_lst objs = (*current_exts)[key]->delete_ext();
      add_objs(objs);
    }
    std::sort(obj_pool->begin(), obj_pool->end(), obj_record_desc_rem_size_extent);
    ext_stack * obj_ids_to_exts = new ext_stack();
    while (!obj_pool->empty()) {
      if (current_exts->find(key) == current_exts->end())
      { (*current_exts)[key] = ext_manager->create_extent(); }

      ext_ptr current_ext = (*current_exts)[key];
      double free_space = current_ext->free_space;
      int ind = get_smaller_obj_index(current_ext->ext_size, free_space);
      if(ind == -1)
      {
        ind = obj_pool->size() - 1;
      }
      // std::cout << "obj_pool size " << obj_pool->size() << std::endl;
      // std::cout << "ind SizeBasedObjectPackerSmallerWholeObj " << ind << std::endl;
      auto r = (*obj_pool)[ind];
      obj_pool->erase(obj_pool->begin() + ind);
      float obj_rem_size = r.second;
      // std::cout <<"smaller ind" << ind << "rem_size" << obj_rem_size <<std::endl;
      if (obj_rem_size < current_ext->ext_size &&
          obj_rem_size > (1 - threshold / 100.0) * current_ext->ext_size) {
        float obj_original_size = obj_rem_size;
        obj_rem_size = (1 - threshold / 100.0) * current_ext->ext_size;
        // std::cout <<"inserting back" << r.first->id << "rem_size" << obj_original_size - obj_rem_size <<std::endl;
        insert_obj_back_into_pool(r.first, obj_original_size - obj_rem_size);
      }
      add_obj_to_current_ext_at_key(
            extent_stack, r.first, obj_rem_size, key, *obj_ids_to_exts);
    }
    for (auto &kv : *obj_ids_to_exts) {
      for (ext_ptr ext : kv.second) {
        extent_stack->add_extent(0, ext);
      }
    }
    delete obj_ids_to_exts;
  };
};
class SizeBasedObjectPackerDynamicStrategy
    : public SizeBasedObjectPackerSmallerWholeObj {
public:
  using SizeBasedObjectPackerSmallerWholeObj::
      SizeBasedObjectPackerSmallerWholeObj;

  void pack_objects(shared_ptr<AbstractExtentStack> extent_stack, std::set<obj_ptr>& objs,
                    float key = 0) override {
    if (current_exts->find(key) != current_exts->end()) {
      object_lst objs = (*current_exts)[key]->delete_ext();
      add_objs(objs);
    }
    std::sort(obj_pool->begin(), obj_pool->end(), obj_record_desc_rem_size_extent);
    ext_stack obj_ids_to_exts = ext_stack();
    while (obj_pool->size() > 0) {
        if (current_exts->find(key) == current_exts->end())
          (*current_exts)[key] = ext_manager ->create_extent();

        ext_ptr current_ext = (*current_exts)[key];
        float free_space = current_ext->free_space;
        int ind = get_smaller_obj_index(current_ext->ext_size, free_space);
        if(ind == -1)
        {
          ind = obj_pool->size() - 1;
        }
        if (obj_pool->back().second <= current_ext->ext_size * (threshold / 100.0)) {
          std::shuffle(obj_pool->begin(), obj_pool->end(), generator);
          break;
        }
        auto r = (*obj_pool)[ind];
        obj_pool->erase(obj_pool->begin() + ind);
        float obj_rem_size = r.second;
        stack_val exts = add_obj_to_current_ext_at_key(extent_stack, r.first, obj_rem_size, key, obj_ids_to_exts);
    }
    while (obj_pool->size() > 0) {
      obj_record r = obj_pool->front();
      obj_pool->erase(obj_pool->begin());
      stack_val exts = add_obj_to_current_ext_at_key(
          extent_stack, r.first, r.second, key, obj_ids_to_exts);
    }
    for (auto &kv : obj_ids_to_exts) {
      extent_stack->add_extent(kv.second);
    }
  };
};
class SizeBasedGCObjectPackerDynamicStrategy
    : public SizeBasedObjectPackerDynamicStrategy {
public:
  using SizeBasedObjectPackerDynamicStrategy::
      SizeBasedObjectPackerDynamicStrategy;
  void generate_stripes(shared_ptr<AbstractExtentStack> extent_stack,
                                float simulation_time) override {
    return;
  }

  void
  gc_extent(ext_ptr ext, shared_ptr<AbstractExtentStack> extent_stack,
            std::set<obj_ptr>& objs) override {
    // std::cout << "gc_extent simple GC" << std::endl;
    for (auto obj_kv : ext->objects) {
      auto obj = obj_kv.first;
      float size = ext->get_obj_size(obj);
      this->add_obj(obj_record(obj, size));
    }
    this->pack_objects(extent_stack, objs);
  }

};

class SizeBasedObjectPackerSmallerWholeObjFillGap
    : public SizeBasedObjectPackerSmallerWholeObj {
public:
  using SizeBasedObjectPackerSmallerWholeObj::
      SizeBasedObjectPackerSmallerWholeObj;
  void pack_objects(shared_ptr<AbstractExtentStack> extent_stack, std::set<obj_ptr>& objs,
                    float key = 0) override {
    if (current_exts->find(key) != current_exts->end()) {
      object_lst objs = (*current_exts)[key]->delete_ext();
      add_objs(objs);
    }
    std::sort(obj_pool->begin(), obj_pool->end(), obj_record_desc_rem_size_extent);
    ext_stack obj_ids_to_exts;
    while (obj_pool->size() > 0) {
        if (current_exts->find(key) == current_exts->end())
          (*current_exts)[key] = ext_manager->create_extent(); 

        ext_ptr current_ext = (*current_exts)[key];
        float free_space = current_ext->free_space;
        int ind = get_smaller_obj_index(current_ext->ext_size, free_space);
        if(ind == -1)
        {
          ind = obj_pool->size() - 1;
        }
        auto r = (*obj_pool)[ind];
        obj_pool->erase(obj_pool->begin() + ind);
        float obj_rem_size = r.second;
        if (obj_rem_size < current_ext->ext_size &&
            obj_rem_size > (1 - threshold / 100.0) * current_ext->ext_size) {
          float obj_original_size = r.second;
          obj_rem_size = (0.99 - threshold / 100.0) * current_ext->ext_size;
          insert_obj_back_into_pool(r.first, obj_original_size - obj_rem_size);
          add_obj_to_current_ext_at_key(extent_stack, r.first, obj_rem_size,
                                        key, obj_ids_to_exts);
          if (obj_pool->size() > 0) {
            float free_space = current_ext->free_space;

            int ind = get_larger_obj_index(current_ext->ext_size, free_space);
            if(ind == -1)
            {
              ind = obj_pool->size() - 1;
            }
            auto obj = (*obj_pool)[ind];
            obj_pool->erase(obj_pool->begin() + ind);
          }
        }

        stack_val exts = add_obj_to_current_ext_at_key(
            extent_stack, r.first, obj_rem_size, key, obj_ids_to_exts);
    }
    for (auto &kv : obj_ids_to_exts) {
      extent_stack->add_extent(kv.second);
    }
  };
};
class SizeBasedGCObjectPackerSmallerWholeObjFillGap
    : public SizeBasedObjectPackerSmallerWholeObjFillGap {
public:
  using SizeBasedObjectPackerSmallerWholeObjFillGap::
      SizeBasedObjectPackerSmallerWholeObjFillGap;
  void generate_stripes(shared_ptr<AbstractExtentStack> extent_stack,
                                float simulation_time) override {
    return;
  }

  void
  gc_extent(ext_ptr ext, shared_ptr<AbstractExtentStack> extent_stack,
            std::set<obj_ptr>& objs) override {
    // std::cout << "gc_extent simple GC" << std::endl;
    for (auto obj_kv : ext->objects) {
      auto obj = obj_kv.first;
      float size = ext->get_obj_size(obj);
      this->add_obj(obj_record(obj, size));
    }
    this->pack_objects(extent_stack, objs);
  }
  void pack_objects(shared_ptr<AbstractExtentStack> extent_stack, std::set<obj_ptr>& objs,
                    float key = 0) override {
    if (current_exts->find(key) != current_exts->end()) {
      object_lst objs = (*current_exts)[key]->delete_ext();
      add_objs(objs);
    }
    std::sort(obj_pool->begin(), obj_pool->end(), obj_record_desc_rem_size_extent);
    ext_stack obj_ids_to_exts;
    while (obj_pool->size() > 0) {
        if (current_exts->find(key) == current_exts->end())
          (*current_exts)[key] = ext_manager->create_extent();

        ext_ptr current_ext = (*current_exts)[key];
        float free_space = current_ext->free_space;
        int ind = get_smaller_obj_index(current_ext->ext_size, free_space);
        if(ind == -1)
        {
          ind = obj_pool->size() - 1;
        }
        auto r = (*obj_pool)[ind];
        obj_pool->erase(obj_pool->begin() + ind);
        float obj_rem_size = r.second;
        if (obj_rem_size < current_ext->ext_size &&
            obj_rem_size > (1 - threshold / 100.0) * current_ext->ext_size) {
          float obj_original_size = r.second;
          obj_rem_size = (0.99 - threshold / 100.0) * current_ext->ext_size;
          insert_obj_back_into_pool(r.first, obj_original_size - obj_rem_size);
          add_obj_to_current_ext_at_key(extent_stack, r.first, obj_rem_size,
                                        key, obj_ids_to_exts);
          if (obj_pool->size() > 0) {
            float free_space = current_ext->free_space;

            int ind = get_larger_obj_index(current_ext->ext_size, free_space);
            if(ind == -1)
            {
              ind = obj_pool->size() - 1;
            }
            r = (*obj_pool)[ind];
            obj_pool->erase(obj_pool->begin() + ind);
          } else {
            break;
          }
        }

        stack_val exts = add_obj_to_current_ext_at_key(
            extent_stack, r.first, obj_rem_size, key, obj_ids_to_exts);
    }
    for (auto &kv : obj_ids_to_exts) {
      extent_stack->add_extent(kv.second);
    }
  };
};

class SizeBasedObjectPackerLargerWholeObj : public SizeBasedObjectPacker {
public:
  using SizeBasedObjectPacker::SizeBasedObjectPacker;
  std::pair<float, ext_ptr >
  add_obj_to_current_ext(shared_ptr<AbstractExtentStack> extent_stack,
                         obj_ptr obj, float obj_rem_size, float key) {
    auto current_ext = (*this->current_exts)[key];
    auto tmp = current_ext->add_object(obj, obj_rem_size);

    obj_rem_size = obj_rem_size - tmp;
    // seal the extent if the extent is full
    if (obj_rem_size > 0 || (obj_rem_size == 0 && current_ext->free_space == 0)) {
      (*this->current_exts)[key] = this->ext_manager->create_extent();
      this->update_extent_type(current_ext);

      current_ext->type = this->get_extent_type(current_ext);
      return std::make_pair(obj_rem_size, current_ext);
    }
    return std::make_pair(obj_rem_size, nullptr);
  }

  stack_val
  add_obj_to_current_ext_at_key(shared_ptr<AbstractExtentStack> extent_stack,
                                obj_ptr obj, float rem_size, float key,
                                ext_stack& obj_ids_to_exts) {
    stack_val exts = stack_val();
    if (this->current_exts->find(key) == this->current_exts->end())
      (*this->current_exts)[key] = this->ext_manager->create_extent();
    auto current_ext = (*this->current_exts)[key];
    float obj_rem_size = rem_size;
    while (obj_rem_size >= current_ext->ext_size) {
      auto rem_size_and_ext =
          this->add_obj_to_current_ext(extent_stack, obj, obj_rem_size, key);
      obj_rem_size = rem_size_and_ext.first;
      current_ext = rem_size_and_ext.second;
      if (current_ext != nullptr)
        exts.emplace_back(current_ext);
    }
    // if(current_ext == nullptr)
    // {
    //   std::cout << "current_ext nullptr" << std::endl;
    // }
    if (exts.size() > 0) {
      int obj_id = obj->id;
      if (obj_ids_to_exts.find(obj_id) != obj_ids_to_exts.end())
        obj_ids_to_exts[obj_id].insert(obj_ids_to_exts[obj_id].end(),
                                       exts.begin(), exts.end());
      else
        obj_ids_to_exts[obj_id] = exts;
    }

    auto rem_size_and_ext =
        this->add_obj_to_current_ext(extent_stack, obj, obj_rem_size, key);
    current_ext = rem_size_and_ext.second;
    obj_rem_size = rem_size_and_ext.first;
    if (current_ext != nullptr) {
      exts.emplace_back(current_ext);
      int obj_id = current_ext->objects.begin()->first->id;
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

  void pack_objects(shared_ptr<AbstractExtentStack> extent_stack, std::set<obj_ptr>& objs,
                    float key = 0) override {
    
    stack_val exts = stack_val();
    ext_stack obj_ids_to_exts = ext_stack();

    // If there are any object in the current extent, place them
    // back in the pool.
    if (this->current_exts->find(key) != this->current_exts->end()) {
      auto objs = (*this->current_exts)[key]->delete_ext();
      this->add_objs(objs);
    }
    std::sort(obj_pool->begin(), obj_pool->end(), obj_record_desc_rem_size_extent);

    while (obj_pool->size() > 0) {
      if (this->current_exts->find(key) == this->current_exts->end())
        (*this->current_exts)[key] = this->ext_manager->create_extent();

      auto current_ext = (*this->current_exts)[key];
      float free_space = current_ext->free_space;
      int ind = this->get_larger_obj_index(current_ext->ext_size, free_space);
      if(ind == -1)
      {
        ind = obj_pool->size() - 1;
      }
      auto r = (*obj_pool)[ind];
      obj_pool->erase(obj_pool->begin() + ind);
      float obj_rem_size = r.second;
      if (obj_rem_size < current_ext->ext_size &&
            obj_rem_size > (1 - threshold / 100.0) * current_ext->ext_size) {
        float obj_original_size = r.second;
        obj_rem_size = (1 - threshold / 100.0) * current_ext->ext_size;
        insert_obj_back_into_pool(r.first, obj_original_size - obj_rem_size);
      }
      stack_val exts = add_obj_to_current_ext_at_key(
          extent_stack, r.first, obj_rem_size, key, obj_ids_to_exts);
    }

    for (auto mapping : obj_ids_to_exts)
      extent_stack->add_extent(mapping.second);
  }
};

class SizeBasedGCObjectPackerLargerWholeObj
    : public SizeBasedObjectPackerLargerWholeObj {
public:
  /*same functions as non-gc one*/
  using SizeBasedObjectPackerLargerWholeObj::
      SizeBasedObjectPackerLargerWholeObj;
  void pack_objects(shared_ptr<AbstractExtentStack> extent_stack, std::set<obj_ptr>& objs,
                    float key = 0) override {
    
    stack_val exts = stack_val();
    ext_stack obj_ids_to_exts = ext_stack();

    // If there are any object in the current extent, place them
    // back in the pool.
    if (this->current_exts->find(key) != this->current_exts->end()) {
      auto objs = (*this->current_exts)[key]->delete_ext();
      this->add_objs(objs);
    }
    std::sort(obj_pool->begin(), obj_pool->end(), obj_record_desc_rem_size_extent);

    while (obj_pool->size() > 0) {
      if (this->current_exts->find(key) == this->current_exts->end())
        (*this->current_exts)[key] = this->ext_manager->create_extent();

      auto current_ext = (*this->current_exts)[key];
      float free_space = current_ext->free_space;
      int ind = this->get_larger_obj_index(current_ext->ext_size, free_space);
      if(ind == -1)
      {
        ind = obj_pool->size() - 1;
      }
      auto r = (*obj_pool)[ind];
      obj_pool->erase(obj_pool->begin() + ind);
      float obj_rem_size = r.second;
      if (obj_rem_size < current_ext->ext_size &&
            obj_rem_size > (1 - threshold / 100.0) * current_ext->ext_size) {
        float obj_original_size = r.second;
        obj_rem_size = (1 - threshold / 100.0) * current_ext->ext_size;
        insert_obj_back_into_pool(r.first, obj_original_size - obj_rem_size);
      }
      stack_val exts = add_obj_to_current_ext_at_key(
          extent_stack, r.first, obj_rem_size, key, obj_ids_to_exts);
    }

    for (auto mapping : obj_ids_to_exts)
      extent_stack->add_extent(mapping.second);
  }

  void
  gc_extent(ext_ptr ext, shared_ptr<AbstractExtentStack> extent_stack,
            std::set<obj_ptr>& objs) override {
    for (auto obj_kv : ext->objects) {
      auto obj = obj_kv.first;
      float size = ext->get_obj_size(obj);
      this->add_obj(obj_record(obj, size));
    }
    this->pack_objects(extent_stack, objs);
  }
  void generate_stripes(shared_ptr<AbstractExtentStack> extent_stack,
                                float simulation_time) override {
    return;
  }
};


class KeyBasedObjectPacker : public SimpleObjectPacker {
protected:
  float (ExtentObject::*obj_key_fnc)();
  float (Extent::*ext_key_fnc)();
  shared_ptr<obj_pq> obj_queue;

public:
  KeyBasedObjectPacker(
      shared_ptr<ObjectManager> obj_manager,
      shared_ptr<ExtentManager> ext_manager,
      shared_ptr<obj_pq> obj_q,
      shared_ptr<current_extents> current_exts = nullptr,
      short num_objs_in_pool = 100, short threshold = 10,
      bool record_ext_types = false, float (ExtentObject::*o_k_f)() = nullptr,
      float (Extent::*e_k_f)() = nullptr)
      : SimpleObjectPacker(obj_manager, ext_manager, nullptr, current_exts,
                           num_objs_in_pool, threshold, record_ext_types),
        obj_key_fnc(o_k_f), ext_key_fnc(e_k_f), obj_queue(obj_q){};
  
  void add_objs(object_lst obj_lst) override {
    for(auto o : obj_lst)
    {
      add_obj(o);
    }
  }
  void generate_exts_at_key(shared_ptr<AbstractExtentStack> extent_stack,
                            int num_exts, float key) override {
    int num_exts_at_key = extent_stack->get_length_at_key(key);
    // std::cout << extent_stack->get_length_of_extent_stack() << std::endl;
    while (num_exts_at_key < num_exts) {
      object_lst objs = this->obj_manager->create_new_object();
      for (auto r : objs) {
        obj_queue->push(obj_pq_record(std::invoke(obj_key_fnc, r.first), r.first,
                                     r.first->size));        
      }
      auto temp = std::set<obj_ptr>();
      pack_objects(extent_stack, temp, key);
      num_exts_at_key = extent_stack->get_length_at_key(key);
      // std::cout << "num_exts_at_key in looop" << num_exts_at_key << "key" << key <<std::endl;
    }
  }

  void add_obj(obj_record r) override {
    obj_queue->push(obj_pq_record(std::invoke(obj_key_fnc, r.first), r.first,
                                 r.first->size));
  }

  void generate_stripes(shared_ptr<AbstractExtentStack> extent_stack,
                        float simulation_time) override {
    if (obj_queue->size() < num_objs_in_pool) {
      object_lst objs =
          obj_manager->create_new_object(num_objs_in_pool - obj_queue->size());
      for (obj_record r : objs) {
        add_obj(r);
      }
      auto temp = std::set<obj_ptr>();
      pack_objects(extent_stack, temp);
    }
  }

  void
  add_obj_to_current_ext_at_key(shared_ptr<AbstractExtentStack> extent_stack,
                                obj_ptr obj, float obj_rem_size,
                                float key) override {
    float temp = 0;
    if (current_exts->find(key) == current_exts->end()) {
      (*current_exts)[key] = ext_manager->create_extent();
    }
    ext_ptr current_extent = (*current_exts)[key];
    float rem_size = obj_rem_size;
    while (rem_size > 0) {
      temp = current_extent->add_object(obj, obj_rem_size);
      rem_size = rem_size - temp;
      if (rem_size > 0 || (rem_size == 0 && current_extent->free_space == 0)) {
        extent_stack->add_extent(std::invoke(ext_key_fnc, current_extent),
                                 current_extent);
        update_extent_type(current_extent);
        current_extent->type = get_extent_type(current_extent);
        current_extent = ext_manager->create_extent();
        (*current_exts)[key] = current_extent;
      }
    }
  }
  void pack_objects(shared_ptr<AbstractExtentStack> extent_stack, std::set<obj_ptr>& objs,
                    float key = 0) override {
    while (!obj_queue->empty()) {
      obj_pq_record r = std::get<obj_pq_record>(obj_queue->top());
      obj_queue->pop();
      add_obj_to_current_ext_at_key(extent_stack, std::get<1>(r),
                                    std::get<2>(r), 0);
    }
  };
};

class KeyBasedGCObjectPacker : public SimpleGCObjectPacker {
protected:
  float (ExtentObject::*obj_key_fnc)();
  float (Extent::*ext_key_fnc)();
  shared_ptr<obj_pq> obj_queue;

public:
  KeyBasedGCObjectPacker(
      shared_ptr<ObjectManager> obj_manager,
      shared_ptr<ExtentManager> ext_manager,
      shared_ptr<obj_pq> obj_q,
      shared_ptr<current_extents> current_exts,
      short num_objs_in_pool = 100, short threshold = 10,
      bool record_ext_types = false, float (ExtentObject::*o_k_f)() = nullptr,
      float (Extent::*e_k_f)() = nullptr)
      : SimpleGCObjectPacker(obj_manager, ext_manager, nullptr, current_exts,
                             num_objs_in_pool, threshold, record_ext_types),
        obj_key_fnc(o_k_f), ext_key_fnc(e_k_f), obj_queue(obj_q){};
  void add_objs(object_lst obj_lst) override {
    for(auto o : obj_lst)
    {
      add_obj(o);
    }
  }
  void add_obj(obj_record r) override {
    obj_queue->push(obj_pq_record(std::invoke(obj_key_fnc, r.first), r.first,
                                 r.first->size));
  }

  void generate_exts_at_key(shared_ptr<AbstractExtentStack> extent_stack,
                            int num_exts, float key) override {
    int num_exts_at_key = extent_stack->get_length_at_key(key);
    while (num_exts_at_key < num_exts) {
      object_lst objs = this->obj_manager->create_new_object();
      for (auto r : objs) {
        obj_queue->push(obj_pq_record(std::invoke(obj_key_fnc, r.first), r.first,
                                     r.first->size));
      }
      auto temp = std::set<obj_ptr>();
      pack_objects(extent_stack, temp, key);
      num_exts_at_key = extent_stack->get_length_at_key(key);
    }
  }
  void pack_objects(shared_ptr<AbstractExtentStack> extent_stack,
                    std::set<obj_ptr>& objs, float key = 0) override {
    while (!obj_queue->empty()) {
      obj_pq_record r = std::get<obj_pq_record>(obj_queue->top());
      float key = 0;
      obj_queue->pop();
      add_obj_to_current_ext_at_key(extent_stack, std::get<1>(r),
                                    std::get<2>(r), key);
    }
  };
  void gc_extent(
      ext_ptr ext, shared_ptr<AbstractExtentStack> extent_stack,
      std::set<obj_ptr>& objs) override {
    for (auto &obj_kv : ext->objects)
    {
      auto obj = obj_kv.first;
      auto size = ext->get_obj_size(obj);
      // fprintf(stderr, "%d %d",  obj->id, size);
      this->add_obj(obj_record(obj, ext->get_obj_size(obj)));
    }
    this->pack_objects(extent_stack, objs);
  }

  void generate_stripes(shared_ptr<AbstractExtentStack> extent_stack,
                        float simulation_time) override {
    return;
  }

  void
  add_obj_to_current_ext_at_key(shared_ptr<AbstractExtentStack> extent_stack,
                                obj_ptr obj, float obj_rem_size,
                                float key) override {
    float temp = 0;
    if (current_exts->find(key) == current_exts->end()) {
      (*current_exts)[key] = ext_manager->create_extent();
    }
    ext_ptr current_extent = (*current_exts)[key];
    float rem_size = obj_rem_size;
    while (rem_size > 0) {
      temp = current_extent->add_object(obj, obj_rem_size);
      rem_size = rem_size - temp;
      if (rem_size > 0 || (rem_size == 0 && current_extent->free_space == 0)) {
        extent_stack->add_extent(std::invoke(ext_key_fnc, current_extent),
                                 current_extent);
        update_extent_type(current_extent);
        current_extent->type = get_extent_type(current_extent);
        current_extent = ext_manager->create_extent();
        (*current_exts)[key] = current_extent;
      }
    }
  }
};

class AgeBasedObjectPacker : public KeyBasedObjectPacker {
public:
  AgeBasedObjectPacker(
      shared_ptr<ObjectManager> obj_manager,
      shared_ptr<ExtentManager> ext_manager,
      shared_ptr<obj_pq> q,
      shared_ptr<current_extents> current_exts,
      short num_objs_in_pool = 100, short threshold = 10,
      bool record_ext_types = false)
      : KeyBasedObjectPacker(obj_manager, ext_manager, q, current_exts,
                             num_objs_in_pool, threshold, record_ext_types,
                             &ExtentObject::get_timestamp,
                             &Extent::get_timestamp) {}
  void pack_objects(shared_ptr<AbstractExtentStack> extent_stack,
                    std::set<obj_ptr>& objs, float key = 0) override {
    while (!obj_queue->empty()) {
      obj_pq_record r = std::get<obj_pq_record>(obj_queue->top());
      obj_queue->pop();
      add_obj_to_current_ext_at_key(extent_stack, std::get<1>(r),
                                    std::get<2>(r), 0);
    }
  };
};

class AgeBasedGCObjectPacker : public KeyBasedGCObjectPacker {
public:
  AgeBasedGCObjectPacker(
      shared_ptr<ObjectManager> obj_manager,
      shared_ptr<ExtentManager> ext_manager,
      shared_ptr<obj_pq> q,
      shared_ptr<current_extents> current_exts,
      short num_objs_in_pool = 100, short threshold = 10,
      bool record_ext_types = false)
      : KeyBasedGCObjectPacker(obj_manager, ext_manager, q, current_exts,
                               num_objs_in_pool, threshold, record_ext_types,
                               &ExtentObject::get_timestamp,
                               &Extent::get_timestamp) {}
  void pack_objects(shared_ptr<AbstractExtentStack> extent_stack, std::set<obj_ptr>& objs,
                    float key = 0) override {
    while (!obj_queue->empty()) {
      obj_pq_record r = std::get<obj_pq_record>(obj_queue->top());
      obj_queue->pop();
      add_obj_to_current_ext_at_key(extent_stack, std::get<1>(r),
                                    std::get<2>(r), 0);
    }
  };
};

class AgeBasedRandomizedObjectPacker : public AgeBasedObjectPacker {
public:
  using AgeBasedObjectPacker::AgeBasedObjectPacker;
  void pack_objects(shared_ptr<AbstractExtentStack> extent_stack, std::set<obj_ptr>& objs,
                    float key = 0) override {
    float prev_key = 0;
    while (obj_queue->size() > 0) {
      prev_key = std::get<0>(std::get<obj_pq_record>(obj_queue->top()));
      obj_pq_record r = std::get<obj_pq_record>(obj_queue->top());
      vector<obj_ptr> chunks;
      while (std::get<0>(r) == prev_key && obj_queue->size() > 0) {
        r = std::get<obj_pq_record>(obj_queue->top());
        obj_queue->pop();
        for (int j = 0; j < std::get<2>(r) / 4; ++j) {
          chunks.push_back(std::get<1>(r));
        }
        if (obj_queue->size() == 0) {
          break;
        }

        r = std::get<obj_pq_record>(obj_queue->top());
      }

      shuffle(chunks.begin(), chunks.end(), generator);
      for (auto obj : chunks) {
        add_obj_to_current_ext_at_key(extent_stack, obj, 4, key);
      }
    }
  }
};

class AgeBasedRandomizedGCObjectPacker : public AgeBasedGCObjectPacker {
public:
  using AgeBasedGCObjectPacker::AgeBasedGCObjectPacker;
  void pack_objects(shared_ptr<AbstractExtentStack> extent_stack, std::set<obj_ptr>& objs,
                    float key = 0) override {
    float prev_key = 0;
    while (obj_queue->size() > 0) {
      prev_key = std::get<0>(std::get<obj_pq_record>(obj_queue->top()));
      obj_pq_record r = std::get<obj_pq_record>(obj_queue->top());
      vector<obj_ptr> chunks;
      while (std::get<0>(r) == prev_key && obj_queue->size() > 0) {
        r = std::get<obj_pq_record>(obj_queue->top());
        obj_queue->pop();
        for (int j = 0; j < std::get<2>(r) / 4; ++j) {
          chunks.push_back(std::get<1>(r));
        }
        if (obj_queue->size() == 0) {
          break;
        }

        r = std::get<obj_pq_record>(obj_queue->top());
      }

      shuffle(chunks.begin(), chunks.end(), generator);
      for (auto obj : chunks) {
        add_obj_to_current_ext_at_key(extent_stack, obj, 4, 0);
      }
    }
  }
};
class GenerationBasedObjectPacker : public KeyBasedObjectPacker {
public:
  GenerationBasedObjectPacker(
      shared_ptr<ObjectManager> obj_manager,
      shared_ptr<ExtentManager> ext_manager,
      shared_ptr<obj_pq> q,
      shared_ptr<current_extents> current_exts,
      short num_objs_in_pool = 100, short threshold = 10,
      bool record_ext_types = false)
      : KeyBasedObjectPacker(obj_manager, ext_manager, q, current_exts,
                             num_objs_in_pool, threshold, record_ext_types,
                             &ExtentObject::get_generation,
                             &Extent::get_generation) {}

  void generate_exts_at_key(shared_ptr<AbstractExtentStack> extent_stack,
                            int num_exts, float key) override {
    int num_exts_at_key = extent_stack->get_length_at_key(key);
    // std::cout << extent_stack->get_length_of_extent_stack() << std::endl;
    while (num_exts_at_key < num_exts) {
      object_lst objs = this->obj_manager->create_new_object();
      for (auto r : objs) {
        obj_ptr obj = r.first;
        obj_queue->push(obj_record(obj, obj->size));
      }
      auto temp = std::set<obj_ptr>();
      pack_objects(extent_stack, temp);
      num_exts_at_key = extent_stack->get_length_at_key(key);
    }
  }
  void generate_stripes(shared_ptr<AbstractExtentStack> extent_stack,
                        float simulation_time) override {
    if (obj_queue->size() < num_objs_in_pool) {
      auto objs =
          obj_manager->create_new_object(num_objs_in_pool - obj_queue->size());
      for (auto p : objs) {
        add_obj(obj_record(p.first, p.second));
      }
      auto temp = std::set<obj_ptr>();
      pack_objects(extent_stack, temp);
    }
  }
  void pack_objects(shared_ptr<AbstractExtentStack> extent_stack, std::set<obj_ptr>& objs, float k = 0) override {
    // std::cout << "pack_objects before" << obj_queue->size() << std::endl;
    while (obj_queue->size() > 0) {
      obj_record r = std::get<obj_record>(obj_queue->top());
      obj_queue->pop();
      float key = r.first->generation;
      add_obj_to_current_ext_at_key(extent_stack, r.first, r.second, key);
    }
    // std::cout << "pack_objects after" << obj_queue->size() << std::endl;
  }
  void add_obj(obj_record record) override { obj_queue->push(record); }
};

class GenerationBasedGCObjectPacker : public KeyBasedGCObjectPacker {
public:
  GenerationBasedGCObjectPacker(
      shared_ptr<ObjectManager> obj_manager,
      shared_ptr<ExtentManager> ext_manager,
      shared_ptr<obj_pq> q,
      shared_ptr<current_extents> current_exts,
      short num_objs_in_pool = 100, short threshold = 10,
      bool record_ext_types = false)
      : KeyBasedGCObjectPacker(obj_manager, ext_manager, q, current_exts,
                               num_objs_in_pool, threshold, record_ext_types,
                               &ExtentObject::get_generation,
                               &Extent::get_generation) {}
  void add_obj(obj_record record) override { obj_queue->push(record); }

  void generate_exts_at_key(shared_ptr<AbstractExtentStack> extent_stack,
                            int num_exts, float key) override {
    int num_exts_at_key = extent_stack->get_length_at_key(key);
    while (num_exts_at_key < num_exts) {
      object_lst objs = this->obj_manager->create_new_object();
      for (auto r : objs) {
        obj_ptr obj = r.first;
        obj_queue->push(obj_record(obj, obj->size));
      }
      std::set<obj_ptr> aaa;
      pack_objects(extent_stack, aaa);
      num_exts_at_key = extent_stack->get_length_at_key(key);
    }
  }
  void pack_objects(shared_ptr<AbstractExtentStack> extent_stack,
                    std::set<obj_ptr> &objs, float k = 0) override {
    while (obj_queue->size() > 0) {
      obj_record r = std::get<obj_record>(obj_queue->top());
      obj_queue->pop();
      if (objs.size() != 0 && objs.find(r.first) != objs.end()) {
        objs.insert(r.first);
        r.first->generation++;
      }
      float key = r.first->generation;

      add_obj_to_current_ext_at_key(extent_stack, r.first, r.second, key);
    }
  }
};

#endif // __OBJECT_PACKER_H_
