//===- PoolAllocatorBitMask.cpp - Implementation of poolallocator runtime -===//
// 
//                          The SAFECode Compiler
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file is one possible implementation of the LLVM pool allocator runtime
// library.
//
// This uses the 'Ptr1' field to maintain a linked list of slabs that are either
// empty or are partially allocated from.  The 'Ptr2' field of the BitmapPoolTy is
// used to track a linked list of slabs which are full, ie, all elements have
// been allocated from them.
//
//===----------------------------------------------------------------------===//

#include "safecode/Runtime/BitmapAllocator.h"
#include "safecode/Runtime/PageManager.h"
#include "PoolSlab.h"

#include <cassert>
#include <cstdio>
#include <cstring>

#define DEBUG(x)

using namespace NAMESPACE_SC;

/// Helper functions

static void *
poolallocarray(BitmapPoolTy* Pool, unsigned Size);

static PoolSlab *
SearchForContainingSlab(BitmapPoolTy *Pool, void *Node, unsigned &TheIndex);

//
// Function: poolinit()
//
// Description:
//  Initialize the specified pool descriptor.  Pool descriptors are either
//  global variables or alloca'ed memory created by instrumentation added by
//  the SAFECode passes.  This function initializes all of the fields of the
//  pool descriptor.
//
void
__pa_bitmap_poolinit(BitmapPoolTy *Pool, unsigned NodeSize) {
  assert(Pool && "Null pool pointer passed into poolinit!\n");
  DEBUG(printf("pool init %x, %d\n", Pool, NodeSize);)

  // Ensure the page manager is initialized
  InitializePageManager();

  // We must alway return unique pointers, even if they asked for 0 bytes
  Pool->NodeSize = NodeSize ? NodeSize : 1;
  // Initialize the splay tree
  Pool->Ptr1 = Pool->Ptr2 = 0;
  Pool->LargeArrays = 0;
  Pool->StackSlabs = Pool->FreeStackSlabs = 0;
  // For SAFECode, we set FreeablePool to 0 always
  //  Pool->FreeablePool = 0;
  Pool->lastUsed = 0;
  // Initialize the SlabAddressArray to zero
  for (unsigned i = 0; i < BitmapPoolTy::AddrArrSize; ++i) {
    Pool->SlabAddressArray[i] = 0;
  }
  Pool->NumSlabs = 0;
}

// pooldestroy - Release all memory allocated for a pool
//
// FIXME: determine how to adjust debug logs when 
//        pooldestroy is called
void
__pa_bitmap_pooldestroy(BitmapPoolTy *Pool) {
  assert(Pool && "Null pool pointer passed in to pooldestroy!\n");

  if (Pool->NumSlabs > BitmapPoolTy::AddrArrSize) {
    Pool->Slabs->clear();
    delete Pool->Slabs;
  }

  // Free any partially allocated slabs
  PoolSlab *PS = (PoolSlab*)Pool->Ptr1;
  while (PS) {
    PoolSlab *Next = PS->Next;
    PS->destroy();
    PS = Next;
  }

  // Free the completely allocated slabs
  PS = (PoolSlab*)Pool->Ptr2;
  while (PS) {
    PoolSlab *Next = PS->Next;
    PS->destroy();
    PS = Next;
  }

  // Free the large arrays
  PS = (PoolSlab*)Pool->LargeArrays;
  while (PS) {
    PoolSlab *Next = PS->Next;
    PS->destroy();
    PS = Next;
  }

}

