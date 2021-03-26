#ifndef __DATA_CENTER_H_
#define __DATA_CENTER_H_

#pragma once

#include <iostream>
#include <algorithm>
#include <set>
#include <unordered_map>
#include "config.h"
#include "event_manager.h"
#include "extent_manager.h"
#include "gc_strategies.h"
#include "object_manager.h"
#include "stripe_manager.h"
#include "stripers.h"
#include "striping_process_coordinator.h"

using std::set;
using std::unordered_map;
using std::cout, std::cerr, std::endl;

struct del_result {
    int total_added_obsolete = 0;
    set<Stripe*> gc_stripes_set = set<Stripe*>();
    unordered_map<string, int> ext_types = unordered_map<string, int>();
};

// Event handler result
struct eh_result {
    int total_reclaimed_space = 0;
    int total_obsolete = 0;
    int total_used_space = 0;
    int dc_size = 0;
    int total_leftovers = 0;
    int total_global_parity_reads = 0;
    int total_global_parity_writes = 0;
    int total_local_parity_reads = 0;
    int total_local_parity_writes = 0;
    int total_obsolete_data_reads = 0;
    int total_absent_data_reads = 0;
    int total_valid_obj_transfers = 0;
    int total_storage_node_to_parity_calculator = 0;
    double max_obs_perc = 0;
    int total_exts_gced = 0;
    int new_obj_writes = 0;
    int new_obj_reads = 0;
    int striper_parities = 0;
    unordered_map<string, int> total_reclaimed_space_by_ext_type = unordered_map<string, int>();
};

// Run simulator metric
struct sim_metric {
    int gc_amplification = 0;
    int total_obsolete = 0;
    int total_used_space = 0;
    vector<double> obs_percentages = vector<double>();
    double max_obs_perc = 0;
    double gc_ratio = 0;
    int total_reclaimed_space = 0;
    int parity_reads = 0;
    int parity_writes = 0;
    int total_user_data_reads = 0;
    int total_user_data_writes = 0;
    double total_gc_bandwidth = 0;
    double total_bandwidth = 0;
    int total_absent_data_reads = 0;
    int total_obsolete_data_reads = 0;
    int total_pool_to_parity_calculator = 0;
    int total_parity_calculator_to_storage_node = 0;
    int total_storage_node_to_parity_calculator = 0;
    int num_objs = 0;
    int num_exts = 0;
    int num_stripes = 0;
    int dc_size = 0;
    int total_leftovers = 0;
    double ave_exts_gced = 0;
    unordered_map<string, int> types = unordered_map<string, int>();
    unordered_map<string, int> cost_by_ext = unordered_map<string, int>();
    unordered_map<string, int> obs_by_ext_types = unordered_map<string, int>();
    gc_ext_type_num_map gced_by_type = gc_ext_type_num_map();
};

class DataCenter {

    int max_size, simul_time, gc_cycle, gced_space;
    float striping_cycle;
    shared_ptr<AbstractStriperDecorator> striper;
    shared_ptr<StripeManager> stripe_mngr;
    shared_ptr<ExtentManager> ext_mngr;
    shared_ptr<ObjectManager> obj_mngr;
    shared_ptr<EventManager> event_mngr;
    shared_ptr<GarbageCollectionStrategy> gc_strategy;
    shared_ptr<StripingProcessCoordinator> coordinator;
    unordered_map<string, int> obs_by_ext_types;

public:
    DataCenter(int max_size, int striping_cycle,
            shared_ptr<AbstractStriperDecorator> striper,
            shared_ptr<StripeManager> stripe_mngr,
            shared_ptr<ExtentManager> ext_mngr,
            shared_ptr<ObjectManager> obj_mngr,
            shared_ptr<EventManager> event_mngr,
            shared_ptr<GarbageCollectionStrategy> gc_strategy,
            shared_ptr<StripingProcessCoordinator> coordinator,
            int simul_time, int gc_cycle)
        : max_size(max_size), striping_cycle(striping_cycle),
          striper(striper), ext_mngr(ext_mngr), obj_mngr(obj_mngr),
          event_mngr(event_mngr), gc_strategy(gc_strategy),
          coordinator(coordinator), simul_time(simul_time), gc_cycle(gc_cycle),
          gced_space(0), obs_by_ext_types(unordered_map<string, int>()) {}

