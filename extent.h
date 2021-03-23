#pragma once
#include <vector>
#include <unordered_map>
#include "extent_object.h"
using obj_record = std::pair<ExtentObject*, int>;
using object_lst = std::vector<obj_record>;
using namespace std;
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
                double sum = 0;
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
