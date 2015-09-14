//===- PoolSlab.cpp - -----------------------------------------*- C++ -*---===//
// 
//                          The SAFECode Compiler
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file implements slabs of pool allocators.
//
//===----------------------------------------------------------------------===//

#include "PoolSlab.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace llvm {

// create - Create a new (empty) slab and add it to the end of the Pools list.
PoolSlab *
PoolSlab::create(BitmapPoolTy *Pool) {
  unsigned NodesPerSlab = getSlabSize(Pool);

#ifndef NDEBUG
  unsigned Size = sizeof(PoolSlab) + 4*((NodesPerSlab+15)/16) +
    Pool->NodeSize*getSlabSize(Pool);
  assert(Size <= PageSize && "Trying to allocate a slab larger than a page!");
#endif
  PoolSlab *PS = (PoolSlab*)AllocatePage();

  assert(PS && "Allocating a page failed!");
  memset(PS, 0, sizeof(PoolSlab));
  PS->NumNodesInSlab = NodesPerSlab;
  PS->isSingleArray = 0;  // Not a single array!
  PS->FirstUnused = 0;    // Nothing allocated.
  PS->UsedBegin   = 0;    // Nothing allocated.
  PS->UsedEnd     = 0;    // Nothing allocated.
  PS->allocated   = 0;    // No bytes allocated.

  for (unsigned i = 0; i < PS->getSlabSize(); ++i)
    {
      PS->markNodeFree(i);
      PS->clearStartBit(i);
    }

  // Add the slab to the list...
  PS->addToList((PoolSlab**)&Pool->Ptr1);
  //  printf(" creating a slab %x\n", PS);
  return PS;
}

void *
PoolSlab::createSingleArray(BitmapPoolTy *Pool, unsigned NumNodes) {
  // FIXME: This wastes memory by allocating space for the NodeFlagsVector
  unsigned NodesPerSlab = getSlabSize(Pool);
  assert(NumNodes > NodesPerSlab && "No need to create a single array!");

  unsigned NumPages = (NumNodes+NodesPerSlab-1)/NodesPerSlab;
  PoolSlab *PS = (PoolSlab*)AllocateNPages(NumPages);

  assert(PS && "poolalloc: Could not allocate memory!");

  if (Pool->NumSlabs > BitmapPoolTy::AddrArrSize)
    Pool->Slabs->insert((void*)PS);
  else if (Pool->NumSlabs == BitmapPoolTy::AddrArrSize) {
    // Create the hash_set
    Pool->Slabs = new std::set<void *>;
    Pool->Slabs->insert((void *)PS);
    for (unsigned i = 0; i < BitmapPoolTy::AddrArrSize; ++i)
      Pool->Slabs->insert((void *) Pool->SlabAddressArray[i]);
  } else {
    // Insert it in the array
    Pool->SlabAddressArray[Pool->NumSlabs] = PS;
  }
  Pool->NumSlabs++;

  PS->addToList((PoolSlab**)&Pool->LargeArrays);

  PS->allocated   = 0xffffffff;    // No bytes allocated.
  PS->isSingleArray = 1;
  PS->NumNodesInSlab = NodesPerSlab;
  PS->SizeOfSlab     = (NumPages * PageSize);
  PS->FirstUnused = NumPages;
  return PS->getElementAddress(0, 0);
}

void
PoolSlab::destroy() {
  if (isSingleArray)
    for (unsigned NumPages = FirstUnused; NumPages != 1;--NumPages)
      FreePage((char*)this + (NumPages-1)*PageSize);

  FreePage(this);
}

