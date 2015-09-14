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

#ifndef POOLALLOCATOR_RUNTIME_H
#define POOLALLOCATOR_RUNTIME_H

#include "poolalloc/ADT/HashExtras.h"
#include "PoolCheck.h"
#include "splay.h"

#define AddrArrSize 2
#define POOLCHECK(x) x

typedef struct PoolTy {
  // Ptr1, Ptr2 - Implementation specified data pointers.
  void *Ptr1, *Ptr2;

  // NodeSize - Keep track of the object size tracked by this pool
  unsigned short NodeSize;

  // FreeablePool - Set to false if the memory from this pool cannot be freed
  // before destroy.
  //
  //  unsigned short FreeablePool;

  // Use the hash_set only if the number of Slabs exceeds AddrArrSize
  hash_set<void*> *Slabs;

  // The array containing the initial address of slabs (as long as there are
  // fewer than a certain number of them)
  unsigned SlabAddressArray[AddrArrSize];

  // The number of slabs allocated. Large arrays are not counted
  unsigned NumSlabs;

  // Large arrays. In SAFECode, these are currently not freed or reused. 
  // A better implementation could split them up into single slabs for reuse,
  // upon being freed.
  void *LargeArrays;

  void *prevPage[4];
  unsigned short lastUsed;

  POOLCHECK(Splay *splay;)
  POOLCHECK(PoolCheckSlab *PCS;)

} PoolTy;



extern "C" {
  void poolinit(PoolTy *Pool, unsigned NodeSize);
  void poolmakeunfreeable(PoolTy *Pool);
  void pooldestroy(PoolTy *Pool);
  void *poolalloc(PoolTy *Pool, unsigned NumBytes);
  void poolfree(PoolTy *Pool, void *Node);

  void *poolrealloc(PoolTy *Pool, void *Node, unsigned NumBytes);
  void *poolallocatorcheck(PoolTy *Pool, void *Node);


  //Extra functions for poolcheck
  void poolregister(PoolTy *Pool, void *allocadptr, unsigned NumBytes);
}

#endif
