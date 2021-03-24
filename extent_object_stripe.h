#pragma once

#include <iostream>
#include <ctime>
#include <list>
#include <vector>
#include <unordered_map>
#include "extent_object_shard.h"

class Extent;
class ExtentObject;
class Stripe;

using std::string;
using std::unordered_map;
using std::list;
using std::vector;
using obj_record = std::pair<ExtentObject*, int>;
using object_lst = std::vector<obj_record>;

class ExtentObject {
    public:
        int id;
        int size;
        double life;
        int generation;
        time_t creation_time;
        int num_times_gced;
        list<Extent_Object_Shard*>* shards;
        vector<Extent*> extents;

        ExtentObject(int s, float l) 
        : size(s), life(l), generation(0), num_times_gced(0), 
          creation_time(time(nullptr)), shards(new list<Extent_Object_Shard*>()),
          extents(vector<Extent*>()) {}

        ~ExtentObject()
        {
          delete shards;
        }

		bool operator<(const ExtentObject & other)
		{
			return this->id < other.id;
		}

        double get_age()
		{
            return difftime(time(nullptr), creation_time);
        }

        void add_extent(Extent * e)
        {
            this->extents.emplace_back(e);
        }

		int get_default_key()
		{
			return 0;
		}
};

class Extent {
    public:
        double obsolete_space;
        double free_space;
        double ext_size;

        unordered_map<ExtentObject*, list<Extent_Object_Shard*>* > * objects;
        unordered_map<int, vector<int>> obj_ids_to_obj_size;
        int locality;
        int generation;
        time_t timestamp;
        string type;
        int secondary_threshold;

        int get_default_key()
        {
            return 0;
        }
        
        time_t get_timestamp()
        {
            return timestamp;
        }

        int get_generation()
        {
            return generation;
        }

        Extent(int e_s, int s_t)
        :obsolete_space(0),free_space(e_s), ext_size(e_s),
        objects(new unordered_map<ExtentObject*, list<Extent_Object_Shard*>* > () ),
        locality(0), generation(0), timestamp(0), type(0),
        secondary_threshold(s_t)
        {}

        ~Extent()
        {
          delete objects;
        }

        double get_age()
        {
            return difftime(time(nullptr), timestamp);
        }

        double get_obj_size(ExtentObject * obj)
        {
            double sum = 0;
            auto it = objects->find(obj);
            for(Extent_Object_Shard* s : *it->second){
                sum+=s->shard_size;
            }
            return sum;
        }

        double get_obsolete_percentage()
        {
            return obsolete_space/ext_size * 100;
        }

        int add_object(ExtentObject* obj, int size, int generation = 0)
        {
            int temp_size = size < free_space?size:free_space;
            int obj_id = obj->id;
            if(!timestamp)
                timestamp = obj->creation_time;
            else if(obj->creation_time < timestamp)
                timestamp = obj->creation_time;

            if(!generation)
                this->generation = obj->generation;
            else if(generation > this->generation)
                this->generation = obj->generation;

            if (this->obj_ids_to_obj_size.find(obj_id) != this->obj_ids_to_obj_size.end()) {
                this->obj_ids_to_obj_size[obj_id].emplace_back(temp_size);
            } else {
                this->obj_ids_to_obj_size[obj_id] = { temp_size };
                Extent_Object_Shard* new_shard = new Extent_Object_Shard(size);
                obj->shards->push_back(new_shard);
                this->objects->emplace(std::make_pair(obj, new list<Extent_Object_Shard*>()));
                this->objects->find(obj)->second->push_back(new_shard);
            }
            free_space -= temp_size;
            return temp_size;
        }

        void remove_objects()
        {
            delete objects;
            objects = NULL;
        }

        double del_object(ExtentObject* obj)
        {
            double sum = 0;
            auto it = objects->find(obj);
            for(Extent_Object_Shard* s : *it->second){
                sum+=s->shard_size;
            }
            obsolete_space += sum;
            if (it != objects->end())
            {
                objects->erase(it);
            }
            return obsolete_space / ext_size;
        }

        object_lst delete_ext()
        {
            object_lst ret;
            
            for (auto& it: *objects) {
                int sum = 0;
                for(Extent_Object_Shard* s: *it.second)
                {
                    sum += s->shard_size;
                    ret.push_back(std::make_pair(it.first, sum));
                }
            }
            delete objects;
            objects = new unordered_map<ExtentObject*, list<Extent_Object_Shard*>* > ();
            generation = 0;
            free_space = ext_size;
            return ret;
        }
};

class Stripe{
    public:
        double obsolete;
        int num_data_blocks;
        int num_localities;
        double free_space;
        vector<int>* localities;
        int ext_size;
        double timestamp;
        int stripe_size;
        int primary_threshold;
        list<Extent*>* extents;
        
        Stripe(int num_data_extents_per_locality, 
        int num_localities, int ext_size, int primary_threshold)
        :obsolete(0),num_data_blocks(num_data_extents_per_locality),
        num_localities(num_localities), 
        free_space(num_localities*num_data_extents_per_locality),
        localities(new vector<int>(num_localities, 0)), ext_size(ext_size), timestamp(0),
        stripe_size(0), primary_threshold(primary_threshold),extents(new list<Extent*>())
        {
            for(int i = 0; i < num_data_blocks * num_localities; ++i)
            {
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
        double update_obsolete(double obsolete)
        {
            this->obsolete += obsolete;
            return this->obsolete/stripe_size * 100;
        }

        double get_obsolete_percentage(){
            return obsolete/stripe_size*100;
        }

        int get_num_data_exts()
        {
            return num_data_blocks * num_localities;
        }

        void add_extent(Extent * ext)
        {
            if(free_space)
            {
                extents->push_back(ext);
                free_space-=1;
                int locality = 0;
                while((*localities)[locality] == num_data_blocks)
                {
                    locality += 1;
                }
                (*localities)[locality] += 1;
                ext->locality = locality;
                if(ext->timestamp > timestamp)
                {
                    this->timestamp = ext->timestamp;
                }
            }else
            {
                std::cerr << "Attempt to add extent to a full stripe";
            }
        }

        void del_extent(Extent * ext)
        {
            ext->remove_objects();
            (*localities)[ext->locality] -= 1;
            obsolete -= ext->obsolete_space;
            extents->remove(ext);
            free_space += 1;
        }


};
