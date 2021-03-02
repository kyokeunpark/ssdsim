#include <iostream>
#include <ctime>
#include <list>
#include "extent_object_shard.h"
using namespace std;

class Extent_Object {
    public:
        int id;
        int size;
        double life;
        int generation;
        time_t creation_time;
        int num_times_gced;
        list<Extent_Object_Shard*>* shards;
        Extent_Object(int s, float l) 
        : size(s), life(l), generation(0), num_times_gced(0), 
          creation_time(time(nullptr)), shards(new list<Extent_Object_Shard*>()){
        }

        double get_age(){
            return difftime(time(nullptr), creation_time);
        }

        ~Extent_Object()
        {
          delete shards;
        }
};