// allocateSingle - Allocate a single element from this pool, returning -1 if
// there is no space.
int
PoolSlab::allocateSingle() {
  // If the slab is a single array, go on to the next slab.  Don't allocate
  // single nodes in a SingleArray slab.
  if (isSingleArray) return -1;

  unsigned SlabSize = getSlabSize();

  // Check to see if there are empty entries at the end of the slab...
  if (UsedEnd < SlabSize) {
    // Mark the returned entry used
    unsigned short UE = UsedEnd;
    markNodeAllocated(UE);
    setStartBit(UE);
    
    // If we are allocating out the first unused field, bump its index also
    if (FirstUnused == UE) {
      FirstUnused++;
    }
    
    // Updated the UsedBegin field if necessary
    if (UsedBegin > UE) UsedBegin = UE;

    // Return the entry, increment UsedEnd field.
    ++UsedEnd;
    assertOkay();
    allocated += 1;
    return UE;
  }
  
  // If not, check to see if this node has a declared "FirstUnused" value that
  // is less than the number of nodes allocated...
  //
  if (FirstUnused < SlabSize) {
    // Successfully allocate out the first unused node
    unsigned Idx = FirstUnused;
    markNodeAllocated(Idx);
    setStartBit(Idx);
    
    // Increment FirstUnused to point to the new first unused value...
    // FIXME: this should be optimized
    unsigned short FU = FirstUnused;
    do {
      ++FU;
    } while ((FU != SlabSize) && (isNodeAllocated(FU)));
    FirstUnused = FU;

    // Updated the UsedBegin field if necessary
    if (UsedBegin > Idx) UsedBegin = Idx;

    assertOkay();
    allocated += 1;
    return Idx;
  }
  
  assertOkay();
  return -1;
}

//
// Method: allocateMultiple()
//
// Description:
//  Allocate multiple contiguous elements from this pool.
//
// Inputs:
//  Size - The number of *nodes* to allocate from this slab.
//
// Return value:
//  -1 - There is no space for an allocation of this size in the slab.
//  -1 - An attempt was made to use this method on a single array slab.
//  Otherwise, the index number of the first free node in the slab is returned.
//
int
PoolSlab::allocateMultiple(unsigned Size) {
  // Do not allocate small arrays in SingleArray slabs
  if (isSingleArray) return -1;

  // For small array allocation, check to see if there are empty entries at the
  // end of the slab...
  if (UsedEnd+Size <= getSlabSize()) {
    // Mark the returned entry used and set the start bit
    unsigned UE = UsedEnd;
    setStartBit(UE);
    for (unsigned i = UE; i != UE+Size; ++i)
      markNodeAllocated(i);
    
    // If we are allocating out the first unused field, bump its index also
    if (FirstUnused == UE)
      FirstUnused += Size;

    // Updated the UsedBegin field if necessary
    if (UsedBegin > UE) UsedBegin = UE;

    // Increment UsedEnd
    UsedEnd += Size;

    // Return the entry
    assertOkay();
    allocated += Size;
    return UE;
  }

  //
  // If not, check to see if this node has a declared "FirstUnused" value
  // starting which Size nodes can be allocated
  //
  unsigned Idx = FirstUnused;
  while (Idx+Size <= getSlabSize()) {
    assert(!isNodeAllocated(Idx) && "FirstUsed is not accurate!");

    // Check if there is a continuous array of Size nodes starting FirstUnused
    unsigned LastUnused = Idx+1;
    for (; (LastUnused != Idx+Size) && (!isNodeAllocated(LastUnused)); ++LastUnused)
      /*empty*/;

    // If we found an unused section of this pool which is large enough, USE IT!
    if (LastUnused == Idx+Size) {
      setStartBit(Idx);
      // FIXME: this loop can be made more efficient!
      for (unsigned i = Idx; i != Idx + Size; ++i)
        markNodeAllocated(i);

      // This should not be allocating on the end of the pool, so we don't need
      // to bump the UsedEnd pointer.
      assert(Idx != UsedEnd && "Shouldn't allocate at end of pool!");

      // If we are allocating out the first unused field, bump its index also.
      if (Idx == FirstUnused) {
        unsigned SlabSize = getSlabSize();
        unsigned i;
        for (i = FirstUnused+Size; i < UsedEnd; ++i) {
          if (!isNodeAllocated(i)) {
            break;
          }
        }
        FirstUnused = i;
        if (isNodeAllocated(i))
          FirstUnused = SlabSize;
      }
      
      // Updated the UsedBegin field if necessary
      if (UsedBegin > Idx) UsedBegin = Idx;

      // Return the entry
      assertOkay();
      allocated += Size;
      return Idx;
    }

    // Otherwise, try later in the pool.  Find the next unused entry.
    Idx = LastUnused;
    while (Idx+Size <= getSlabSize() && isNodeAllocated(Idx))
      ++Idx;
  }

  assertOkay();
  return -1;
}

