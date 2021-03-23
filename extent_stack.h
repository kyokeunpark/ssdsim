#pragma once
#include "stripe_manager.h"
#include <algorithm>
#include <list>
#include <map>
#include <memory>
#include <random>
typedef list<Extent *> *extent_stack_ext_lst;
/*
 * Struct used to check if a pointer to an extent exists within a
 * list of extents. It is kind of a hack, but works well, given the current
 * structure of the ExtentStack class.
 */
struct isExtent {
    Extent *extent;

    isExtent(Extent *e) : extent(e) {}

    bool operator()(const Extent *e) const { return this->extent == e; }
};

using stack_val = std::list<Extent *>;
using ext_stack = std::map<int, stack_val>;

// Used for WholeObjectExtentStack, which stores extent lists together
using stack_lst = std::list<stack_val>;
using ext_lst_stack = std::map<int, stack_lst>;

class AbstractExtentStack {
  protected:
    shared_ptr<StripeManager> stripe_manager;

  public:
    AbstractExtentStack() : stripe_manager(nullptr) {}
    AbstractExtentStack(shared_ptr<StripeManager> s_m) : stripe_manager(s_m) {}
    virtual ~AbstractExtentStack() {}

    virtual int num_stripes(int stripe_size) = 0;
    virtual stack_val pop_stripe_num_exts(int stripe_size) = 0;
    virtual void add_extent(int key, Extent *ext) = 0;
    virtual int get_length_of_extent_stack() = 0;
    virtual int get_length_at_key(int key) = 0;
    virtual Extent *get_extent_at_key(int key) = 0;
    virtual bool contains_extent(Extent *extent) = 0;
    virtual void remove_extent(Extent *extent) = 0;

    virtual Extent *get_extent_at_closest_key(int key)
    {
        return this->get_extent_at_key(key);
    }
};

class ExtentStack : public AbstractExtentStack {

  protected:
    // ordered by key
    ext_stack extent_stack;

  public:
    ExtentStack(shared_ptr<StripeManager> s_m)
        : AbstractExtentStack(s_m), extent_stack(ext_stack()) {}

    virtual int num_stripes(int stripe_size) override = 0;

    virtual stack_val pop_stripe_num_exts(int stripe_size) override = 0;

