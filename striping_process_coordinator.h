#pragma once
#include "extent_object.h"
#include "object_packers.h"
#include "stripe_manager.h"
#include "stripers.h"
#include <memory>
template <class extent_stack_value_type, class extent_stack_key_type, class sim_T>
class StripingProcessCoordinator{
    public:
        shared_ptr<SimpleObjectPacker> object_packer;
        shared_ptr<SimpleGCObjectPacker> gc_object_packer;
        shared_ptr<AbstractStriperDecorator> striper;
        shared_ptr<AbstractStriperDecorator> gc_striper;
        shared_ptr<ExtentStack< extent_stack_value_type, extent_stack_key_type>> extent_stack;
        shared_ptr<ExtentStack< extent_stack_value_type, extent_stack_key_type>> gc_extent_stack;
        shared_ptr<StripeManager> stripe_manager;
        sim_T simulation_time;
        StripingProcessCoordinator(shared_ptr<SimpleObjectPacker> o_p,
            shared_ptr<SimpleGCObjectPacker> gc_o_p,
            shared_ptr<AbstractStriperDecorator>s, shared_ptr<AbstractStriperDecorator>gc_s,
            shared_ptr<ExtentStack< extent_stack_value_type, extent_stack_key_type>> e_s, 
            shared_ptr<ExtentStack< extent_stack_value_type, extent_stack_key_type>>gc_e_s,  
            shared_ptr<StripeManager> s_m, sim_T s_t):
            object_packer(o_p), gc_object_packer(gc_o_p), striper(s),
            gc_striper(gc_s), extent_stack(e_s), gc_extent_stack(gc_e_s), 
            stripe_manager(s_m), simulation_time(s_t)
        {}

        void gc_extent(shared_ptr<Extent> ext, 
            shared_ptr<ExtentStack<extent_stack_value_type, extent_stack_key_type>> extent_stack,
            list<Extent_Object *> objs)
        {
            gc_object_packer->gc_extent(ext, extent_stack, objs);

        }

        Extent * get_gc_extent(int key = 0)
        {
            if (gc_extent_stack->get_length_at_key(key) > 0){
                return gc_extent_stack->get_extent_at_key(key);
            }
            return nullptr;
        }

        Extent * get_extent(int key=0)
        {
            if (extent_stack->get_length_at_key(key) > 0)
            {
                return extent_stack->get_extent_at_key(key);
            }
            else
            {
                object_packer->generate_exts_at_key(extent_stack, 1, key);
                return extent_stack->get_extent_at_key(key);
            }
        }

        Stripe * get_stripe(int key = 0)
        {
            int num_exts_per_stripe = stripe_manager->num_data_exts_per_stripe;
            int num_exts_at_key = extent_stack->get_length_at_key(key);
            if(num_exts_at_key >= num_exts_per_stripe)
            {
                return striper->create_stripe(extent_stack, simulation_time);
            }
            else {
                object_packer->generate_exts_at_key(extent_stack, num_exts_per_stripe, key);
                return striper->create_stripe(extent_stack, simulation_time);
            }
        }
        array<int, 3> stripe_generator(shared_ptr<AbstractStriperDecorator> striper, 
            shared_ptr<SimpleObjectPacker> objecct_packer,
            shared_ptr<ExtentStack<extent_stack_value_type, extent_stack_key_type>> extent_stack)
        {
            object_packer->generate_stripes(extent_stack, simulation_time);
            return striper->create_stripes(extent_stack, simulation_time);
        }

        void pack_exts(int num_exts = 0, int key = 0)
        {
            object_packer->generate_exts_at_key(extent_stack, num_exts, key);
        }
        void generate_exts()
        {
            gc_object_packer->generate_extents();
        }
        void generate_objs(int space)
        {
            gc_object_packer->generate_objs(space);
        }

        map<string, int> get_ext_types()
        {
            auto types = object_packer->ext_types;
            auto gc_types = gc_object_packer->ext_types;
            for (auto & it: gc_types)
            {
                if(types.find(it.first)!= types.end())
                {
                    types.emplace(it.first, it.second + types.find(it.first)->second);
                }
                else
                {
                    types.emplace(it.first, types.find(it.first)->second);
                }
            }
            return types;
        }

