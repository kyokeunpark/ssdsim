#ifndef __STRIPING_PROCESS_COORDINATOR_H_
#define __STRIPING_PROCESS_COORDINATOR_H_
#include "extent_object.h"
#include "extent_stack.h"
#include "object_packer.h"
#include "stripe_manager.h"
#include "stripers.h"
#include <any>
#include <memory>
class StripingProcessCoordinator {
  public:
    shared_ptr<SimpleObjectPacker> object_packer;
    shared_ptr<SimpleGCObjectPacker> gc_object_packer;
    shared_ptr<AbstractStriperDecorator> striper;
    shared_ptr<AbstractStriperDecorator> gc_striper;
    shared_ptr<AbstractExtentStack> extent_stack;
    shared_ptr<AbstractExtentStack> gc_extent_stack;
    shared_ptr<StripeManager> stripe_manager;
    float simulation_time;
    StripingProcessCoordinator(shared_ptr<SimpleObjectPacker> o_p,
                               shared_ptr<SimpleGCObjectPacker> gc_o_p,
                               shared_ptr<AbstractStriperDecorator> s,
                               shared_ptr<AbstractStriperDecorator> gc_s,
                               shared_ptr<AbstractExtentStack> e_s,
                               shared_ptr<AbstractExtentStack> gc_e_s,
                               shared_ptr<StripeManager> s_m, float s_t)
        : object_packer(o_p), gc_object_packer(gc_o_p), striper(s),
          gc_striper(gc_s), extent_stack(e_s), gc_extent_stack(gc_e_s),
          stripe_manager(s_m), simulation_time(s_t) {}

    void gc_extent(Extent *ext, set<Extent_Object *> objs) {
        gc_object_packer->gc_extent(ext, gc_extent_stack, objs);
    }

    Extent *get_gc_extent(int key = 0) {
        if (gc_extent_stack->get_length_at_key(key) > 0) {
            return gc_extent_stack->get_extent_at_key(key);
        }
        return nullptr;
    }

    Extent *get_extent(int key = 0) {
        if (extent_stack->get_length_at_key(key) > 0) {
            return extent_stack->get_extent_at_key(key);
        } else {
            object_packer->generate_exts_at_key(extent_stack, 1, key);
            return extent_stack->get_extent_at_key(key);
        }
    }

    array<int, 3> get_stripe(int key = 0) {
        int num_exts_per_stripe = stripe_manager->num_data_exts_per_stripe;
        int num_exts_at_key = extent_stack->get_length_at_key(key);
        if (num_exts_at_key >= num_exts_per_stripe) {
            return striper->create_stripe(extent_stack, simulation_time);
        } else {
            object_packer->generate_exts_at_key(extent_stack,
                                                num_exts_per_stripe, key);
            return striper->create_stripe(extent_stack, simulation_time);
        }
    }

    array<int, 3>
    stripe_generator(shared_ptr<AbstractStriperDecorator> striper,
                     shared_ptr<SimpleObjectPacker> objecct_packer,
                     shared_ptr<AbstractExtentStack> extent_stack) {
        object_packer->generate_stripes(extent_stack, simulation_time);
        return striper->create_stripes(extent_stack, simulation_time);
    }

    void pack_exts(int num_exts = 0, int key = 0) {
        object_packer->generate_exts_at_key(extent_stack, num_exts, key);
    }
    void generate_extents() { gc_object_packer->generate_exts(); }
    void generate_objs(int space) { gc_object_packer->generate_objs(space); }

    ext_types_mgr get_ext_types() {
        auto types = object_packer->get_ext_types();
        auto gc_types = gc_object_packer->get_ext_types();
        for (auto &it : gc_types) {
            if (types.find(it.first) != types.end()) {
                types.emplace(it.first,
                              it.second + types.find(it.first)->second);
            } else {
                types.emplace(it.first, types.find(it.first)->second);
            }
        }
        return types;
    }