// getSize
unsigned PoolSlab::getSize(void *Ptr, unsigned ElementSize) {
  if (isSingleArray) abort();
  const void *FirstElement = getElementAddress(0, 0);
  if (FirstElement <= Ptr) {
    unsigned Delta = (char*)Ptr-(char*)FirstElement;
    unsigned Index = Delta/ElementSize;
    
    if (Index < getSlabSize()) {
      //we have the index now do something like free
      assert(isStartOfAllocation(Index) &&
             "poolrealloc: Attempt to realloc from the middle of allocated array\n");
      unsigned short ElementEndIdx = Index + 1;
      
      // FIXME: This should use manual strength reduction to produce decent code.
      unsigned short UE = UsedEnd;
      while (ElementEndIdx != UE &&
             !isStartOfAllocation(ElementEndIdx) && 
             isNodeAllocated(ElementEndIdx)) {
        ++ElementEndIdx;
      }
      return (ElementEndIdx - Index);
    }
  }
  if (logregs)
    {
      fprintf(stderr, "PoolSlab::getSize failed!\n");
      fflush(stderr);
    }
  abort();
}


//
// Method: containsElement()
//
// Description:
//  Return the element number of the specified address in this slab.  If the
//  address is not in slab, return -1.
//
int
PoolSlab::containsElement(void *Ptr, unsigned ElementSize) const {
  const void *FirstElement = getElementAddress(0, 0);

  //
  // If the pointer is less than the first element of the slab, then it is
  // not within the slab at all.
  //
  if (FirstElement <= Ptr) {

    //
    // Calculate the offet, in bytes, of the pointer from the beginning of the
    // slab.
    //
    unsigned Delta = (char*)Ptr-(char*)FirstElement;

    //
    // If this array is a single array and the pointer is within the bounds of
    // the slab, then simply return the offset of the pointer divided by the
    // size of each element.
    //
    if (isSingleArray) {
      if (Delta < SizeOfSlab) {
        return Delta/ElementSize;
      }
    }

    unsigned Index = Delta/ElementSize;
    if (Index < getSlabSize()) {
      if (Delta % ElementSize != 0) {
        fprintf(stderr, "Freeing pointer into the middle of an element!\n");
        fflush(stderr);
        abort();
      }
      
      return Index;
    }
  }

  //
  // The pointer is not within a slab.
  //
  return -1;
}