        array<int, 3> generate_stripes()
        {
            return stripe_generator(striper, object_packer, extent_stack);
        }
        array<int, 3> generate_gc_stripes()
        {
            return stripe_generator(gc_striper, gc_object_packer, gc_extent_stack);
        }
        int get_length_extent_stack()
        {
            return extent_stack->get_length_of_extent_stack();
        }
        int get_length_gc_extent_stack()
        {
            return gc_extent_stack->get_length_of_extent_stack();
        }
        void del_sealed_extent(Extent * extent)
        {
            cout << extent << endl;
            auto objs = extent->delete_ext();
            if(extent_stack->contains_extent(extent))
            {
                extent_stack->remove_extent(extent);
                object_packer->add_objs(objs);
                object_packer->pack_objects(extent_stack);
            }else if(gc_extent_stack->contains_extent(extent))
            {
                gc_extent_stack->remove_extent(extent);
                gc_object_packer->add_objs(objs);
                gc_object_packer->pack_objects(gc_extent_stack);
            }
                
        }

        bool extent_in_extent_stacks(Extent * extent)
        {
            return extent_stack->contains_extent(extent) || gc_extent_stack->contains_extent(extent);
        }

        array<double, 2> get_proporition_of_stripers()
        {
            int num_times_alternative = striper->num_times_alternatives + gc_striper->num_times_alternatives;
            int num_times_default = striper->num_times_default + gc_striper->num_times_default;
            if (num_times_alternative == 0 && num_times_default == 0)
            {
                return array<double, 2>{0, 0};
            }
            int total = num_times_alternative + num_times_default;
            return array<double, 2>{num_times_default* 100.0/total , num_times_alternative * 100.0/total};
        }
};

template <class extent_stack_value_type, class extent_stack_key_type, class sim_T>
class BestEffortStripingProcessCoordinator
    :public StripingProcessCoordinator<extent_stack_value_type, extent_stack_key_type, sim_T>
{
    public:
        extent_stack_key_type default_key;
        BestEffortStripingProcessCoordinator(shared_ptr<SimpleObjectPacker> o_p,
            shared_ptr<SimpleGCObjectPacker> gc_o_p,
            shared_ptr<AbstractStriperDecorator>s, shared_ptr<AbstractStriperDecorator>gc_s,
            shared_ptr<ExtentStack< extent_stack_value_type, extent_stack_key_type>> e_s, 
            shared_ptr<ExtentStack< extent_stack_value_type, extent_stack_key_type>>gc_e_s,  
            shared_ptr<StripeManager> s_m, sim_T s_t,extent_stack_key_type key)
            :StripingProcessCoordinator<extent_stack_value_type, extent_stack_key_type, sim_T>
                (o_p, gc_o_p,s,gc_s,e_s,gc_e_s,s_m, s_t), default_key(key)
        {}

    Extent * get_extent(extent_stack_key_type key)
    {
        if (this->extent_stack->get_length_at_key(key) > 0)
        {
             return this->extent_stack->get_extent_at_key(key);
        }else {
            if (this->extent_stack->get_length_of_extent_stack())
            {
                return this->extent_stack->get_extent_at_closest_key(key);
            }else {
                this->object_packer->generate_exts_at_key(this->extent_stack, 1, default_key);
                return this->extent_stack->get_extent_at_closest_key(key);
            }
                
        }
    }
    Extent * get_gc_extent(extent_stack_key_type key)
    {
        if (this->gc_extent_stack->get_length_at_key(key) > 0)
        {
            return this->gc_extent_stack.get_extent_at_key(key);
        }else {
            if (this->gc_extent_stack->get_length_of_extent_stack())
            {
                return this->gc_extent_stack->get_extent_at_closest_key(key);
            }else
            {
                this->object_packer->generate_exts_at_key(this->extent_stack, 1, default_key);
                return this->extent_stack->get_extent_at_closest_key(key);
            }
                
        }
            
    }
        
};