/// This allocator is used for parallel checking, which puts
/// the execution of poolreigster / poolunregister into the checking 
/// thread

#ifndef _PAR_POOL_ALLOCATOR_H_
#define _PAR_POOL_ALLOCATOR_H_

#include "PoolAllocator.h"
#include <cassert>
/* #define DEBUG_OUTPUT */

extern "C" {
  void __sc_par_poolregister(PoolTy *Pool, void *allocaptr, unsigned NumBytes);
  void __sc_par_poolunregister(PoolTy *Pool, void *allocaptr);
  void __sc_par_pool_init_runtime (unsigned Dangling,
                                   unsigned RewriteOOB,
                                   unsigned Terminate);
  void __sc_par_poolinit(PoolTy *Pool, unsigned NodeSize);
  void * __sc_par_poolalloc(PoolTy *Pool, unsigned NumBytes);
  void __sc_par_poolfree(PoolTy *Pool, void *Node);
  void * __sc_par_poolrealloc(PoolTy *Pool, void *Node, unsigned NumBytes);
  void * __sc_par_poolcalloc(PoolTy *Pool, unsigned Number, unsigned NumBytes); 
  void * __sc_par_poolstrdup(PoolTy *Pool, char *Node);
}

class ParPoolAllocator {
public:
  typedef PoolTy PoolT;
  static void * poolalloc(PoolTy *Pool, unsigned NumBytes) {
    void * ret = __barebone_poolalloc(Pool, NumBytes);
#if DEBUG_OUTPUT
    fprintf(stderr, "Alloc Pool=%p ret=%p/%08x\n", Pool, ret, NumBytes);
    fflush(stderr);
#endif
    __sc_par_poolregister(Pool, ret, NumBytes);
    return ret;
  }

  static void * pool_alloca(PoolTy * Pool, unsigned int NumBytes) {
    assert (0 && "Should be deprecated\n");
    void * ret = __barebone_pool_alloca(Pool, NumBytes);
    __sc_par_poolregister(Pool, ret, NumBytes); 
    return ret;
  }

  static void poolinit(PoolTy *Pool, unsigned NodeSize) {
    __barebone_poolinit(Pool, NodeSize);
  }

  static void pooldestroy(PoolTy *Pool) {
    __barebone_pooldestroy(Pool);
#if DEBUG_OUTPUT
    fprintf(stderr, "Destr Pool=%p\n", Pool);
    fflush(stderr);
#endif
    Pool->Objects.clear();
  }
  
  static void pool_init_runtime(unsigned Dangling,
                                unsigned RewriteOOB,
                                unsigned Terminate) {
    ::pool_init_runtime(Dangling, RewriteOOB, Terminate);
  }

  static void poolfree(PoolTy *Pool, void *Node) {
    __barebone_poolfree(Pool, Node);
#if DEBUG_OUTPUT
    fprintf(stderr, "Free Pool=%p ret=%p\n", Pool, Node);
    fflush(stderr);
#endif
    __sc_par_poolunregister(Pool, Node);
  }
};


#endif

