#pragma once
#include <queue>
#include <list>
#include "extent_object_stripe.h"

using std::priority_queue;
using std::list;

//im using std tuple
typedef std::tuple<int,ExtentObject *> event;
class EventManager{
    public:
        priority_queue<event>* events;

        EventManager():events(new priority_queue<event>()){
        }


        void put_event(int life, ExtentObject * obj){
            events->emplace(event(life, obj));
        }

        void put_event_in_lst(list<event> lst){
            for(event e: lst)
            {
                events->emplace(e);
            }
        }
        bool empty(){
            return events->empty();
        }

};