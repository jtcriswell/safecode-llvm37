//===- PoolAllocator.h - Pool allocator runtime interface file --*- C++ -*-===//
// 
//                     The LLVM Compiler Infrastructure
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file defines the interface which is implemented by the LLVM pool
// allocator runtime library.
//
//===----------------------------------------------------------------------===//

#ifndef POOLCHECK_RUNTIME_H
#define POOLCHECK_RUNTIME_H

#include "splay.h"

typedef struct PoolCheckSlab {
  void *Slab;
  PoolCheckSlab *nextSlab;
} PoolCheckSlab;

typedef struct MetaPoolTy {
  void * Pool;
  MetaPoolTy *next;
} MetaPoolTy;


#ifdef __cplusplus
extern "C" {
#endif
  //register that starting from allocaptr numbytes are a part of the pool
  void poolcheck(MetaPoolTy **Pool, void *Node);
  bool poolcheckoptim(void *Pool, void *Node);
  void poolcheckregister(Splay *splay, void * allocaptr, unsigned NumBytes);
  void AddPoolDescToMetaPool(MetaPoolTy **MetaPool, void *PoolDesc);
  void poolcheckarray(MetaPoolTy **Pool, void *Node, void * Node1);
  bool poolcheckarrayoptim(MetaPoolTy *Pool, void *Node, void * Node1);
  void poolcheckAddSlab(PoolCheckSlab **PoolCheckSlabPtr, void *Slab);
  void poolcheckinit(void *Pool, unsigned NodeSize);
  void poolcheckdestroy(void *Pool);
  void poolcheckfree(void *Pool, void *Node);

  // Functions that need to be provided by the pool allocation run-time
  PoolCheckSlab *poolcheckslab(void *Pool);
  Splay *poolchecksplay(void *Pool);
  void poolcheckfail (const char * msg);
  void * poolcheckmalloc (unsigned int size);
#ifdef __cplusplus
}
#endif

#endif
