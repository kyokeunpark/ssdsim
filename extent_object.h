#pragma once
#include <iostream>
#include <ctime>
#include <list>
#include "extent_object_shard.h"
using namespace std;

class ExtentObject {
    public:
        int id;
        int size;
        double life;
        int generation;
        time_t creation_time;
        int num_times_gced;
        list<Extent_Object_Shard*>* shards;

        ExtentObject(int s, float l) 
        : size(s), life(l), generation(0), num_times_gced(0), 
          creation_time(time(nullptr)), shards(new list<Extent_Object_Shard*>()){}

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

		int get_default_key()
		{
			return 0;
		}
};
