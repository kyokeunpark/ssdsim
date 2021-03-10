#include "stripe_manager.h"
#include "extent_manager.h"
#include "extent_stack.h"
#include <array>
template<class es_v_T, class es_k_T, class sim_time_T>
class AbstractStriper{
    public:
        StripeManager * stripe_manager;
        ExtentManager * extent_manager;
        int num_times_alternatives;
        int num_times_default;
        AbstractStriper(){}
        AbstractStriper(StripeManager * s_m, ExtentManager * e_m):
            stripe_manager(s_m), extent_manager(e_m), num_times_alternatives(0),
            num_times_default(0){}

        virtual array<int,3> create_stripes
            (ExtentStack<es_v_T, es_k_T> * extent_stack, sim_time_T * simulation_time) = 0;
        virtual array<int,3>  create_stripe
            (ExtentStack<es_v_T, es_k_T> * extent_stack, sim_time_T * simulation_time) = 0;
        virtual array<int,7>  cost_to_replace_extents(int ext_size, int exts_per_locality, double obs_data_per_locality) = 0;
        virtual double cost_to_write_data(int data) = 0;
        virtual int num_stripes_reqd() = 0;
};

class SimpleStriper : public AbstractStriper<class es_v_T, class es_k_T, class sim_time_T>{
    public:
        using AbstractStriper::AbstractStriper;
        list<string> ext_types;
        int num_stripes_reqd()
        {
            return 1;
        }
        array<int,3> create_stripes(ExtentStack<es_v_T, es_k_T> * extent_stack, sim_time_T * simulation_time)
        {
            int num_exts = stripe_manager->num_data_exts_per_stripe;
            int writes = 0;
            int reads = 0;
            int stripes = 0;
            list<Extent *> exts_to_stripe = extent_stack->pop_stripe_num_exts(num_exts);
            Stripe * current_stripe = stripe_manager->create_new_stripe(exts_to_stripe.front()->ext_size);

            for(int i = 0; i < num_exts; i++)
            {
                Extent * ext = exts_to_stripe.front();
                exts_to_stripe.pop_front();
                current_stripe->add_extent(ext);
                writes += ext->ext_size;
                reads += ext->ext_size;
            }
            stripes += 1;
            return array<int, 3>{stripes, reads, writes};
        }
        array<int,7>  cost_to_replace_extents(int ext_size, int exts_per_locality, double obs_data_per_locality)
        {
            return {0, 0, 0, 0, 0, 0, 0};
        }
        double cost_to_write_data(int data)
        {
            return data;
        }
};

class AbstractStriperDecorator:public AbstractStriper<class es_v_T, class es_k_T, class sim_time_T>
{
    public:
        AbstractStriper * striper;
        AbstractStriperDecorator(AbstractStriper * s):striper(s){}
};

class ExtentStackStriper: public AbstractStriperDecorator
{
    public:
    using AbstractStriperDecorator::AbstractStriperDecorator;
    int num_reqd()
    {
        return 0;
    }

    array<int,3> create_stripes(ExtentStack<es_v_T, es_k_T> * extent_stack, sim_time_T * simulation_time)
    {
        int num_exts = stripe_manager->num_data_exts_per_stripe;
        int total_writes = 0;
        int total_reads = 0;
        int stripes = 0;
        while(extent_stack->num_stripes(num_exts))
        {
            array<int, 3> res = striper->create_stripes(extent_stack, simulation_time);
            total_writes += res[2];
            total_reads += res[1];
            stripes += res[0];
        }
        stripes += 1;
        return array<int, 3>{stripes, total_reads, total_writes};
    }

    array<int,3> create_stripe(ExtentStack<es_v_T, es_k_T> * extent_stack, sim_time_T * simulation_time)
    {
        int num_exts = stripe_manager->num_data_exts_per_stripe;
        int total_writes = 0;
        int total_reads = 0;
        int stripes = 0;
        if(extent_stack->num_stripes(num_exts))
        {
            array<int, 3> res = striper->create_stripes(extent_stack, simulation_time);
            total_writes += res[2];
            total_reads += res[1];
            stripes += res[0];
        }
        stripes += 1;
        return array<int, 3>{stripes, total_reads, total_writes};
    }
    array<int,7>  cost_to_replace_extents(int ext_size, int exts_per_locality, double obs_data_per_locality)
    {
        return striper->cost_to_replace_extents(ext_size, exts_per_locality, obs_data_per_locality);
    }
    double cost_to_write_data(int data)
    {
        return data;
    }
};



