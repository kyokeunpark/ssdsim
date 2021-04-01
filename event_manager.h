#pragma once
#include "extent_object_stripe.h"
#include <list>
#include <queue>

using std::list;
using std::priority_queue;

// im using std tuple
using event = std::tuple<float, ExtentObject *>;
using e_queue = std::priority_queue<event, std::vector<event>, std::greater<event>>;

class EventManager {
public:
  e_queue *events;

  EventManager() : events(new e_queue()) {}

  void put_event(float life, ExtentObject *obj) {
    events->emplace(event(life, obj));
  }

  void put_event_in_lst(list<event> lst) {
    for (event e : lst) {
      events->emplace(e);
    }
  }
  bool empty() { return events->empty(); }
};
