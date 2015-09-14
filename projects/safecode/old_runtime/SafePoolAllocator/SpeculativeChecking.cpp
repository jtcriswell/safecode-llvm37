//===- SpeculativeChecking.cpp - Implementation of Speculative Checking --*- C++ -*-===//
// 
//                          The SAFECode Compiler
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file implements the asynchronous checking interfaces, enqueues checking
// requests and provides a synchronization token for each checking request.
//
//===----------------------------------------------------------------------===//

#include "safecode/SAFECode.h"

#include "SafeCodeRuntime.h"
#include "PoolAllocator.h"
#include "AtomicOps.h"
#include "Profiler.h"
#include "ParPoolAllocator.h"
#include <iostream>

NAMESPACE_SC_BEGIN

static unsigned int gDataStart;

// A flag to indicate that the checking thread has done its work
static volatile unsigned int __attribute__((aligned(128))) gCheckingThreadWorking = 0;

typedef LockFreeFifo CheckQueueTy;
CheckQueueTy gCheckQueue;

static unsigned int gDataEnd;

static void __stub_poolcheck(uintptr_t* req) {
  poolcheck((PoolTy*)req[0], (void*)req[1]);
}

static void __stub_poolcheckui(uintptr_t* req) {
  poolcheckui((PoolTy*)req[0], (void*)req[1]);
}

static void __stub_poolcheckalign(uintptr_t* req) {
  poolcheckalign((PoolTy*)req[0], (void*)req[1], (unsigned)req[2]);
}

static void __stub_boundscheck(uintptr_t* req) {
  boundscheck((PoolTy*)req[0], (void*)req[1], (void*)req[2]);
}

static void __stub_boundscheckui(uintptr_t* req) {
  boundscheckui((PoolTy*)req[0], (void*)req[1], (void*)req[2]);
}

static void __stub_poolargvregister(uintptr_t* req) {
  poolargvregister((int)req[0], (char**)req[1]);
}

static void __stub_poolregister(uintptr_t* req) {
  poolregister((PoolTy*)req[0], (void*)req[1], req[2]);
}

static void __stub_poolunregister(uintptr_t* req) {
  poolunregister((PoolTy*)req[0], (void*)req[1]);
}

static void __stub_pooldestroy(uintptr_t* req) {
  ParPoolAllocator::pooldestroy((PoolTy*)req[0]);
}

static void __stub_sync(uintptr_t* ) {
  gCheckingThreadWorking = false;
}

static void __stub_stop(uintptr_t*) {
  pthread_exit(NULL);
}

//Checking thread cached versions

PoolTy* PoolCache[2];

static void __stub_cachepool_0(uintptr_t* req) {
  PoolCache[0] = (PoolTy*)req[0];
}
static void __stub_cachepool_1(uintptr_t* req) {
  PoolCache[1] = (PoolTy*)req[0];
}
static void __stub_poolcheck_0(uintptr_t* req) {
  poolcheck(PoolCache[0], (void*)req[0]);
}
static void __stub_poolcheck_1(uintptr_t* req) {
  poolcheck(PoolCache[1], (void*)req[0]);
}
static void __stub_boundscheck_0(uintptr_t* req) {
  boundscheck(PoolCache[0], (void*)req[0], (void*)req[1]);
}
static void __stub_boundscheck_1(uintptr_t* req) {
  boundscheck(PoolCache[1], (void*)req[0], (void*)req[1]);
}

static void __stub_code_dup_arg(uintptr_t* req) {
  typedef void (*dup_arg0_t)(void*);
  ((dup_arg0_t)req[0])((void*)req[1]);
}

// Function to test queue performance
static void __stub_no_op(uintptr_t*) {
}
extern "C" {

void __sc_par_enqueue_1() {
  gCheckQueue.enqueue (NULL, __stub_no_op);
}

void __sc_par_enqueue_2() {
  gCheckQueue.enqueue (NULL, NULL, __stub_no_op);
}

void __sc_par_enqueue_3() {
  gCheckQueue.enqueue (NULL, NULL, NULL, __stub_no_op);
}

}

extern "C" {
void __sc_par_wait_for_completion();
}

namespace {
  class SpeculativeCheckingGuard {
  public:
    SpeculativeCheckingGuard() : mCheckTask(gCheckQueue) {
    }
    ~SpeculativeCheckingGuard() {
        gCheckQueue.enqueue(__stub_stop);
        pthread_join(mCheckTask.thread(), NULL);
    }

