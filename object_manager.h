#include "stripe.h"
#include "event_manager.h"
#include "sampler.h"
#include "config.h"
#include <random>

class ObjectManager{
    public:
        list<Extent_Object*>* objects;
        EventManager * event_manager;
        Sampler * sampler;
        bool add_noise;
        
        ObjectManager(EventManager * e_m, Sampler * s, bool a_n = true):
        objects(new list<Extent_Object*>()), event_manager(e_m), sampler(s), add_noise(a_n){
            //np.random.seed(0)
            srand(0);
        }

        //docstring and code doesnt match managers.py
        vector <Extent_Object *> create_new_object(int num_samples = 1){
            vector <Extent_Object *> new_objs = vector<Extent_Object *>();
            auto size_age_samples = sampler->get_size_age_sample();
            samples size_samples = size_age_samples.first;
            samples life_samples = size_age_samples.second;
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
                new_objs.push_back(obj);
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