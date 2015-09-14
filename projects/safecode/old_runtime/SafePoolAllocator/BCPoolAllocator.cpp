//===- BCPoolAllocator.cpp: -----------------------------------------------===//
//
//                          The SAFECode Compiler
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pool allocator registers objects into splay tree to perform memory
// access checking.
//
//===----------------------------------------------------------------------===//

#include "PoolAllocator.h"
#include <cassert>

/* #define DEBUG_OUTPUT */

class BCPoolAllocator {
public:
  typedef PoolTy PoolT;
  static void * poolalloc(PoolTy *Pool, unsigned NumBytes) {
    void * ret = __barebone_poolalloc(Pool, NumBytes);
#if DEBUG_OUTPUT
    fprintf(stderr, "Alloc Pool=%p ret=%p/%08x\n", Pool, ret, NumBytes);
    fflush(stderr);
#endif
    poolregister(Pool, ret, NumBytes);
    return ret;
  }

  static void * pool_alloca(PoolTy * Pool, unsigned int NumBytes) {
    assert (0 && "Should be deprecated\n");
    void * ret = __barebone_pool_alloca(Pool, NumBytes);
    poolregister(Pool, ret, NumBytes); 
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
  
  static void pool_init_runtime() {
    // Disable dangling pointer checkings and rewriting of out of bound
    // pointers
    ::pool_init_runtime(0, 0, 1);
  }

  static void poolfree(PoolTy *Pool, void *Node) {
    __barebone_poolfree(Pool, Node);
#if DEBUG_OUTPUT
    fprintf(stderr, "Free Pool=%p ret=%p\n", Pool, Node);
    fflush(stderr);
#endif
    poolunregister(Pool, Node);
  }
};


extern "C" {
  void __sc_bc_pool_init_runtime(unsigned Dangling,
                                 unsigned RewriteOOB,
                                 unsigned Terminate) {
    BCPoolAllocator::pool_init_runtime();
  }

  void __sc_bc_poolinit(PoolTy *Pool, unsigned NodeSize) {
#if DEBUG_OUTPUT
    fprintf(stderr, "Init Pool=%p\n", Pool);
    fflush(stderr);
#endif
    BCPoolAllocator::poolinit(Pool, NodeSize);
  }

  void __sc_bc_pooldestroy(PoolTy *Pool) {
#if DEBUG_OUTPUT
    fprintf(stderr, "Destroy Pool=%p\n", Pool);
    fflush(stderr);
#endif
    BCPoolAllocator::pooldestroy(Pool);
  }

  void * __sc_bc_poolalloc(PoolTy *Pool, unsigned NumBytes) {
    return BCPoolAllocator::poolalloc(Pool, NumBytes);
  }
  
  void __sc_bc_poolfree(PoolTy *Pool, void *Node) {
    BCPoolAllocator::poolfree(Pool, Node);
  }

  void * __sc_bc_poolrealloc(PoolTy *Pool, void *Node, unsigned NumBytes) {
    return PoolAllocatorFacade<BCPoolAllocator>::realloc(Pool, Node, NumBytes);
  }

  void * __sc_bc_poolcalloc(PoolTy *Pool, unsigned Number, unsigned NumBytes) {
    return PoolAllocatorFacade<BCPoolAllocator>::calloc(Pool, Number, NumBytes);
  }

  void * __sc_bc_poolstrdup(PoolTy *Pool, char *Node) {
    return PoolAllocatorFacade<BCPoolAllocator>::strdup(Pool, Node);
  }

}

