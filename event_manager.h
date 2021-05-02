#pragma once
#include "extent_object_stripe.h"
#include <list>
#include <queue>
#include "lock.h"

using std::make_shared;
using std::list;
using std::priority_queue;

// im using std tuple
using event = std::tuple<float, obj_ptr>;
using e_queue = std::priority_queue<event, std::vector<event>, std::greater<event>>;

class EventManager {
  shared_ptr<mutex> mtx = nullptr;

public:
  e_queue *events;

  EventManager(bool is_threaded = false) : events(new e_queue()) {
    if (is_threaded)
      mtx = make_shared<mutex>();
  }

  void put_event(float life, obj_ptr obj) {
    lock(mtx);
    events->emplace(event(life, obj));
    unlock(mtx);
  }

  void put_event_in_lst(list<event> lst) {
    lock(mtx);
    for (event e : lst) {
      events->emplace(e);
    }
    unlock(mtx);
  }

  bool empty() { return events->empty(); }

  event pop_event() {
    event e(-1, nullptr);
    lock(mtx);
    if (!this->events->empty()) {
      e = this->events->top();
      this->events->pop();
    }
    unlock(mtx);
    return e;
  }
};