    void activate(void) {
      mCheckTask.activate();
    }

  private:
    Task<CheckQueueTy> mCheckTask;
  };
}

NAMESPACE_SC_END

using namespace NAMESPACE_SC;

extern "C" {
  void __sc_par_poolcheck(PoolTy *Pool, void *Node) {
    gCheckQueue.enqueue((uintptr_t)Pool, (uintptr_t)Node, __stub_poolcheck);
  }
  void __sc_par_poolcheck_0(void *Node) {
    gCheckQueue.enqueue((uintptr_t)Node, __stub_poolcheck_0);
  }
  void __sc_par_poolcheck_1(void *Node) {
    gCheckQueue.enqueue((uintptr_t)Node, __stub_poolcheck_1);
  }
  void __sc_par_poolcheckui(PoolTy *Pool, void *Node) {
    gCheckQueue.enqueue((uintptr_t)Pool, (uintptr_t)Node, __stub_poolcheckui);
  }
  
void
__sc_par_poolcheckalign (PoolTy *Pool, void *Node, unsigned Offset) {
	gCheckQueue.enqueue((uintptr_t)Pool, (uintptr_t)Node, (uintptr_t)Offset, __stub_poolcheckalign);
}

void __sc_par_boundscheck(PoolTy * Pool, void * Source, void * Dest) {
  gCheckQueue.enqueue ((uintptr_t)Pool, (uintptr_t)Source, (uintptr_t)Dest, __stub_boundscheck);
}
void __sc_par_boundscheck_0(void * Source, void * Dest) {
  gCheckQueue.enqueue ((uintptr_t)Source, (uintptr_t)Dest, __stub_boundscheck_0);
}

void __sc_par_boundscheck_1(void * Source, void * Dest) {
  gCheckQueue.enqueue ((uintptr_t)Source, (uintptr_t)Dest, __stub_boundscheck_1);
}

void __sc_par_boundscheckui(PoolTy * Pool, void * Source, void * Dest) {
  gCheckQueue.enqueue((uintptr_t)Pool, (uintptr_t)Source, (uintptr_t)Dest, __stub_boundscheckui);
}

void __sc_par_poolargvregister(int argc, char ** argv) {
  gCheckQueue.enqueue((uintptr_t)argc, (uintptr_t)argv, __stub_poolargvregister);
}

void __sc_par_poolregister(PoolTy *Pool, void *allocaptr, unsigned NumBytes){
  gCheckQueue.enqueue((uintptr_t)Pool, (uintptr_t)allocaptr, NumBytes, __stub_poolregister);
}

void __sc_par_poolunregister(PoolTy *Pool, void *allocaptr) {
  gCheckQueue.enqueue((uintptr_t)Pool, (uintptr_t)allocaptr, __stub_poolunregister);
}

void __sc_par_pooldestroy(PoolTy *Pool) {
  gCheckQueue.enqueue((uintptr_t)Pool, __stub_pooldestroy);
}

 void __sc_par_cachepool_0(PoolTy* Pool) {
   gCheckQueue.enqueue((uintptr_t)Pool, __stub_cachepool_0);
 }
 void __sc_par_cachepool_1(PoolTy* Pool) {
   gCheckQueue.enqueue((uintptr_t)Pool, __stub_cachepool_1);
 }
 
  void __sc_par_enqueue_code_dup(void * code, void * args) {
    gCheckQueue.enqueue((uintptr_t)code, (uintptr_t)args, __stub_code_dup_arg);
 }

void __sc_par_wait_for_completion() {
  PROFILING(
  unsigned int size = gCheckQueue.size();
  unsigned long long start_sync_time = rdtsc();
  )

  gCheckingThreadWorking = true;
  
  gCheckQueue.enqueue(__stub_sync);

  while (gCheckingThreadWorking) { asm("pause"); }

  PROFILING(
  unsigned long long end_sync_time = rdtsc();
  profile_sync_point(start_sync_time, end_sync_time, size);
  )
}

void __sc_par_store_check(void * ptr) {
  if (&gDataStart <= ptr && ptr <= &gDataEnd) {
    __builtin_trap();
  }
} 
  
void __sc_par_pool_init_runtime(unsigned Dangling,
                                unsigned RewriteOOB,
                                unsigned Terminate) {
  ParPoolAllocator::pool_init_runtime(Dangling, RewriteOOB, Terminate);
  static SpeculativeCheckingGuard g;
  g.activate();
}

}