    array<int, 3> generate_stripes() {
        return stripe_generator(striper, object_packer, extent_stack);
    }
    array<int, 3> generate_gc_stripes() {
        return stripe_generator(gc_striper, gc_object_packer, gc_extent_stack);
    }
    int get_length_extent_stack() {
        return extent_stack->get_length_of_extent_stack();
    }
    int get_length_gc_extent_stack() {
        return gc_extent_stack->get_length_of_extent_stack();
    }
    void del_sealed_extent(Extent *extent) {
        cout << extent << endl;
        auto objs = extent->delete_ext();
        if (extent_stack->contains_extent(extent)) {
            extent_stack->remove_extent(extent);
            object_packer->add_objs(objs);
            object_packer->pack_objects(extent_stack);
        } else if (gc_extent_stack->contains_extent(extent)) {
            gc_extent_stack->remove_extent(extent);
            gc_object_packer->add_objs(objs);
            gc_object_packer->pack_objects(gc_extent_stack);
        }
    }

    bool extent_in_extent_stacks(Extent *extent) {
        return extent_stack->contains_extent(extent) ||
               gc_extent_stack->contains_extent(extent);
    }

    array<double, 2> get_proporition_of_stripers() {
        int num_times_alternative = striper->num_times_alternatives +
                                    gc_striper->num_times_alternatives;
        int num_times_default =
            striper->num_times_default + gc_striper->num_times_default;
        if (num_times_alternative == 0 && num_times_default == 0) {
            return array<double, 2>{0, 0};
        }
        int total = num_times_alternative + num_times_default;
        return array<double, 2>{num_times_default * 100.0 / total,
                                num_times_alternative * 100.0 / total};
    }
};

class BestEffortStripingProcessCoordinator : public StripingProcessCoordinator {
  public:
    any (*default_key)();
    BestEffortStripingProcessCoordinator(
        shared_ptr<SimpleObjectPacker> o_p,
        shared_ptr<SimpleGCObjectPacker> gc_o_p,
        shared_ptr<AbstractStriperDecorator> s,
        shared_ptr<AbstractStriperDecorator> gc_s,
        shared_ptr<AbstractExtentStack> e_s,
        shared_ptr<AbstractExtentStack> gc_e_s, shared_ptr<StripeManager> s_m,
        float s_t, any (*key)())
        : StripingProcessCoordinator(o_p, gc_o_p, s, gc_s, e_s, gc_e_s, s_m,
                                     s_t),
          default_key(key) {}

    Extent *get_extent(short key) {
        if (this->extent_stack->get_length_at_key(key) > 0) {
            return this->extent_stack->get_extent_at_key(key);
        } else {
            if (this->extent_stack->get_length_of_extent_stack()) {
                return this->extent_stack->get_extent_at_closest_key(key);
            } else {
                this->object_packer->generate_exts_at_key(
                    this->extent_stack, 1, std::any_cast<int>(default_key()));
                return this->extent_stack->get_extent_at_closest_key(key);
            }
        }
    }
    Extent *get_gc_extent(int key) {
        if (this->gc_extent_stack->get_length_at_key(key) > 0) {
            return this->gc_extent_stack->get_extent_at_key(key);
        } else {
            if (this->gc_extent_stack->get_length_of_extent_stack()) {
                return this->gc_extent_stack->get_extent_at_closest_key(key);
            } else {
                this->object_packer->generate_exts_at_key(
                    this->extent_stack, 1, std::any_cast<int>(default_key()));
                return this->extent_stack->get_extent_at_closest_key(key);
            }
        }
    }
    array<int, 3> get_stripe(int key = 0) {
        int num_exts_per_stripe =
            this->stripe_manager->num_data_exts_per_stripe;
        int num_exts = this->extent_stack->get_length_of_extent_stack();
        if (num_exts >= num_exts_per_stripe) {
            return this->striper->create_stripe(this->extent_stack,
                                                this->simulation_time);
        } else {
            this->object_packer->generate_exts_at_key(
                this->extent_stack, num_exts_per_stripe,
                std::any_cast<int>(default_key()));
            return this->striper->create_stripe(this->extent_stack,
                                                this->simulation_time);
        }
    }
};
#endif