class NumStripesStriper: public AbstractStriperDecorator
{
    public:
    int num_stripes_per_cycle;
    NumStripesStriper(int n, AbstractStriper * s):AbstractStriperDecorator(s),  num_stripes_per_cycle(n){}

    int num_reqd()
    {
        return num_stripes_per_cycle;
    }

    array<int,3> create_stripes(ExtentStack<es_v_T, es_k_T> * extent_stack, sim_time_T * simulation_time)
    {
        int total_writes = 0;
        int total_reads = 0;
        int total_stripes = 0;
        for(int i = 0; i < num_stripes_per_cycle; i++)
        {
            array<int, 3> res = striper->create_stripes(extent_stack, simulation_time);
            total_stripes += res[0];
            total_writes += res[2];
            total_reads += res[1];
        }
        return array<int, 3>{total_stripes, total_reads, total_writes};
    }

    array<int,3> create_stripe(ExtentStack<es_v_T, es_k_T> * extent_stack, sim_time_T * simulation_time)
    {
        int total_writes = 0;
        int total_stripes = 0;
        int total_reads = 0;
        array<int, 3> res = striper->create_stripes(extent_stack, simulation_time);
        total_stripes += res[0];
        total_writes += res[2];
        total_reads += res[1];
        return array<int, 3>{total_stripes, total_reads, total_writes};
    }
    array<int,7>  cost_to_replace_extents(int ext_size, int exts_per_locality, double obs_data_per_locality)
    {
        return striper->cost_to_replace_extents(ext_size, exts_per_locality, obs_data_per_locality);
    }
    double cost_to_write_data(int data)
    {
        return data;
    }
};





class StriperWithEC: public AbstractStriperDecorator
{
    public:
    using AbstractStriperDecorator::AbstractStriperDecorator;

    int num_reqd()
    {
        return striper->num_stripes_reqd();
    }

    array<int,3> create_stripes(ExtentStack<es_v_T, es_k_T> * extent_stack, sim_time_T * simulation_time)
    {
            array<int, 3> res = striper->create_stripes(extent_stack, simulation_time);
            int stripes = res[0];
            int writes = res[2];
            int reads = res[1];
            writes *= stripe_manager->coding_overhead;
        return array<int, 3>{stripes, reads, writes};
    }

    array<int,3> create_stripe(ExtentStack<es_v_T, es_k_T> * extent_stack, sim_time_T * simulation_time)
    {
        array<int, 3> res = striper->create_stripe(extent_stack, simulation_time);
        int stripes = res[0];
        int writes = res[2];
        int reads = res[1];
        writes *= stripe_manager->coding_overhead;
        return array<int, 3>{stripes, reads, writes};
    }
    array<int,7>  cost_to_replace_extents(int ext_size, vector<int> exts_per_locality, 
                vector<int>  obs_data_per_locality, vector<int>valid_objs_per_locality)
    {
        int global_parity_reads = 0;
        int global_parity_writes = 0;
        int local_parity_reads = 0;
        int local_parity_writes = 0;
        int valid_obj_reads = 0;
        int obsolete_data_reads = 0;
        int absent_data_reads = 0;
        int total_exts_replaced = 0;
        for(int i = 0; i < exts_per_locality.size(); i++)
        {
            total_exts_replaced += exts_per_locality[i];
        }
        if (total_exts_replaced == stripe_manager->num_data_exts_per_stripe)
        {
            return array<int, 7>{ global_parity_reads, stripe_manager->num_global_parities * ext_size, local_parity_reads,
                stripe_manager->num_local_parities * ext_size, obsolete_data_reads, valid_obj_reads, absent_data_reads};
        }
        int num_exts_per_locality = stripe_manager->num_data_exts_per_locality;
        for(int i = 0; i < obs_data_per_locality.size(); i++)
        {
            int num_exts = exts_per_locality[i];
            if (num_exts == num_exts_per_locality){
                valid_obj_reads += valid_objs_per_locality[i];

                // read in obsolete data - needed for updating global parities
                obsolete_data_reads += obs_data_per_locality[i];
                // compute new parity block and write it out
                local_parity_writes += ext_size;
            }// replacing some extents in locality
            else if(num_exts != 0)
            {
                valid_obj_reads += valid_objs_per_locality[i];
                obsolete_data_reads += obs_data_per_locality[i];
                local_parity_reads += ext_size;
                local_parity_writes += ext_size;
            }
            global_parity_reads += stripe_manager->num_global_parities * ext_size;
            global_parity_writes += stripe_manager->num_global_parities * ext_size;
            num_times_default += 1;
        }
        return array<int, 7>{global_parity_reads, global_parity_writes, 
            local_parity_reads, local_parity_writes, obsolete_data_reads, valid_obj_reads, absent_data_reads};
    }
    double cost_to_write_data(int data)
    {
        return data;
    }
};

