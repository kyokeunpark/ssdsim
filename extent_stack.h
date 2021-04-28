#pragma once
#include "stripe_manager.h"
#include <algorithm>
#include <functional>
#include <iostream>
#include <map>
#include <memory>

using std::max;
using std::min;
using std::shared_ptr;
typedef vector<ext_ptr > *extent_stack_ext_lst;
/*
 * Struct used to check if a pointer to an extent exists within a
 * list of extents. It is kind of a hack, but works well, given the current
 * structure of the ExtentStack class.
 */
struct isExtent {
  ext_ptr extent;

  isExtent(ext_ptr e) : extent(e) {}

  bool operator()(const ext_ptr e) const { return this->extent == e; }
};

using stack_val = std::vector<ext_ptr >;
using ext_stack = std::map<float, stack_val>;
using ext_stack_desc = std::map<float, stack_val, std::greater<int>>;

// Used for WholeObjectExtentStack, which stores extent lists together
using stack_lst = std::vector<stack_val>;
using ext_lst_stack = std::map<float, stack_lst>;
using ext_lst_stack_desc = std::map<float, stack_lst, std::greater<float>>;

class AbstractExtentStack {
protected:
  std::shared_ptr<StripeManager> stripe_manager;

public:
  AbstractExtentStack() : stripe_manager(nullptr) {}
  AbstractExtentStack(shared_ptr<StripeManager> s_m) : stripe_manager(s_m) {}
  virtual ~AbstractExtentStack() {}

  virtual int num_stripes(int stripe_size) = 0;
  virtual list<ext_ptr > pop_stripe_num_exts(int stripe_size) = 0;
  virtual void add_extent(float key, ext_ptr ext){};
  virtual int get_length_of_extent_stack() = 0;
  virtual int get_length_at_key(float key) = 0;
  virtual ext_ptr get_extent_at_key(float key) = 0;
  virtual bool contains_extent(ext_ptr extent) = 0;
  virtual void remove_extent(ext_ptr extent) = 0;
  virtual ext_ptr get_extent_at_closest_key(float key) { std::cerr<<"should never be called virtual get_ext_at_closet_key"; return nullptr; };
  virtual void add_extent(stack_val &ext_lst) {
    std::cerr << "extent stack virtual add extent!";
  }
};
template<typename ext_stack_T = ext_stack_desc>
class ExtentStack : public AbstractExtentStack {

protected:
  // ordered by key
  ext_stack_T extent_stack;

public:
  ExtentStack(shared_ptr<StripeManager> s_m)
      : AbstractExtentStack(s_m), extent_stack(ext_stack_T()) {}

  virtual int num_stripes(int stripe_size) override = 0;

  virtual list<ext_ptr > pop_stripe_num_exts(int stripe_size) override = 0;

  void add_extent(float key, ext_ptr ext) override {
    if (this->extent_stack.find(key) == this->extent_stack.end())
      this->extent_stack.emplace(key, stack_val());
    this->extent_stack[key].push_back(ext);
  }

  virtual int get_length_of_extent_stack() override {
    int length = 0;
    for (auto &kv : this->extent_stack) {
      length += kv.second.size();
    }
    return length;
  }

  virtual int get_length_at_key(float key) override {
    auto it = extent_stack.find(key);
    return it == extent_stack.end() ? 0 : it->second.size();
  }

  /* pop at index 0 not sure if this is still a stack..
  def get_extent_at_key(self, key):
  """Returns an extent at the given key.
  """
  ext = self.extent_stack[key].pop(0)
  if (len(self.extent_stack.get(key)) == 0):
      del self.extent_stack[key]
  return ext
  */
  virtual ext_ptr get_extent_at_key(float key) override {
    if (extent_stack.find(key) == extent_stack.end())
      return nullptr;
    stack_val * exts = &this->extent_stack[key];
    ext_ptr ret = exts->front();
    exts->erase(exts->begin());
    if (exts->size() == 0)
      extent_stack.erase(key);
    
    return ret;
  }

  virtual bool contains_extent(ext_ptr extent) override {
    for (auto &kv : extent_stack) {
      if (find(kv.second.begin(), kv.second.end(), extent) != kv.second.end())
        return true;
    }
    return false;
  }

  // can end early
  virtual void remove_extent(ext_ptr extent) override {
    auto it =  extent_stack.begin();
    while (it != extent_stack.end()) {
      auto found = std::find(it->second.begin(), it->second.end(), extent);
      if(found != it->second.end())
      {
        it->second.erase(found);
      }
      
      if (it->second.size() == 0)
      {
        it = extent_stack.erase(it);
      }else {
        it++;
      }
    }
    
  }
};
template<typename T = ext_stack_desc> 
class SingleExtentStack : public ExtentStack<T>{
  using ExtentStack<T>::ExtentStack;

public:
  T * get_extent_stack() { return &this->extent_stack; }
  int num_stripes(int stripe_size) override {
    return this->get_length_of_extent_stack() / stripe_size;
  }

