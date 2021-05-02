#pragma once
#include "extent_object_stripe.h"
#include "lock.h"
#include <unordered_map>
#include <set>
#include <mutex>

using std::shared_ptr;
using std::mutex;

class ExtentManager {

private:

public:
  int ext_size;
  std::set<ext_ptr > exts;
  int max_id;
  float (Extent::*key_fnc)();
  shared_ptr<mutex> mtx = nullptr;

  ExtentManager(int s, float (Extent::*k_f)(), bool is_threaded = false)
      : ext_size(s), key_fnc(k_f), exts(std::set<ext_ptr >()) {
    max_id = 0;
    if (is_threaded)
      mtx = make_shared<mutex>();
  }

  float get_key(ext_ptr ext) { return ((*ext).*key_fnc)(); }

  int get_num_ext() { return exts.size(); }

  ext_ptr create_extent(int s = 0, int secondary_threshold = 15) {
    ext_ptr e;
    if (!s)
      e = make_shared<Extent>(ext_size, secondary_threshold, max_id);
    else
      e = make_shared<Extent>(s, secondary_threshold, max_id);
    lock(mtx);
    max_id++;
    exts.insert(e);
    unlock(mtx);
    return e;
  }

  unordered_map<string, int> get_ext_types() {
    unordered_map<string, int> ret;
    lock(mtx);
    for (ext_ptr e : exts) {
      if (e->type.compare("0") != 0) {
        ret.emplace(e->type, 0);
        ret[e->type] += 1;
      }
    }
    unlock(mtx);
    return ret;
  }

  void delete_extent(ext_ptr extent) {
    lock(mtx);
    exts.erase(extent);
    unlock(mtx);
  }
};
