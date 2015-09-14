//===- BareBonePoolAllocator.cpp: -----------------------------------------===//
//
//                          The SAFECode Compiler
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Barebone allcator is a simple wrapper around the pool allocation APIs,
// which is equivalent of pool allocation.
//
//===----------------------------------------------------------------------===//

#include "PoolAllocator.h"
#include <cassert>

namespace llvm {

class BarebonePoolAllocator {
public:
  typedef PoolTy PoolT;
  static void * poolalloc(PoolTy *Pool, unsigned NumBytes) {
    void * ret = __barebone_poolalloc(Pool, NumBytes);
    return ret;
  }

  static void * pool_alloca(PoolTy * Pool, unsigned int NumBytes) {
    assert (0 && "Should be deprecated\n");
    void * ret = __barebone_pool_alloca(Pool, NumBytes);
    return ret;
  }

  static void poolinit(PoolTy *Pool, unsigned NodeSize) {
    __barebone_poolinit(Pool, NodeSize);
  }

  static void pooldestroy(PoolTy *Pool) {
    __barebone_pooldestroy(Pool);
  }
  
  static void pool_init_runtime() {
    ::pool_init_runtime(0, 0, 1);
  }

  static void poolfree(PoolTy *Pool, void *Node) {
    __barebone_poolfree(Pool, Node);
  }
};
}

using namespace llvm;

extern "C" {
  void __sc_barebone_pool_init_runtime(unsigned, unsigned, unsigned) {
    BarebonePoolAllocator::pool_init_runtime();
  }

  void __sc_barebone_poolinit(PoolTy *Pool, unsigned NodeSize) {
    BarebonePoolAllocator::poolinit(Pool, NodeSize);
  }

  void __sc_barebone_pooldestroy(PoolTy *Pool) {
    BarebonePoolAllocator::pooldestroy(Pool);
  }

  void * __sc_barebone_poolalloc(PoolTy *Pool, unsigned NumBytes) {
    return BarebonePoolAllocator::poolalloc(Pool, NumBytes);
  }
  
  void __sc_barebone_poolfree(PoolTy *Pool, void *Node) {
    BarebonePoolAllocator::poolfree(Pool, Node);
  }

  void * __sc_barebone_poolrealloc(PoolTy *Pool, void *Node, unsigned NumBytes) {
    return PoolAllocatorFacade<BarebonePoolAllocator>::realloc(Pool, Node, NumBytes);
  }

  void * __sc_barebone_poolcalloc(PoolTy *Pool, unsigned Number, unsigned NumBytes) {
    return PoolAllocatorFacade<BarebonePoolAllocator>::calloc(Pool, Number, NumBytes);
  }

  void * __sc_barebone_poolstrdup(PoolTy *Pool, char *Node) {
    return PoolAllocatorFacade<BarebonePoolAllocator>::strdup(Pool, Node);
  }

  void __sc_no_op_poolcheck(PoolTy *, void *) {}
  void __sc_no_op_poolcheckalign (PoolTy *, void *, unsigned Offset) {}
  void * __sc_no_op_boundscheck (PoolTy * , void * , void * dest) { return dest; }
  void __sc_no_op_poolregister(PoolTy *, void *, unsigned) {}
  void __sc_no_op_poolunregister(PoolTy *, void*) {}
  void * __sc_no_op_exactcheck(int, int, void * dest) { return dest; }
  void * __sc_no_op_exactcheck2 (signed char *, signed char * dest, unsigned ) { return dest; }
}

