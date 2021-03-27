#pragma once
#include "extent_object_stripe.h"
#include <list>
#include <queue>

using std::list;
using std::priority_queue;

// im using std tuple
using event = std::tuple<int, ExtentObject *>;

class EventManager {
public:
  priority_queue<event> *events;

  EventManager() : events(new priority_queue<event>()) {}

  void put_event(int life, ExtentObject *obj) {
    events->emplace(event(life, obj));
  }

  void put_event_in_lst(list<event> lst) {
    for (event e : lst) {
      events->emplace(e);
    }
  }
  bool empty() { return events->empty(); }
};