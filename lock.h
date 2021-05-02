#ifndef LOCK_H_
#define LOCK_H_

#pragma once
#include <memory>
#include <mutex>

using std::shared_ptr;
using std::mutex;

// This way, mutex is only used when multithreading configuration is loaded
// in order to improve performance when multithreading is not enabled
static inline void lock(shared_ptr<mutex> mtx) {
	if (mtx) mtx->lock();
}
static inline void unlock(shared_ptr<mutex> mtx) {
	if (mtx) mtx->unlock();
}

#endif // LOCK_H_