// freeElement - Free the single node, small array, or entire array indicated.
void
PoolSlab::freeElement(unsigned short ElementIdx) {
  if (!isNodeAllocated(ElementIdx)) return;
  //  assert(isNodeAllocated(ElementIdx) &&
  //         "poolfree: Attempt to free node that is already freed\n");
#if 0
  assert(!isSingleArray && "Cannot free an element from a single array!");
#else
#if 0
  if (isSingleArray) {
    this->addToList((PoolSlab**)&Pool->FreeLargeArrays);
    return;
  }
#endif
#endif

  // Mark this element as being free!
  markNodeFree(ElementIdx);
  --allocated;

  // If this slab is not a SingleArray
  assert(isStartOfAllocation(ElementIdx) &&
         "poolfree: Attempt to free middle of allocated array\n");
  
  // Free the first cell
  clearStartBit(ElementIdx);
  markNodeFree(ElementIdx);
  
  // Free all nodes if this was a small array allocation.
  unsigned short ElementEndIdx = ElementIdx + 1;

  // FIXME: This should use manual strength reduction to produce decent code.
  unsigned short UE = UsedEnd;
  while (ElementEndIdx != UE &&
         !isStartOfAllocation(ElementEndIdx) && 
         isNodeAllocated(ElementEndIdx)) {
    markNodeFree(ElementEndIdx);
    --allocated;
    ++ElementEndIdx;
  }
  
  // Update the first free field if this node is below the free node line
  if (ElementIdx < FirstUnused) FirstUnused = ElementIdx;

  // Update the first used field if this node was the first used.
  if (ElementIdx == UsedBegin) UsedBegin = ElementEndIdx;
  
  // If we are freeing the last element in a slab, shrink the UsedEnd marker
  // down to the last used node.
  if (ElementEndIdx == UE) {
#if 0
    printf("FU: %d, UB: %d, UE: %d  FREED: [%d-%d)",
           FirstUnused, UsedBegin, UsedEnd, ElementIdx, ElementEndIdx);
#endif

    // If the user is freeing the slab entirely in-order, it's quite possible
    // that all nodes are free in the slab.  If this is the case, simply reset
    // our pointers.
    if (UsedBegin == UE) {
      FirstUnused = 0;
      UsedBegin = 0;
      UsedEnd = 0;
      assertOkay();
    } else if (FirstUnused == ElementIdx) {
      // Freed the last node(s) in this slab.
      FirstUnused = ElementIdx;
      UsedEnd = ElementIdx;
      assertOkay();
    } else {
      UsedEnd = lastNodeAllocated(ElementIdx);
      if (FirstUnused > UsedEnd) FirstUnused = UsedEnd;
      assertOkay();
      assert(FirstUnused <= UsedEnd+1 &&
             "FirstUnused field was out of date!");
    }
  }
  assertOkay();
}

unsigned
PoolSlab::lastNodeAllocated(unsigned ScanIdx) {
  // Check the last few nodes in the current word of flags...
  unsigned CurWord = ScanIdx/16;
  unsigned short Flags = NodeFlagsVector[CurWord] & 0xFFFF;
  if (Flags) {
    // Mask off nodes above this one
    Flags &= (1 << ((ScanIdx & 15)+1))-1;
    if (Flags) {
      // If there is still something in the flags vector, then there is a node
      // allocated in this part.  The goto is a hack to get the uncommonly
      // executed code away from the common code path.
      //printf("A: ");
      goto ContainsAllocatedNode;
    }
  }

  // Ok, the top word doesn't contain anything, scan the whole flag words now.
  --CurWord;
  while (CurWord != ~0U) {
    Flags = NodeFlagsVector[CurWord] & 0xFFFF;
    if (Flags) {
      // There must be a node allocated in this word!
      //printf("B: ");
      goto ContainsAllocatedNode;
    }
    CurWord--;
  }
  return 0;

 ContainsAllocatedNode:
  // Figure out exactly which node is allocated in this word now.  The node
  // allocated is the one with the highest bit set in 'Flags'.
  //
  // This should use __builtin_clz to get the value, but this builtin is only
  // available with GCC 3.4 and above.  :(
  assert(Flags && "Should have allocated node!");
  
  unsigned short MSB;
#if GCC3_4_EVENTUALLY
  MSB = 16 - ::__builtin_clz(Flags);
#else
  for (MSB = 15; (Flags & (1U << MSB)) == 0; --MSB)
    /*empty*/;
#endif

  assert((1U << MSB) & Flags);   // The bit should be set
  assert((~(1U << MSB) & Flags) < Flags);// Removing it should make flag smaller
  ScanIdx = CurWord*16 + MSB;
  assert(isNodeAllocated(ScanIdx));
  return (ScanIdx+1);
}

}