  list<ext_ptr > pop_stripe_num_exts(int stripe_size) override {
    list<ext_ptr > ret;
    int num_left_to_add = stripe_size;
    // std::cout << "num_left_to_add pop_stripe_num_exts singleES" << stripe_size << std::endl;
    // std::cout << "get_length_of_extent_stack pop_stripe_num_exts singleES" << this->get_length_of_extent_stack() << std::endl;
    if (this->get_length_of_extent_stack() < num_left_to_add) {
      return ret;
    }
    /*
     * NOTE: This produces slightly different result compared to original code.
     *       This is because the original code sorts the extents by key
     *       (in reverse order). Since we are not using standard integer
     *       keys to identify each extents, this is not possible.
     *
     *       In practice, this seems to alter some of the read and write
     *       results slightly, but not by much.
     */
    auto it = this->extent_stack.begin();
    while (it != this->extent_stack.end()) {
      int n =  it->second.size() > num_left_to_add ? num_left_to_add : it->second.size();
      for (int i = 0; i < n; i++) {
        ret.push_back(it->second.front());
        it->second.erase(it->second.begin());
      }
      if (it->second.size() == 0) {
        it = this->extent_stack.erase(it);
      }else {
        it++;
      }
      num_left_to_add = stripe_size - ret.size();
    }
    return ret;
  }
};

class MultiExtentStack : public ExtentStack<ext_stack> {
  public:
  using ExtentStack::ExtentStack;

  int num_stripes(int stripe_size) override {
    int num_stripes = 0;
    for (auto &kv : extent_stack) {
      num_stripes += kv.second.size() / stripe_size;
    }
    return num_stripes;
  }

  list<ext_ptr > pop_stripe_num_exts (int stripe_size) override{
    list<ext_ptr >ret;
    auto it = extent_stack.begin();
    while (it!= extent_stack.end()) {
      vector<ext_ptr > *ext_lst = &it->second;
      if (ext_lst->size() >= stripe_size) {
        for (int i = 0; i < stripe_size; i++) {
          ret.push_back(ext_lst->front());
          ext_lst->erase(ext_lst->begin());
        }
        if (ext_lst->size() == 0) {
          extent_stack.erase(it);
        }
        return ret;
      }
      it++;
    }
    return ret;
  }
};

class BestEffortExtentStack : public SingleExtentStack<ext_stack> {
public:
  using SingleExtentStack<ext_stack> ::SingleExtentStack;
  // double check correctness
  ext_ptr get_extent_at_closest_key(float key) override {
    if (extent_stack.size() == 1) {
      return get_extent_at_key(extent_stack.begin()->first);
    }
    if (key < extent_stack.begin()->first) {
      return get_extent_at_key(extent_stack.begin()->first);
    }
    if (key > prev(extent_stack.end())->first) {
      return get_extent_at_key(prev(extent_stack.end())->first);
    }
    auto next_pos = extent_stack.lower_bound(key);
    if (next_pos == extent_stack.end()) {
      next_pos = prev(extent_stack.end());
    }
    auto prev_pos = next_pos == extent_stack.begin() ? extent_stack.begin()
                                                     : prev(next_pos);
    float next_key = next_pos->first;
    float prev_key = prev_pos->first;
    if (next_key - key < key - prev_key) {
      return get_extent_at_key(next_key);
    }
    return get_extent_at_key(prev_key);
  }
};
class ExtentStackRandomizer : public AbstractExtentStack {
public:
  std::shared_ptr<SingleExtentStack<>> extent_stack;
  ExtentStackRandomizer(std::shared_ptr<SingleExtentStack<>> e_s)
      : extent_stack(e_s) {}
  int num_stripes(int stripe_size) override {
    return extent_stack->num_stripes(stripe_size);
  }
  void add_extent(float e, ext_ptr s) override { extent_stack->add_extent(e, s); }
  int get_length_at_key(float k) override {
    return extent_stack->get_length_at_key(k);
  }
  int get_length_of_extent_stack() override {
    return extent_stack->get_length_of_extent_stack();
  }
  bool contains_extent(ext_ptr extent) override {
    return extent_stack->contains_extent(extent);
  }
  void remove_extent(ext_ptr extent) override {
    extent_stack->remove_extent(extent);
  }
  list<ext_ptr > pop_stripe_num_exts(int stripe_size) override {
    auto it = extent_stack->get_extent_stack()->begin();
    while (it != extent_stack->get_extent_stack()->end() ) {

      std::shuffle(it->second.begin(), it->second.end(), generator);
      it++;
    }
    return extent_stack->pop_stripe_num_exts(stripe_size);
  }
  ext_ptr get_extent_at_closest_key(float key) override {
    auto it = extent_stack->get_extent_stack()->begin();
    while (it != extent_stack->get_extent_stack()->end() ) {
      std::shuffle(it->second.begin(), it->second.end(), generator);
      it++;
    }
    return extent_stack->get_extent_at_closest_key(key);
  }
  ext_ptr get_extent_at_key(float key) override {
    auto it = extent_stack->get_extent_stack()->begin();
    while (it != extent_stack->get_extent_stack()->end() ) {
      std::shuffle(it->second.begin(), it->second.end(), generator);
      it++;
    }
    return extent_stack->get_extent_at_key(key);
  }
};

