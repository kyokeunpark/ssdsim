#pragma once
#include "config.h"
#include "extent_object_shard.h"
#include <memory>
#include <ctime>
#include <iostream>
#include <list>
#include <numeric>
#include <unordered_map>
#include <vector>
class Extent;
class ExtentObject;
class Stripe;

using std::list;
using std::string;
using std::unordered_map;
using std::vector;
using std::make_shared;
using shard_ptr = std::shared_ptr<Extent_Object_Shard>;
using obj_ptr = std::shared_ptr<ExtentObject>;
using ext_ptr = std::shared_ptr<Extent>;
using stripe_ptr = std::shared_ptr<Stripe>;
using obj_record = std::pair<obj_ptr, float>;
using object_lst = std::vector<obj_record>;

class ExtentObject: public std::enable_shared_from_this<ExtentObject> {
protected:
public:
  int id;
  float size;
  double life;
  int generation;
  time_t creation_time;
  int num_times_gced;
  list<float> shards;
  list<ext_ptr> extents;

  ExtentObject(int id, float s, float l)
      : id(id), size(s), life(l), generation(0), num_times_gced(0),
        creation_time(time(nullptr)), shards(list<float>()),
        extents(list<ext_ptr>()) {}

  float get_timestamp() { return creation_time; }

  bool operator<(const ExtentObject &other) { return this->id < other.id; }

  float get_size() {
    float sum = 0;
    for (auto shard : this->shards)
      sum += shard;
    return sum;
  }

  double get_age() { return difftime(time(nullptr), creation_time); }

  void add_extent(ext_ptr e) { this->extents.emplace_back(e); }

  void remove_extent(ext_ptr e) {
    for (auto it = this->extents.begin(); it != this->extents.end(); it++) {
      if (*it == e) {
        this->extents.erase(it);
        return;
      }
    }
  }

  int get_default_key() { return 0; }
  float get_generation() { return generation; }
};

class Extent: public std::enable_shared_from_this<Extent> {
public:
  double obsolete_space;
  double free_space;
  int id;
  double ext_size;

  //unordered_map<obj_ptr, list<shard_ptr>> objects;
  unordered_map<obj_ptr, list<float>> objects;
  stripe_ptr stripe;
  int locality;
  int generation;
  float timestamp;
  string type;
  int secondary_threshold;
  float get_default_key() { return 0.0; }
  bool operator <(const Extent& d) {
        return this->id < d.id;
  }
  bool operator >(const Extent& d) {
        return this->id > d.id;
  }
  float get_timestamp() { return timestamp; }

  float get_generation() { return generation; }

  Extent(double e_s, int s_t, int i)
      : obsolete_space(0), free_space(e_s), ext_size(e_s), id(i),
        objects(unordered_map<obj_ptr, list<float>>()),
        locality(0), generation(0), timestamp(configtime), type("0"),
        secondary_threshold(s_t), stripe(nullptr) {}

  ~Extent() {
    this->delete_ext();
  }

  double get_age() { return difftime(time(nullptr), timestamp); }

  float get_obj_size(obj_ptr obj) {
    auto sizes = this->objects[obj];
    return std::accumulate(sizes.begin(), sizes.end(), 0);
  }

  double get_obsolete_percentage() { return obsolete_space / ext_size * 100; }

  float add_object(obj_ptr obj, float size, int generation = 0) {
    float temp_size = size < free_space ? size : free_space;
    int obj_id = obj->id;
    if (timestamp == 0)
      timestamp = obj->creation_time;
    else if (obj->creation_time < timestamp)
      timestamp = obj->creation_time;

    if (this->generation == 0)
      this->generation = obj->generation;
    else if (generation > this->generation)
      this->generation = obj->generation;

    if (this->objects.find(obj) != this->objects.end()) {
      obj->shards.push_back(temp_size);
    } else {
      obj->shards.push_back(temp_size);
      obj->add_extent(shared_from_this());
      auto shard_lst = list<float>();
      shard_lst.push_back(temp_size);
      this->objects.emplace(std::make_pair(obj, shard_lst));
    }
    free_space -= temp_size;
    return temp_size;
  }

  void remove_objects() {
    for (auto obj : this->objects)
      obj.first->remove_extent(shared_from_this());
    this->objects.clear();
  }

  double del_object(obj_ptr obj) { 
    auto it = this->objects.find(obj);
    if (it != this->objects.end()) {
      this->obsolete_space += std::accumulate(it->second.begin(), it->second.end(), 0);
      this->objects.erase(obj);
    }
    return obsolete_space / ext_size * 100;
  }

  object_lst delete_ext() {
    object_lst ret;

    for (auto & it : objects) {
      float sum = 0;
      for (auto s : it.second) {
        sum += s;
      }
      it.first->extents.remove(shared_from_this());
      ret.push_back(std::make_pair(it.first, sum));
    }

    for (auto & it : objects) {
      float sum = 0;
      for (auto shard : it.second)
        sum += shard;
    }

    generation = 0;
    free_space = ext_size;
    return ret;
  }
};

class Stripe: public std::enable_shared_from_this<Stripe> {
public:
  int id;
  double obsolete;
  int num_data_blocks;
  int num_localities;
  double free_space;
  vector<int> *localities;
  int ext_size;
  double timestamp;
  int stripe_size;
  int primary_threshold;
  list<ext_ptr> extents;

  Stripe(int id, int num_data_extents_per_locality, int num_localities,
         int ext_size, int primary_threshold)
      : id(id), obsolete(0), num_data_blocks(num_data_extents_per_locality),
        num_localities(num_localities),
        free_space(num_localities * num_data_extents_per_locality),
        localities(new vector<int>(num_localities, 0)), ext_size(ext_size),
        timestamp(0), primary_threshold(primary_threshold),
        extents(list<ext_ptr>()) {
    this->stripe_size = 0;
    for (int i = 0; i < num_data_blocks * num_localities; ++i) {
      this->stripe_size += ext_size;
    }
  }

  //????the python code doesnt seem right, need to ask///
  /*    def update_obsolete(self, obsolete):
  """
  Updates the amount of obsolete data in this stripe.

  Returns the percent of obsolete data.
  :param obsolete: float
  :rtype: float
  """
  self.obsolete += obsolete*/
  double update_obsolete(double obsolete) {
    this->obsolete += obsolete;
    return this->obsolete;
  }

  double get_obsolete_percentage() { return obsolete / stripe_size * 100; }

  int get_num_data_exts() { return num_data_blocks * num_localities; }

  void add_extent(ext_ptr ext) {
    if (free_space > 0) {
      extents.push_back(ext);
      ext->stripe = shared_from_this();
      free_space -= 1;
      int locality = 0;
      while ((*localities)[locality] == num_data_blocks) {
        locality += 1;
      }
      (*localities)[locality] += 1;
      ext->locality = locality;
      if (ext->timestamp > timestamp) {
        this->timestamp = ext->timestamp;
      }
    } else {
      std::cerr << "Attempt to add extent to a full stripe";
    }
  }

  void del_extent(ext_ptr ext) {
    ext->stripe = nullptr;
    ext->remove_objects();
    (*localities)[ext->locality] -= 1;
    obsolete -= ext->obsolete_space;
    extents.remove(ext);
    free_space += 1;
  }
};
inline bool operator<(const ExtentObject &a, const ExtentObject &b) {
  return a.id < b.id;
}