//
// Function: poolalloc()
//
// Description:
//  Allocate memory from the specified pool with the specified size.
//
// Inputs:
//  Pool - The pool from which to allocate the memory.
//  Size - The size, in bytes, of the memory object to allocate.  This does
//         *not* need to match the size of the objects found in the pool.
//
void *
__pa_bitmap_poolalloc(BitmapPoolTy *Pool, unsigned NumBytes) {
  void *retAddress = NULL;
  assert(Pool && "Null pool pointer passed into poolalloc!\n");

  // FIXME: Is it necessary?
  // Ensure that we're always allocating at least 1 byte.
  if (NumBytes == 0)
    NumBytes = 1;

  //
  // Calculate the number of nodes within the pool to allocate for an object
  // of the specified size.
  //
  unsigned NodeSize = Pool->NodeSize;
  assert (NodeSize && "__pa_bitmap_poolalloc: Node Size is zero!\n");
  unsigned NodesToAllocate = (NumBytes + NodeSize - 1) / NodeSize;
  
  // Call a helper function if we need to allocate more than 1 node.
  if (NodesToAllocate > 1) {
    if (logregs) {
      fprintf(stderr, " poolalloc:848: Allocating more than 1 node for %d bytes\n", NumBytes); fflush(stderr);
    }

    //
    // Allocate the memory.
    //
    retAddress = poolallocarray(Pool, NodesToAllocate);
      
    assert (retAddress && "poolalloc(1): Returning NULL!\n");
    return retAddress;
  }

  // Special case the most common situation, where a single node is being
  // allocated.
  PoolSlab *PS = (PoolSlab*)Pool->Ptr1;

  if (__builtin_expect(PS != 0, 1)) {
    int Element = PS->allocateSingle();
    if (__builtin_expect(Element != -1, 1)) {
      // We allocated an element.  Check to see if this slab has been
      // completely filled up.  If so, move it to the Ptr2 list.
      if (__builtin_expect(PS->isFull(), false)) {
        PS->unlinkFromList();
        PS->addToList((PoolSlab**)&Pool->Ptr2);
      }     
      return PS->getElementAddress(Element, NodeSize);
    }

    // Loop through all of the slabs looking for one with an opening
    for (PS = PS->Next; PS; PS = PS->Next) {
      int Element = PS->allocateSingle();
      if (Element != -1) {
        // We allocated an element.  Check to see if this slab has been
        // completely filled up.  If so, move it to the Ptr2 list.
        if (PS->isFull()) {
          PS->unlinkFromList();
          PS->addToList((PoolSlab**)&Pool->Ptr2);
        }
        return PS->getElementAddress(Element, NodeSize);
      }
    }
  }

  // Otherwise we must allocate a new slab and add it to the list
  PoolSlab *New = PoolSlab::create(Pool);

  //
  // Ensure that we're always allocating at least 1 byte.
  //
  if (NumBytes == 0)
    NumBytes = 1;
  
  if (Pool->NumSlabs > BitmapPoolTy::AddrArrSize)
    Pool->Slabs->insert((void *)New);
  else if (Pool->NumSlabs == BitmapPoolTy::AddrArrSize) {
    // Create the hash_set
    Pool->Slabs = new hash_set<void *>;
    Pool->Slabs->insert((void *)New);
    for (unsigned i = 0; i < BitmapPoolTy::AddrArrSize; ++i)
      Pool->Slabs->insert((void *)Pool->SlabAddressArray[i]);
  }
  else {
    // Insert it in the array
    Pool->SlabAddressArray[Pool->NumSlabs] = (void*) New;
  }
  Pool->NumSlabs++;

  int Idx = New->allocateSingle();
  assert(Idx == 0 && "New allocation didn't return zero'th node?");
  if (logregs) {
    fprintf(stderr, " poolalloc:967: canonical page at 0x%p from underlying allocator\n", (void*)New);
  }
  return New->getElementAddress(0, 0);
}

//
// Function: poolstrdup()
//
// Description:
//  Duplicate a string by allocating memory for a new string and copying the
//  contents of the old string into the new string.
//
// Inputs:
//  Pool - The pool in which the new string should reside.
//  Node - The string which should be duplicated.
//
// Return value:
//  0 - The duplication failed.
//  Otherwise, a pointer to the duplicated string is returned.
//
void *
__pa_bitmap_poolstrdup (BitmapPoolTy *Pool, void *Node) {
  if (Node == 0) return 0;

  unsigned int NumBytes = strlen((char*)Node) + 1;
  void *New = __pa_bitmap_poolalloc (Pool, NumBytes);
  if (New) {
    memcpy(New, Node, NumBytes+1);
  }
  return New;
}

/////
///
/// Helper functions
/// 
/////