    void add_extent(int key, Extent *ext) override {
        if (this->extent_stack.find(key) != this->extent_stack.end())
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

    virtual int get_length_at_key(int key) override {
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
    virtual Extent *get_extent_at_key(int key) override {
        if (extent_stack.find(key) == extent_stack.end())
            return nullptr;
        stack_val exts = this->extent_stack[key];
        Extent *ret = exts.front();
        exts.pop_front();
        if (exts.size() == 0)
            extent_stack.erase(key);
        return ret;
    }

    virtual bool contains_extent(Extent *extent) override {
        for (auto &kv : extent_stack) {
            if (find(kv.second.begin(), kv.second.end(), extent) !=
                kv.second.end())
                return true;
        }
        return false;
    }

    // can end early
    virtual void remove_extent(Extent *extent) override {
        for (auto &kv : extent_stack) {
            kv.second.remove(extent);
            if (kv.second.size() == 0)
                extent_stack.erase(kv.first);
        }
    }
};

class SingleExtentStack : public ExtentStack {
    using ExtentStack::ExtentStack;

  public:
    ext_stack get_extent_stack() { return extent_stack; }
    int num_stripes(int stripe_size) override {
        return get_length_of_extent_stack() / stripe_size;
    }

    list<Extent *> pop_stripe_num_exts(int stripe_size) override {
        list<Extent *> ret;
        int num_left_to_add = stripe_size;
        if (this->get_length_of_extent_stack() < num_left_to_add) {
            return ret;
        }
        for (auto &kv : extent_stack) {
            list<Extent *> ext_lst = kv.second;
            for (int i = 0;
                 i < (kv.second.size() > num_left_to_add ? num_left_to_add
                                                         : kv.second.size());
                 i++) {
                ret.push_back(ext_lst.front());
                ext_lst.pop_front();
            }
            if (ext_lst.size() == 0) {
                this->extent_stack.erase(kv.first);
            }
            num_left_to_add = stripe_size - ret.size();
        }
        return ret;
    }
};

class MultiExtentStack : public ExtentStack {
    using ExtentStack::ExtentStack;
    int num_stripes(int stripe_size) {
        int num_stripes = 0;
        for (auto &kv : extent_stack) {
            num_stripes += kv.second.size() / stripe_size;
        }
        return num_stripes;
    }

    list<Extent *> pop_stripe_num_exts(int stripe_size) {
        list<Extent *> ret;
        for (auto &kv : extent_stack) {
            list<Extent *> ext_lst = kv.second;
            if (ext_lst.size() >= stripe_size) {
                for (int i = 0; i < stripe_size; i++) {
                    ret.push_back(ext_lst.front());
                    ext_lst.pop_front();
                }
                if (ext_lst.size() == 0) {
                    extent_stack.erase(kv.first);
                }
                return ret;
            }
        }
        return ret;
    }
};

class BestEffortExtentStack : public SingleExtentStack {
  public:
    using SingleExtentStack::SingleExtentStack;
    // double check correctness
    Extent *get_extent_at_closest_key(int key) override {
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
        int next_key = next_pos->first;
        int prev_key = prev_pos->first;
        if (next_key - key < key - prev_key) {
            return get_extent_at_key(next_key);
        }
        return get_extent_at_key(prev_key);
    }
};
class ExtentStackRandomizer : public AbstractExtentStack {
  public:
    shared_ptr<SingleExtentStack> extent_stack;
    ExtentStackRandomizer(shared_ptr<SingleExtentStack> e_s)
        : extent_stack(e_s) {}
    int num_stripes(int stripe_size) override {
        return extent_stack->num_stripes(stripe_size);
    }
    void add_extent(int e, Extent *s) override {
        extent_stack->add_extent(e, s);
    }
    int get_length_at_key(int k) override {
        return extent_stack->get_length_at_key(k);
    }
    int get_length_of_extent_stack() override {
        return extent_stack->get_length_of_extent_stack();
    }
    bool contains_extent(Extent *extent) override {
        return extent_stack->contains_extent(extent);
    }
    void remove_extent(Extent *extent) override {
        extent_stack->remove_extent(extent);
    }
    list<Extent *> pop_stripe_num_exts(int stripe_size) override {
        for (auto &kv : extent_stack->get_extent_stack()) {
            auto rng = std::default_random_engine{};
            std::shuffle(*kv.second.begin(), *kv.second.end(), rng);
        }
        return extent_stack->pop_stripe_num_exts(stripe_size);
    }
    Extent *get_extent_at_closest_key(int key) {
        for (auto &kv : extent_stack->get_extent_stack()) {
            auto rng = std::default_random_engine{};
            std::shuffle(*kv.second.begin(), *kv.second.end(), rng);
        }
        return extent_stack->get_extent_at_closest_key(key);
    }
    Extent *get_extent_at_key(int key) override {
        for (auto &kv : extent_stack->get_extent_stack()) {
            auto rng = std::default_random_engine{};
            std::shuffle(*kv.second.begin(), *kv.second.end(), rng);
        }
        return extent_stack->get_extent_at_key(key);
    }
};

class WholeObjectExtentStack : public ExtentStack {
    using ExtentStack::ExtentStack;

    ext_lst_stack extent_stack;

  public:
    WholeObjectExtentStack(shared_ptr<StripeManager> stripe_manager)
        : ExtentStack(stripe_manager) {
        this->extent_stack = ext_lst_stack();
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

    void add_extent(stack_val &ext_lst) {
        int key = ext_lst.size();
        if (this->extent_stack.find(key) != this->extent_stack.end())
            extent_stack.emplace(key, stack_lst());
        extent_stack[key].emplace_back(ext_lst);
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
            auto it = extent_stack.upper_bound(temp);
            if (it != extent_stack.begin())
                it = prev(it);

            stack_val &lst = *it->second.begin();
            it->second.begin()->pop_front();
            temp -= lst.size();
            for (auto e : lst) {
                lst.pop_front();
                ret.push_back(e);
            }
            if (it->second.size() == 0)
                extent_stack.erase(it->first);
        }
        return ret;
    }

    /*error prone double check*/
    stack_val pop_stripe_num_exts(int stripe_size) override {
        stack_val ret;
        int num_left_to_add = stripe_size;

        if (get_length_of_extent_stack() < num_left_to_add)
            return ret;

        int largest_key = prev(extent_stack.end())->first;
        auto largest = prev(extent_stack.end());
        stack_val longest_lst = *largest->second.begin();
        largest->second.pop_front();
        for (int i = 0; i < (num_left_to_add > largest_key ? largest_key
                                                           : num_left_to_add);
             i++) {
            ret.push_back(longest_lst.front());
            longest_lst.pop_front();
        }

        num_left_to_add = stripe_size - ret.size();
        if (longest_lst.size() == 0)
            extent_stack.erase(largest_key);
        if (longest_lst.size() > 0)
            add_extent(longest_lst);
        if (num_left_to_add > 0)
            for (Extent *e : fill_gap(num_left_to_add))
                ret.push_back(e);

        return ret;
    }

    /* not sure if this is correct??????
    def get_length_at_key(self, key):
        """Returns the number of extents in extent stack at key
        """
        return self.get_length_of_extent_stack()
    */
    int get_length_at_key(int key) override {
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
    Extent *get_extent_at_key(int k) override {
        stack_val &exts = extent_stack.begin()->second.front();
        Extent *ext = exts.front();
        exts.pop_front();
        if (exts.size() == 0)
            extent_stack.erase(extent_stack.begin()->first);
        return ext;
    }

    bool contains_extent(Extent *extent) override {
        for (auto &kv : extent_stack) {
            for (auto &l : kv.second) {
                if (find_if(l.begin(), l.end(), isExtent(extent)) != l.end())
                    return true;
            }
        }
        return false;
    }

    void remove_extent(Extent *extent) override {
        for (auto &kv : extent_stack) {
            for (auto &l : kv.second) {
                auto it = find(l.begin(), l.end(), extent);
                if (it != l.end()) {
                    l.remove(extent);
                    if (l.size() == 0)
                        kv.second.remove(l);
                }
            }
            if (kv.second.size() == 0)
                extent_stack.erase(kv.first);
        }
    }
};