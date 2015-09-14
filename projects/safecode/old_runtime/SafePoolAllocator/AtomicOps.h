//=== AtomicOps.h --- Declare atomic operation primitives -------*- C++ -*-===//
// 
//                     The LLVM Compiler Infrastructure
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file declares synchronization primitives used in speculative checking.
//
//===----------------------------------------------------------------------===//

#ifndef _ATOMIC_OPS_H_
#define _ATOMIC_OPS_H_

#include "safecode/SAFECode.h"

#include <pthread.h>
#include <cassert>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <strings.h>
#include "Profiler.h"

NAMESPACE_SC_BEGIN

#define mb()  asm volatile ("" ::: "memory")

class LockFreeFifo
{
  static const size_t N = 65536;
public:
  typedef void (*ptr_t)(uintptr_t*);
  struct element_t {
    volatile ptr_t op;
    uintptr_t d[3];
  } __attribute__((packed));

  LockFreeFifo () {
    writeidx = 0;
    bzero(&buffer[0], sizeof(buffer));
  }

  void dispatch (void) {
    unsigned val = 0;
    while (true) {
      while (!buffer[val].op) {asm("pause");}
      buffer[val].op(&buffer[val].d[0]);
      buffer[val].op = 0;
      val = (val + 1) % N;
    }
  }

  void enqueue (const ptr_t op)
  {
    unsigned val = writeidx;
    while (buffer[val].op) {asm("pause");}
    buffer[val].op = op;
    writeidx = (val + 1) % N;
  }

  void enqueue (uintptr_t d1, const ptr_t op)
  {
    unsigned val = writeidx;
    while (buffer[val].op) {asm("pause");}
    buffer[val].d[0] = d1;
    mb();
    buffer[val].op = op;
    writeidx = (val + 1) % N;
  }

  void enqueue (uintptr_t d1, uintptr_t d2, const ptr_t op)
  {
    unsigned val = writeidx;
    while (buffer[val].op) {asm("pause");}
    buffer[val].d[0] = d1;
    buffer[val].d[1] = d2;
    mb();
    buffer[val].op = op;
    writeidx = (val + 1) % N;
  }

  void enqueue (uintptr_t d1, uintptr_t d2, uintptr_t d3, const ptr_t op)
  {
    unsigned val = writeidx;
    while (buffer[val].op) {asm("pause");}
    buffer[val].d[0] = d1;
    buffer[val].d[1] = d2;
    buffer[val].d[2] = d3;
    mb();
    buffer[val].op = op;
    writeidx = (val + 1) % N;
  }

private:
  unsigned __attribute__((aligned(128))) writeidx;
  element_t __attribute__((aligned(128))) buffer[N];
};

template <class QueueTy>
class Task {
public:
  Task(QueueTy & queue) : mQueue(queue) {}
  void activate() {
    typedef void * (*start_routine_t)(void*);
    int ret = pthread_create(&mThread, NULL, (start_routine_t)(&Task::runHelper), this);
    assert (ret == 0 && "Create checking failed!");
  }

	pthread_t thread() const {
		return mThread;
  }

  QueueTy & getQueue() const {
    return mQueue;
  }

private:
  pthread_t mThread;
  static void * runHelper(Task * this_) {
    this_->run();
    return NULL;
  }

  void run() {
    mQueue.dispatch();
  }
  
  QueueTy & mQueue;
  bool mActive;
};

NAMESPACE_SC_END

#endif