    /*
     * Returns the amount of added obsolete data, a set of stripes affected by
     * the deletion of object with obj_id and a dict mapping extent types
     * to added obsoleted data.
     */
    del_result del_object(int obj_id)
    {
        del_result ret;
        auto obj = this->obj_mngr->get_object(obj_id);
        auto ext_lst = obj->extents;
        std::reverse(ext_lst.begin(), ext_lst.end());
        for (auto ex : ext_lst) {
            obj->remove_extent(ex);

            // Size of obj in extent
            int temp = ex->get_obj_size(obj_id);
            this->gced_space += temp;
            ex->del_object(obj);

            // Extent in stripe
            if (ex->stripe != nullptr) {
                if (ret.ext_types.find(ex->type) != ret.ext_types.end())
                    ret.ext_types[ex->type] += temp;
                else
                    ret.ext_types[ex->type] = temp;

                // Update stripe level obsolete data amount
                ex->stripe->update_obsolete(temp);
                ret.gc_stripes_set.emplace(ex->stripe);
                ret.total_added_obsolete += temp;
            } else if (this->coordinator->extent_in_extent_stacks(ex)) { // Sealed extent
                this->coordinator->del_sealed_extent(ex);
                this->ext_mngr->delete_extent(ex);
            } else { // Unsealed extent
                ex->obsolete_space -= temp;
                ex->free_space += temp;
            }
        }
        this->obj_mngr->remove_object(obj);
        return ret;
    }