class WholeObjectExtentStack : public AbstractExtentStack {
  using AbstractExtentStack::AbstractExtentStack;

  ext_lst_stack_desc extent_stack;

public:
  WholeObjectExtentStack(shared_ptr<StripeManager> stripe_manager)
      : AbstractExtentStack(stripe_manager) {
    this->extent_stack = ext_lst_stack_desc();
  }

  /*
   * Returns the number of stripes in extent stack
   */
  int num_stripes(int stripe_size) override {
    return get_length_of_extent_stack() / stripe_size;
  }

  int adjust_index(int ind, int length) {
    ind = min(ind, length - 1);
    ind = max(0, ind);
    return ind;
  }

  void add_extent(stack_val &ext_lst) override {
    float key = ext_lst.size();
    //std::cout << "key" << key <<std::endl;
    if (this->extent_stack.find(key) == this->extent_stack.end())
      extent_stack.emplace(key, stack_lst());
    extent_stack[key].push_back(ext_lst);
  }

  int get_length_of_extent_stack() override {
    int length = 0;
    for (auto &kv : extent_stack) {
      for (auto &l : kv.second)
        length += l.size();
    }
    return length;
  }

  /*error prone double check*/
  stack_val fill_gap(int num_left_to_add) {
    stack_val ret;
    int temp = num_left_to_add;
    while (temp > 0) {
      auto it = extent_stack.lower_bound(temp);
      
      for(auto e: *(it->second.begin()))
      {
        ret.push_back(e);
      }
      temp -= it->second.begin()->size();
      it->second.erase(it->second.begin());
      if (it->second.empty())
        extent_stack.erase(it);
    }
    return ret;
  }

  /*error prone double check*/
  list<ext_ptr > pop_stripe_num_exts(int stripe_size) override {
    list<ext_ptr > ret;
    int num_left_to_add = stripe_size;

    if (get_length_of_extent_stack() < num_left_to_add)
      return ret;

    int largest_key = extent_stack.begin()->first;
    auto largest_kv = extent_stack.begin();
    stack_val longest_lst = *(largest_kv->second.begin());
    largest_kv->second.erase(largest_kv->second.begin());
    int loop_times = longest_lst.size() > num_left_to_add?num_left_to_add:longest_lst.size();
    for (int i = 0;
         i < loop_times;
         i++) {
      ret.push_back(longest_lst.front());
      longest_lst.erase(longest_lst.begin());
    }

    num_left_to_add = stripe_size - ret.size();
    if (largest_kv->second.size() == 0)
      extent_stack.erase(largest_kv);
    //std::cout << "longest_lst_size" << longest_lst.size()<<std::endl;
    if (longest_lst.size() > 0)
      add_extent(longest_lst);
    if (num_left_to_add > 0)
    { 
      for (ext_ptr e : fill_gap(num_left_to_add))
        ret.push_back(e);
    }
    return ret;
  }

  /* not sure if this is correct??????
  def get_length_at_key(self, key):
      """Returns the number of extents in extent stack at key
      """
      return self.get_length_of_extent_stack()
  */
  int get_length_at_key(float key) override {
    return get_length_of_extent_stack();
  }

  /*what's the point of passing key in....
  def get_extent_at_key(self, key):
       """Returns an extent at the given key.
       """
       key = min(self.extent_stack.keys())
       ext = self.extent_stack[key].pop(0)[0]
       if (len(self.extent_stack.get(key)) == 0):
           del self.extent_stack[key]
       return ext
  */
  ext_ptr get_extent_at_key(float k) override {
      auto smallest = std::prev(extent_stack.end());
      auto ext = smallest->second.front().front();
      smallest->second.erase(smallest->second.begin());
      if (smallest->second.size() == 0)
        extent_stack.erase(smallest);
      return ext;
  }

  bool contains_extent(ext_ptr extent) override {
    for (auto &kv : extent_stack) {
      for (auto &l : kv.second) {
        if (find(l.begin(), l.end(), extent) != l.end())
          return true;
      }
    }
    return false;
  }

  void remove_extent(ext_ptr extent) override {
    auto it = extent_stack.begin();
    while(it != extent_stack.end())
    {
      auto lst_it = it->second.begin();
      *lst_it->begin();
      while(lst_it != it->second.end())
      {
        auto found = std::find(lst_it->begin(), lst_it->end(), extent);
        if(found != lst_it->end())
        {
          lst_it->erase(found);
        }
        if(lst_it->empty())
        {
          lst_it = it->second.erase(lst_it);
        }else{
          lst_it++;
        }
      }
      if(it->second.size() == 0)
      {
        it = extent_stack.erase(it);
      }else{
        it++;
      }
    }
  }
};