// Function: poolallocarray()
//
// Description:
//  This is a helper function used to implement poolalloc() when the number of
//  nodes to allocate is not 1.
//
// Inputs:
//  Pool - A pointer to the pool from which to allocate.
//  Size - The number of nodes to allocate.
//
// FIXME: determine whether Size is bytes or number of nodes.
//

static void *
poolallocarray(BitmapPoolTy* Pool, unsigned Size) {
  assert(Pool && "Null pool pointer passed into poolallocarray!\n");
  
  // check to see if we need to allocate a single large array
  if (Size > PoolSlab::getSlabSize(Pool)) {
    if (logregs) {
      fprintf(stderr, " poolallocarray:694: Size = %d, SlabSize = %d\n", Size, PoolSlab::getSlabSize(Pool));
      fflush(stderr);
    }
    return PoolSlab::createSingleArray(Pool, Size);
  }
 
  PoolSlab *PS = (PoolSlab*)Pool->Ptr1;

  // Loop through all of the slabs looking for one with an opening
  for (; PS; PS = PS->Next) {
    int Element = PS->allocateMultiple(Size);
    if (Element != -1) {
      //
      // We allocated an element.  Check to see if this slab has been
      // completely filled up.  If so, move it to the Ptr2 list.
      //
      if (PS->isFull()) {
        PS->unlinkFromList();
        PS->addToList((PoolSlab**)&Pool->Ptr2);
      }
      
      return PS->getElementAddress(Element, Pool->NodeSize);
    }
  }
  
  PoolSlab *New = PoolSlab::create(Pool);
  //  printf("new slab created %x \n", New);
  if (Pool->NumSlabs > BitmapPoolTy::AddrArrSize)
    Pool->Slabs->insert((void *)New);
  else if (Pool->NumSlabs == BitmapPoolTy::AddrArrSize) {
    // Create the hash_set
    Pool->Slabs = new hash_set<void *>;
    Pool->Slabs->insert((void *)New);
    for (unsigned i = 0; i < BitmapPoolTy::AddrArrSize; ++i)
      Pool->Slabs->insert((void *)Pool->SlabAddressArray[i]);
  }
  else {
    // Insert it in the array
    Pool->SlabAddressArray[Pool->NumSlabs] = New;
  }
  
  Pool->NumSlabs++;
  
  int Idx = New->allocateMultiple(Size);
  assert(Idx == 0 && "New allocation didn't return zero'th node?");
  
  return New->getElementAddress(0, 0);
}

