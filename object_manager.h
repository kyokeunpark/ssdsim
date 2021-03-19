#pragma once
#include "stripe.h"
#include "event_manager.h"
#include "samplers.h"
#include "config.h"
#include <random>

using obj_record = std::pair<Extent_Object*, int>;
using object_lst = std::vector<obj_record>;

class ObjectManager{
    public:
        list<Extent_Object*>* objects;
        shared_ptr<EventManager> event_manager;
        shared_ptr<Sampler> sampler;
        bool add_noise;
        
        ObjectManager(shared_ptr<EventManager> e_m, shared_ptr<Sampler> s, bool a_n = true):
        objects(new list<Extent_Object*>()), event_manager(e_m), sampler(s), add_noise(a_n){
            //np.random.seed(0)
            srand(0);
        }

        //docstring and code doesnt match managers.py
        object_lst create_new_object(int num_samples = 1){
            object_lst new_objs = object_lst();
            auto size_age_samples = sampler->get_size_age_sample();
            sizes size_samples = size_age_samples.first;
            lives life_samples = size_age_samples.second;
            for(int i = 0; i < size_samples.size(); i++)
            {
                int size = size_samples[i];
                int life = life_samples[i];
                int noise = rand() % 25;
                if(add_noise)
                {
                    noise -= 12;
                    life += noise/24;
                }
                life += TIME;
                Extent_Object * obj = new Extent_Object(size, life);
                new_objs.emplace_back(std::make_pair(obj, size));
                this->objects->push_back(obj);
                event_manager->put_event(life, obj);
            }
            return new_objs;
        }
        
        int get_num_objs()
        {
            return objects->size();
        }

        void remove_object(Extent_Object * obj)
        {
            objects->remove(obj);
        }
};