    /*
     * Returns the metrics from the simulation
     */
    eh_result event_handler()
    {
        eh_result ret;
        configtime = 0;
        int net_obsolete = 0;
        int used_space = 0;
        double daily_max_perc = 0;
        double obs_perc = -1;
        double obs_timestamp = -1;
        int next_del_time = this->simul_time + 1;
        int next_del_obj_id = -1;
        vector<double> obs_percentages = vector<double>();
        unordered_map<string, int> net_obs_by_ext_type = unordered_map<string, int>();
        ExtentObject *next_del_obj = nullptr;

        while (configtime <= this->simul_time && ret.dc_size < this->max_size) {
            int added_obsolete_this_gc = 0;
            unordered_map<string, int> added_obsolete_by_type = unordered_map<string, int>();
            for (auto it : this->obs_by_ext_types)
                added_obsolete_by_type[it.first] = 0;
            // Find all candidates for GC
            set<Stripe*> gc_stripes_set = set<Stripe*>();
            while (next_del_time <= configtime && !this->event_mngr->empty()) {
                del_result dr = this->del_object(next_del_obj_id);
                gc_stripes_set.insert(dr.gc_stripes_set.begin(), dr.gc_stripes_set.end());
                added_obsolete_this_gc += dr.total_added_obsolete;

                // Since garbage collection has to wait for gc cycle need to
                // add how long the data sits around before the garbage
                // collection kicks in to the obsolete data metric.
                ret.total_obsolete += dr.total_added_obsolete * (configtime - next_del_time);
                for (auto it : dr.ext_types) {
                    if (added_obsolete_by_type.find(it.first) == added_obsolete_by_type.end()) {
                        added_obsolete_by_type[it.first] = it.second;
                        this->obs_by_ext_types[it.first] = it.second * (configtime - next_del_time);
                    } else {
                        added_obsolete_by_type[it.first] += it.second;
                        this->obs_by_ext_types[it.first] += it.second * (configtime - next_del_time);
                    }
                }

                if (!this->event_mngr->empty()) {
                    auto e = this->event_mngr->events->top();
                    this->event_mngr->events->pop();
                    next_del_time = std::get<0>(e);
                    next_del_obj = std::get<1>(e);
                    next_del_obj_id = next_del_obj->id;
                }
            }
            this->event_mngr->put_event(next_del_time, next_del_obj);

            auto gc_ret = this->gc_strategy->gc_handler(gc_stripes_set);
            if (!this->event_mngr->empty()) {
                auto e = this->event_mngr->events->top();
                this->event_mngr->events->pop();
                next_del_time = std::get<0>(e);
                next_del_obj = std::get<1>(e);
                next_del_obj_id = next_del_obj->id;
            }
            ret.total_reclaimed_space += gc_ret.reclaimed_space;
            ret.total_exts_gced += gc_ret.total_num_exts_replaced;
            ret.new_obj_reads += gc_ret.total_user_reads;
            ret.new_obj_writes += gc_ret.total_user_writes;

            ret.total_valid_obj_transfers += gc_ret.total_valid_obj_transfers;
            ret.total_storage_node_to_parity_calculator += gc_ret.total_storage_node_to_parity_calculator;

            ret.total_global_parity_reads += gc_ret.total_global_parity_reads;
            ret.total_global_parity_writes += gc_ret.total_global_parity_writes;
            ret.total_local_parity_reads += gc_ret.total_local_parity_reads;
            ret.total_local_parity_writes += gc_ret.total_local_parity_writes;
            ret.total_obsolete_data_reads += gc_ret.total_obsolete_data_reads;
            ret.total_absent_data_reads += gc_ret.total_absent_data_reads;

            net_obsolete += added_obsolete_this_gc - gc_ret.reclaimed_space;

            for (auto it : gc_ret.total_reclaimed_space_by_ext_type) {
                if (ret.total_reclaimed_space_by_ext_type.find(it.first) == ret.total_reclaimed_space_by_ext_type.end())
                    ret.total_reclaimed_space_by_ext_type[it.first] = it.second;
                else
                    ret.total_reclaimed_space_by_ext_type[it.first] += it.second;
            }

            for (auto it : this->obs_by_ext_types) {
                if (net_obs_by_ext_type.find(it.first) != net_obs_by_ext_type.end()
                        && ret.total_reclaimed_space_by_ext_type.find(it.first)
                        != ret.total_reclaimed_space_by_ext_type.end())
                    net_obs_by_ext_type[it.first] += added_obsolete_by_type[it.first] - ret.total_reclaimed_space_by_ext_type[it.first];
                else if (net_obs_by_ext_type.find(it.first) != net_obs_by_ext_type.end())
                    net_obs_by_ext_type[it.first] += added_obsolete_by_type[it.first];
                else if (ret.total_reclaimed_space_by_ext_type.find(it.first)
                        != ret.total_reclaimed_space_by_ext_type.end())
                    net_obs_by_ext_type[it.first] = added_obsolete_by_type[it.first] - ret.total_reclaimed_space_by_ext_type[it.first];
                else
                    net_obs_by_ext_type[it.first] = added_obsolete_by_type[it.first];
                this->obs_by_ext_types[it.first] += net_obs_by_ext_type[it.first] * this->gc_cycle;
            }

            if (next_del_obj)
                this->event_mngr->put_event(next_del_time, next_del_obj);

            auto str_result = this->coordinator->generate_stripes();
            if (!this->event_mngr->empty()) {
                auto e = this->event_mngr->events->top();
                this->event_mngr->events->pop();
                next_del_time = std::get<0>(e);
                next_del_obj = std::get<1>(e);
                next_del_obj_id = next_del_obj->id;
            }
            ret.total_used_space += used_space * this->striping_cycle;
            ret.new_obj_writes += str_result.writes;
            ret.new_obj_reads += str_result.reads;

            ret.striper_parities += (str_result.writes - str_result.reads);

            used_space = this->stripe_mngr->get_data_dc_size();

            ret.total_obsolete += net_obsolete * this->gc_cycle;
            obs_perc = -1;
            if (used_space > 0)
                obs_perc = (added_obsolete_this_gc + net_obsolete) / (double) used_space * 100;
            obs_perc = std::max(obs_perc, daily_max_perc);

            // Keep a record of the daily maximum obsolete percentage, ignore
            // the first month since the data center is too small at that point
            if (configtime > 30 && obs_perc > ret.max_obs_perc)
            ret.max_obs_perc = obs_perc;
            if (std::round(configtime) - round(obs_timestamp) >= 1) {
                obs_percentages.emplace_back(daily_max_perc);
                obs_timestamp = configtime;
                daily_max_perc = 0;
            }

            configtime += this->gc_cycle;

            ret.dc_size = this->stripe_mngr->get_total_dc_size();
        }

        cout << "Number of objects in dc: " << this->obj_mngr->get_num_objs() << endl;
        cout << "Number of extents in dc: " << this->ext_mngr->get_num_ext() << endl;
        cout << "Number of stripes in dc: " << this->stripe_mngr->get_num_stripes() << endl;
        cout << "Ave number of exts gc'ed per cycle"
                << ret.total_exts_gced / ((configtime) * 1 / this->striping_cycle) << endl;
        
        return ret;
    }

