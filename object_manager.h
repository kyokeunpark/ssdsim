#pragma once
#include "config.h"
#include "event_manager.h"
#include "extent_object_stripe.h"
#include "samplers.h"
#include <memory>
#include <random>
#include <unordered_map>

using std::shared_ptr;
using std::unordered_map;
using obj_record = std::pair<ExtentObject *, int>;
using object_lst = std::vector<obj_record>;

class ObjectManager {
public:
  int max_id;
  shared_ptr<EventManager> event_manager;
  shared_ptr<Sampler> sampler;
  unordered_map<int, ExtentObject *> objects;
  bool add_noise;

  ObjectManager() {}
  ObjectManager(shared_ptr<EventManager> e_m, shared_ptr<Sampler> s,
                bool a_n = true)
      : objects(unordered_map<int, ExtentObject *>()),
        event_manager(e_m), sampler(s),
        add_noise(a_n) {
    max_id = 1;
    srand(0);
  }

  // docstring and code doesnt match managers.py
  object_lst create_new_object(int num_samples = 1) {
    object_lst new_objs = object_lst();
    auto size_age_samples = sampler->get_size_age_sample(num_samples);
    sizes size_samples = size_age_samples.first;
    lives life_samples = size_age_samples.second;
    for (int i = 0; i < size_samples.size(); i++) {
      int size = size_samples[i];
      int life = life_samples[i];
      int noise = rand() % 25;
      if (add_noise) {
        noise -= 12;
        life += noise / 24;
      }
      life += configtime;
      ExtentObject *obj = new ExtentObject(max_id, size, life);
      new_objs.emplace_back(std::make_pair(obj, size));
      this->objects[max_id] = obj;
      max_id++;
      event_manager->put_event(life, obj);
    }
    return new_objs;
  }

  ExtentObject *get_object(int obj_id) {
    if (this->objects.find(obj_id) != this->objects.end())
      return this->objects[obj_id];
    return nullptr;
  }

  int get_num_objs() { return objects.size(); }

  void remove_object(ExtentObject *obj) {
    objects.erase(obj->id);
  }

  ~ObjectManager() {}
};
