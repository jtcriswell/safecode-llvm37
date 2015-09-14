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

#ifndef _PA_BITMAP_RUNTIME_H_
#define _PA_BITMAP_RUNTIME_H_

#include "safecode/ADT/HashExtras.h"
#include "safecode/SAFECode.h"

#include <string>

// Use a macro for the const attribute.  This allows const to be disabled for
// debugging, allowing a programmer to change logregs during a debugging
// session.
#define CONST const

/// It should be always zero in production version 
/* Set to 1 to log object registrations */
static CONST __attribute__((unused)) unsigned logregs = 0;

NAMESPACE_SC_BEGIN

/// This structure is intended to be used by inheritance (synatatic sugar from
/// C++), but it does not have a virtual destructor for it. Therefore you should
/// never delete a BitmapPoolTy* directly!
struct BitmapPoolTy {
  static const unsigned AddrArrSize = 2;
  // Linked list of slabs used for stack allocations
  void * StackSlabs;

  // Linked list of slabs available for stack allocations
  void * FreeStackSlabs;

  // Ptr1, Ptr2 - Implementation specified data pointers.
  void *Ptr1, *Ptr2;

  // FreeablePool - Set to false if the memory from this pool cannot be freed
  // before destroy.
  //
  //  unsigned short FreeablePool;

  // Use the hash_set only if the number of Slabs exceeds AddrArrSize
  hash_set<void*> *Slabs;

  // The array containing the initial address of slabs (as long as there are
  // fewer than a certain number of them)
  void* SlabAddressArray[AddrArrSize];

  // The number of slabs allocated. Large arrays are not counted
  unsigned NumSlabs;

  // TODO: Not sure for what this value is used.
  unsigned short lastUsed;

  // NodeSize - Keep track of the object size tracked by this pool
  unsigned short NodeSize;

  // Large arrays. In SAFECode, these are currently not freed or reused. 
  // A better implementation could split them up into single slabs for reuse,
  // upon being freed.
  void *LargeArrays;
  void *FreeLargeArrays;
};

#if 0
/// Template class to implement
/// realloc, calloc, strdup based on a particular implementation 
/// of a pool allocator 
template <class AllocatorT>
class PoolAllocatorFacade {
public:
  typedef typename AllocatorT::PoolT PoolT;
  static void * realloc(PoolT * Pool, void * Node, unsigned NumBytes) {
    if (Node == 0) return AllocatorT::poolalloc(Pool, NumBytes);
    if (NumBytes == 0) {
      poolfree(Pool, Node);
      return 0;
    }

    void *New = AllocatorT::poolalloc(Pool, NumBytes);
    memcpy(New, Node, NumBytes);
    AllocatorT::poolfree(Pool, Node);
    return New;
  }

  static void * calloc(PoolT *Pool, unsigned Number, unsigned NumBytes) {
    void * New = AllocatorT::poolalloc (Pool, Number * NumBytes);
    if (New) bzero (New, Number * NumBytes);
    return New;
  } 

  static void * strdup(PoolT *Pool, char *Node) {
    if (Node == 0) return 0;

    unsigned int NumBytes = strlen(Node) + 1;
    void *New = AllocatorT::poolalloc(Pool, NumBytes);
    if (New) {
      memcpy(New, Node, NumBytes+1);
    }
    return New;
  } 
};
#endif

NAMESPACE_SC_END

/// Interfaces provided by the bitmap allocators
extern "C" {
  void __pa_bitmap_poolinit(NAMESPACE_SC::BitmapPoolTy *Pool, unsigned NodeSize);
  void __pa_bitmap_pooldestroy(NAMESPACE_SC::BitmapPoolTy *Pool);
  void * __pa_bitmap_poolalloc(NAMESPACE_SC::BitmapPoolTy *Pool, unsigned NumBytes);
  void * __pa_bitmap_poolstrdup(NAMESPACE_SC::BitmapPoolTy *Pool, void *Node);
  void __pa_bitmap_poolfree(NAMESPACE_SC::BitmapPoolTy *Pool, void *Node);
  void * __pa_bitmap_poolcheck(NAMESPACE_SC::BitmapPoolTy *Pool, void *Node);
}

#endif