    sim_metric run_simulation()
    {
        sim_metric ret;
        eh_result eh = this->event_handler();

        ret.total_obsolete = eh.total_obsolete;
        ret.dc_size = eh.dc_size;
        ret.total_used_space = eh.total_used_space;
        ret.total_leftovers = eh.total_leftovers;
        ret.total_reclaimed_space = eh.total_reclaimed_space;
        ret.total_absent_data_reads = eh.total_absent_data_reads;
        ret.total_obsolete_data_reads = eh.total_obsolete_data_reads;
        ret.total_storage_node_to_parity_calculator = eh.total_storage_node_to_parity_calculator;

        // TODO: add storage node to parity calculator and other costs to make 
        //       the by type cost breakdowns work with all gc strategies
        ret.total_pool_to_parity_calculator = eh.total_valid_obj_transfers;
        ret.total_parity_calculator_to_storage_node = eh.total_valid_obj_transfers;
        ret.num_objs = this->obj_mngr->get_num_objs();
        ret.num_exts = this->ext_mngr->get_num_ext();
        ret.num_stripes = this->stripe_mngr->get_num_stripes();
        ret.total_user_data_reads = this->stripe_mngr->get_data_dc_size() + eh.total_reclaimed_space;

        // Coding overhead is total storage made up of (local parity + data +
        // global parity) / just data extents
        float coding_overhead = this->stripe_mngr->coding_overhead;
        ret.total_user_data_writes = ret.total_user_data_reads * coding_overhead;
        double total_user_bandwidth = ret.total_user_data_reads + ret.total_user_data_writes;
        ret.total_bandwidth = eh.total_absent_data_reads + eh.total_global_parity_reads +
                eh.total_global_parity_writes + eh.total_local_parity_reads +
                eh.total_local_parity_writes + eh.total_obsolete_data_reads +
                eh.total_storage_node_to_parity_calculator + eh.new_obj_reads + eh.new_obj_writes;

        ret.parity_reads = eh.total_global_parity_reads + eh.total_local_parity_reads;
        ret.parity_writes = (eh.total_global_parity_writes + eh.total_local_parity_writes + eh.striper_parities)
                - (ret.total_user_data_writes + ret.total_user_data_reads);
        
        ret.total_gc_bandwidth = ret.total_bandwidth - total_user_bandwidth;
        cout << "Parity reads, parity writes" << ret.parity_reads << ", " << ret.parity_writes << endl;

        if (ret.total_reclaimed_space > 0)
            ret.gc_amplification = ret.total_gc_bandwidth / ret.total_reclaimed_space;
        else
            ret.gc_amplification = 0;

        ret.gc_ratio = ret.total_gc_bandwidth / total_user_bandwidth;
        ret.gced_by_type = this->gc_strategy->get_gc_ed_exts_by_type();

        ret.types = this->coordinator->get_extent_types();
        auto user_exts_by_type = this->ext_mngr->get_ext_types();
        unordered_map<string, double> user_bandwidth_by_ext_type = unordered_map<string, double>();
        for (auto it : user_exts_by_type) {
            int user_reads, user_writes;
            user_reads = user_exts_by_type[it.first] * this->ext_mngr->ext_size;
            if (eh.total_reclaimed_space_by_ext_type.find(it.first)
                    != eh.total_reclaimed_space_by_ext_type.end())
                user_reads += eh.total_reclaimed_space_by_ext_type[it.first];
            user_writes = user_reads * coding_overhead;
            user_bandwidth_by_ext_type[it.first] = user_reads + user_writes;
        }

        unordered_map<string, double> gc_bandwidth_by_key = unordered_map<string, double>();
        unordered_map<string, double> total_bandwidth_by_key = unordered_map<string, double>();
        auto valid_objs_by_ext_type = this->gc_strategy->get_valid_objs_by_ext_type();
        for (auto it : ret.types) {
            string key = it.first;
            int val = it.second;
            if (valid_objs_by_ext_type.find(key) != valid_objs_by_ext_type.end())
                total_bandwidth_by_key[key] = (ret.types[key] * this->ext_mngr->ext_size + ret.types[key]
                        * this->ext_mngr->ext_size * coding_overhead + valid_objs_by_ext_type[key]);
            else
                total_bandwidth_by_key[key] = (ret.types[key] * this->ext_mngr->ext_size
                        + ret.types[key] * this->ext_mngr->ext_size * coding_overhead);
            gc_bandwidth_by_key[key] = total_bandwidth_by_key[key] - user_bandwidth_by_ext_type[key];
            ret.total_gc_bandwidth = gc_bandwidth_by_key[key];
            ret.total_bandwidth = total_bandwidth_by_key[key];
        }

        // TODO: Need a tostring operator for unordered_map<string, double> to
        //       print some of the metric we have currently

        int total_gc_by_type = 0;
        for (auto it : gc_bandwidth_by_key) {
            if (!it.second)
                ret.cost_by_ext[it.first] = 0;
            else
                ret.cost_by_ext[it.first] = it.second / total_user_bandwidth;
            total_gc_by_type += ret.cost_by_ext[it.first];
            cout << "Cost per ext for " << it.first << ": " << ret.cost_by_ext[it.first] << endl;
        }

        double total_obs_percent = 0;
        for (auto & it : this->obs_by_ext_types) {
            ret.total_obsolete += it.second;
            it.second = ((float) it.second / ret.total_used_space) * 100;
            cout << "Obs \% per ext for " << it.first << ": " << it.second << endl;
            total_obs_percent += it.second;
        }

        return ret;
    }
};

#endif // __DATA_CENTER_H_