class EfficientStriperWithEC: public StriperWithEC
{
    using StriperWithEC::StriperWithEC;

    array<int,7> cost_to_replace_extents(int ext_size, vector<int> exts_per_locality, 
                vector<int>  obs_data_per_locality, vector<int>valid_objs_per_locality)
    {
        int str1_ec_reads = 0;
        int str2_ec_reads = 0;
        int global_parity_reads = 0;
        int global_parity_writes = 0;
        int local_parity_reads = 0;
        int local_parity_writes = 0;
        int obsolete_data_reads = 0;
        int absent_data_reads = 0;
        int valid_obj_reads = 0;
        int total_exts_replaced;
        for(int i = 0; i < exts_per_locality.size(); i++)
        {
            total_exts_replaced += exts_per_locality[i];
        }
        if (total_exts_replaced == stripe_manager->num_data_exts_per_stripe){
            return array<int, 7>{global_parity_reads, stripe_manager->num_global_parities * ext_size,
             local_parity_reads, stripe_manager->num_local_parities * ext_size, obsolete_data_reads,
              valid_obj_reads, absent_data_reads};
        }
        int num_exts_per_locality = stripe_manager->num_data_exts_per_locality;
        for(int i = 0 ; i < obs_data_per_locality.size(); i++)
        {
            int num_exts = exts_per_locality[i];
            // whole locality is replaced
            if (num_exts == num_exts_per_locality){
                valid_obj_reads += valid_objs_per_locality[i];
                // read in obsolete data - needed for updating global parities
                obsolete_data_reads += obs_data_per_locality[i];
                // compute new parity block and write it out
                local_parity_writes += ext_size;
            }
            // replacing some extents in locality
            else if (num_exts != 0){
                valid_obj_reads += valid_objs_per_locality[i];

                // read in obsolete data
                obsolete_data_reads += obs_data_per_locality[i];
                // read in parity
                local_parity_reads += ext_size;

                // in this strategy read in the extents not being gc'ed instead and recompute the parities from scratch
                absent_data_reads += (num_exts_per_locality - num_exts) * ext_size;

                // recompute and write out parity
                local_parity_writes += ext_size;
            }
            else if (num_exts == 0){
                // need to bring in the extents from any locality not gc'ed to compute global parities
                absent_data_reads += (num_exts_per_locality) * ext_size;
            }
        }
        global_parity_reads += stripe_manager->num_global_parities * ext_size;
        global_parity_writes += stripe_manager->num_global_parities * ext_size;
        str1_ec_reads = global_parity_reads + obsolete_data_reads + local_parity_reads;
        if (str1_ec_reads < absent_data_reads)
        {
            num_times_default += 1;
            return array<int, 7> {global_parity_reads, global_parity_writes, local_parity_reads, 
                local_parity_writes, obsolete_data_reads, valid_obj_reads, 0};
        }else {
            num_times_alternatives += 1;
            return array<int, 7>{0, global_parity_writes, 0, local_parity_writes, 0, 0, absent_data_reads};
        }
    }
};