//
// Function: poolfree()
//
// Description:
//  Mark the object specified by the given pointer as free and available for
//  allocation for new objects.
//
// Inputs:
//  Pool - The pool to which the pointer should belong.
//  Node - A pointer to the beginning of the object to free.  This pointer is
//         allowed to be NULL.
//
// Notes:
//  This routine should be resistent to several types of deallocation errors:
//    o) Deallocating an object which does not exist within the pool.
//    o) Deallocating an already-free object.
//
void
__pa_bitmap_poolfree(BitmapPoolTy *Pool, void *Node) {
  assert(Pool && "Null pool pointer passed in to poolfree!\n");
  PoolSlab *PS;
  int Idx;
  
  if (logregs) {
    fprintf(stderr, "poolfree: 1368: Pool=%p, addr=%p\n", (void*) Pool, Node);
    fflush (stderr);
  }

  //
  // If the pointer is NULL, that is okay.  Just do nothing.
  //
  if (Node == 0) return;

  // Canonical pointer for the pointer we're freeing
  void * CanonNode = Node;

  unsigned TheIndex;
  PS = SearchForContainingSlab(Pool, CanonNode, TheIndex);
  Idx = TheIndex;

  //
  // If no slab can be found, then the pointer we were given is invalid.  Since
  // we want to tolerate invalid frees, go ahead and return.
  //
  if (!PS) return;
  assert (PS && "poolfree: No poolslab found for object!\n");
  PS->freeElement(Idx);

  //
  // If we could not find the slab in which the node belongs, then we were
  // passed an invalid pointer.  Simply ignore it.
  //
  if (!PS) return;
  
  // If PS was full, it must have been in list #2.  Unlink it and move it to
  // list #1.
  if (PS->isFull()) {
    // Now that we found the node, we are about to free an element from it.
    // This will make the slab no longer completely full, so we must move it to
    // the other list!
    PS->unlinkFromList(); // Remove it from the Ptr2 list.

    //
    // Do not re-use single array slabs.
    //

    if (!(PS->isSingleArray)) {
      PoolSlab **InsertPosPtr = (PoolSlab**)&Pool->Ptr1;

      // If the partially full list has an empty node sitting at the front of
      // the list, insert right after it.
      if ((*InsertPosPtr))
        if ((*InsertPosPtr)->isEmpty())
          InsertPosPtr = &(*InsertPosPtr)->Next;

      PS->addToList(InsertPosPtr);     // Insert it now in the Ptr1 list.
    }
  }

  // Ok, if this slab is empty, we unlink it from the of slabs and either move
  // it to the head of the list, or free it, depending on whether or not there
  // is already an empty slab at the head of the list.
  if ((PS->isEmpty()) && (!(PS->isSingleArray))) {
    PS->unlinkFromList();   // Unlink from the list of slabs...
    
    // If we can free this pool, check to see if there are any empty slabs at
    // the start of this list.  If so, delete the FirstSlab!
    PoolSlab *FirstSlab = (PoolSlab*)Pool->Ptr1;
    if (0 && FirstSlab && FirstSlab->isEmpty()) {
      // Here we choose to delete FirstSlab instead of the pool we just freed
      // from because the pool we just freed from is more likely to be in the
      // processor cache.
    FirstSlab->unlinkFromList();
    FirstSlab->destroy();
    //  Pool->Slabs.erase((void *)FirstSlab);
    }
 
    // Link our slab onto the head of the list so that allocations will find it
    // efficiently.    
    PS->addToList((PoolSlab**)&Pool->Ptr1);
  }

  return; 
}


// SearchForContainingSlab - Do a brute force search through the list of
// allocated slabs for the node in question.
//
static PoolSlab *
SearchForContainingSlab(BitmapPoolTy *Pool, void *Node, unsigned &TheIndex) {
  PoolSlab *PS = (PoolSlab*)Pool->Ptr1;
  unsigned NodeSize = Pool->NodeSize;

  // Search the partially allocated slab list for the slab that contains this
  // node.
  int Idx = -1;
  if (PS) {               // Pool->Ptr1 could be null if Ptr2 isn't
    for (; PS; PS = PS->Next) {
      Idx = PS->containsElement(Node, NodeSize);
      if (Idx != -1) break;
    }
  }

  // If the partially allocated slab list doesn't contain it, maybe the
  // completely allocated list does.
  if (PS == 0) {
    PS = (PoolSlab*)Pool->Ptr2;
    assert(Idx == -1 && "Found node but don't have PS?");
    
    while (PS) {
      assert(PS && "poolfree: node being free'd not found in allocation "
             " pool specified!\n");
      Idx = PS->containsElement(Node, NodeSize);
      if (Idx != -1) break;
      PS = PS->Next;
    }
  }
  
  // Otherwise, maybe its a block within LargeArrays
  if(PS == 0) {
    PS = (PoolSlab*)Pool->LargeArrays;
    assert(Idx == -1  && "Found node but don't have PS?");
    
    while (PS) {
      assert(PS && "poolfree: node being free'd not found in allocation "
             " pool specified!\n");
      Idx = PS->containsElement(Node, NodeSize);
      if (Idx != -1) break;
      PS = PS->Next;
    }
  }

  TheIndex = Idx;
  return PS;
}

//
// Function: __pa_bitmap_poolcheck()
//
// Description:
//  Determine whether the specified pointer is located within the specified
//  pool and, if so, what its beginning address is.
//
void *
__pa_bitmap_poolcheck (BitmapPoolTy * Pool, void * Node) {
  //
  // If there is no pool, do nothing.
  //
  if (!Pool)
    return 0;

  //
  // Search for the object within the pool.
  //
  unsigned TheIndex;
  if (PoolSlab * PS = SearchForContainingSlab (Pool, Node, TheIndex)) {
    return PS->getElementAddress(TheIndex, Pool->NodeSize);
  }

  return 0;
}

