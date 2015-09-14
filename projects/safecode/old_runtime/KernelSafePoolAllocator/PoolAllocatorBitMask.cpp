//===- PoolAllocatorBitMask.cpp - Implementation of poolallocator runtime -===//
// 
//                       The LLVM Compiler Infrastructure
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
// empty or are partially allocated from.  The 'Ptr2' field of the PoolTy is
// used to track a linked list of slabs which are full, ie, all elements have
// been allocated from them.
//
//===----------------------------------------------------------------------===//

#include "PoolAllocator.h"
#include "PoolCheck.h"
#include "PageManager.h"
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#define DEBUG(x)
#define POOLCHECK(x) x 
//===----------------------------------------------------------------------===//
//
//  PoolSlab implementation
//
//===----------------------------------------------------------------------===//
unsigned ArrayBoundsCheck = 1;

// PoolSlab Structure - Hold multiple objects of the current node type.
// Invariants: FirstUnused <= UsedEnd
//
struct PoolSlab {
  PoolSlab **PrevPtr, *Next;
  bool isSingleArray;   // If this slab is used for exactly one array
  PoolSlab * OrigSlab;
private:
  // FirstUnused - First empty node in slab
  unsigned short FirstUnused;

  // UsedBegin - The first node in the slab that is used.
  unsigned short UsedBegin;

  // UsedEnd - 1 past the last allocated node in slab. 0 if slab is empty
  unsigned short UsedEnd;

  // NumNodesInSlab - This contains the number of nodes in this slab, which
  // effects the size of the NodeFlags vector, and indicates the number of nodes
  // which are in the slab.
  unsigned int NumNodesInSlab;
  // NodeFlagsVector - This array contains two bits for each node in this pool
  // slab.  The first (low address) bit indicates whether this node has been
  // allocated, and the second (next higher) bit indicates whether this is the
  // start of an allocation.
  //
  // This is a variable sized array, which has 2*NumNodesInSlab bits (rounded up
  // to 4 bytes).
  unsigned NodeFlagsVector1;
  bool isNodeAllocated(unsigned NodeNum) {
    unsigned * NodeFlagsVector = &NodeFlagsVector1;
    return NodeFlagsVector[NodeNum/16] & (1 << (NodeNum & 15));
  }

  void markNodeAllocated(unsigned NodeNum) {
    unsigned * NodeFlagsVector = &NodeFlagsVector1;
    NodeFlagsVector[NodeNum/16] |= 1 << (NodeNum & 15);
  }

  void markNodeFree(unsigned NodeNum) {
    unsigned * NodeFlagsVector = &NodeFlagsVector1;
    NodeFlagsVector[NodeNum/16] &= ~(1 << (NodeNum & 15));
  }

  void setStartBit(unsigned NodeNum) {
    unsigned * NodeFlagsVector = &NodeFlagsVector1;
    NodeFlagsVector[NodeNum/16] |= 1 << ((NodeNum & 15)+16);
  }

  bool isStartOfAllocation(unsigned NodeNum) {
    unsigned * NodeFlagsVector = &NodeFlagsVector1;
    return NodeFlagsVector[NodeNum/16] & (1 << ((NodeNum & 15)+16));
  }
  
  void clearStartBit(unsigned NodeNum) {
    unsigned * NodeFlagsVector = &NodeFlagsVector1;
    NodeFlagsVector[NodeNum/16] &= ~(1 << ((NodeNum & 15)+16));
  }

public:
  // create - Create a new (empty) slab and add it to the end of the Pools list.
  static PoolSlab *create(PoolTy *Pool);

  // createSingleArray - Create a slab for a large singlearray with NumNodes
  // entries in it, returning the pointer into the pool directly.
  static void *createSingleArray(PoolTy *Pool, unsigned NumNodes);

  // getSlabSize - Return the number of nodes that each slab should contain.
  static unsigned getSlabSize(PoolTy *Pool) {
    // We need space for the header...
    unsigned NumNodes = PageSize-sizeof(PoolSlab);
    
    // We need space for the NodeFlags...
    unsigned NodeFlagsBytes = NumNodes/Pool->NodeSize * 2 / 8;
    NumNodes -= (NodeFlagsBytes+3) & ~3;  // Round up to int boundaries.

    // Divide the remainder among the nodes!
    return NumNodes / Pool->NodeSize;
  }

  void initialize(unsigned NodesPerSlab) ;
  
  unsigned int GetNumNodesInSlab() {
    return NumNodesInSlab;
  }
  
  void addToList(PoolSlab **PrevPtrPtr) {
    PoolSlab *InsertBefore = *PrevPtrPtr;
    *PrevPtrPtr = this;
    PrevPtr = PrevPtrPtr;
    Next = InsertBefore;
    if (InsertBefore) InsertBefore->PrevPtr = &Next;
  }

  void unlinkFromList() {
    *PrevPtr = Next;
    if (Next) Next->PrevPtr = PrevPtr;
  }

  unsigned getSlabSize() const {
    return NumNodesInSlab;
  }

  // destroy - Release the memory for the current object.
  void destroy();


  // Unmap - Release the memory for the current object.
  void mprotect();
  
  // isEmpty - This is a quick check to see if this slab is completely empty or
  // not.
  bool isEmpty() const { return UsedEnd == 0; }

  // isFull - This is a quick check to see if the slab is completely allocated.
  //
  bool isFull() const { return isSingleArray || FirstUnused == getSlabSize(); }

  // allocateSingle - Allocate a single element from this pool, returning -1 if
  // there is no space.
  int allocateSingle();

  // allocateMultiple - Allocate multiple contiguous elements from this pool,
  // returning -1 if there is no space.
  int allocateMultiple(unsigned Num);

  // getElementAddress - Return the address of the specified element.
  void *getElementAddress(unsigned ElementNum, unsigned ElementSize) {
    unsigned * NodeFlagsVector = &NodeFlagsVector1;
    char *Data = (char*)&NodeFlagsVector[((unsigned)NumNodesInSlab+15)/16];
    return &Data[ElementNum*ElementSize];
  }
  const void *getElementAddress(unsigned ElementNum, unsigned ElementSize)const{
    const unsigned * NodeFlagsVector = &NodeFlagsVector1;
    const char *Data =
      (const char *)&NodeFlagsVector[(unsigned)(NumNodesInSlab+15)/16];
    return &Data[ElementNum*ElementSize];
  }

  // containsElement - Return the element number of the specified address in
  // this slab.  If the address is not in slab, return -1.
  int containsElement(void *Ptr, unsigned ElementSize) const;

  // freeElement - Free the single node, small array, or entire array indicated.
  void freeElement(unsigned short ElementIdx);
  
  // lastNodeAllocated - Return one past the last node in the pool which is
  // before ScanIdx, that is allocated.  If there are no allocated nodes in this
  // slab before ScanIdx, return 0.
  unsigned lastNodeAllocated(unsigned ScanIdx);
};

// create - Create a new (empty) slab and add it to the end of the Pools list.
PoolSlab *PoolSlab::create(PoolTy *Pool) {
  unsigned NodesPerSlab = getSlabSize(Pool);

  unsigned Size = sizeof(PoolSlab) + 4*((NodesPerSlab+15)/16) +
    Pool->NodeSize*getSlabSize(Pool);
  assert(Size <= PageSize && "Trying to allocate a slab larger than a page!");
  PoolSlab *PS = (PoolSlab*)AllocatePage();

  PS->NumNodesInSlab = NodesPerSlab;
  PS->OrigSlab = PS; 
  PS->isSingleArray = 0;  // Not a single array!
  PS->FirstUnused = 0;    // Nothing allocated.
  PS->UsedBegin   = 0;    // Nothing allocated.
  PS->UsedEnd     = 0;    // Nothing allocated.

  // Add the slab to the list...
  PS->addToList((PoolSlab**)&Pool->Ptr1);
  //  printf(" creating a slab %x\n", PS);
  return PS;
}

void *PoolSlab::createSingleArray(PoolTy *Pool, unsigned NumNodes) {
  // FIXME: This wastes memory by allocating space for the NodeFlagsVector
  unsigned NodesPerSlab = getSlabSize(Pool);
  assert(NumNodes > NodesPerSlab && "No need to create a single array!");

  unsigned NumPages = (NumNodes+NodesPerSlab-1)/NodesPerSlab;
  PoolSlab *PS = (PoolSlab*)AllocateNPages(NumPages);

  assert(PS && "poolalloc: Could not allocate memory!");

  if (Pool->NumSlabs > AddrArrSize)
    Pool->Slabs->insert((void*)PS);
  else if (Pool->NumSlabs == AddrArrSize) {
    // Create the hash_set
    Pool->Slabs = new hash_set<void *>;
    Pool->Slabs->insert((void *)PS);
    for (unsigned i = 0; i < AddrArrSize; ++i)
      Pool->Slabs->insert((void *) Pool->SlabAddressArray[i]);
  } else {
    // Insert it in the array
    Pool->SlabAddressArray[Pool->NumSlabs] = (unsigned) PS;
  }
  Pool->NumSlabs++;
  
  PS->addToList((PoolSlab**)&Pool->LargeArrays);

  PS->isSingleArray = 1;
  PS->OrigSlab = PS;
  PS->NumNodesInSlab = NumPages * PageSize;
  *(unsigned*)&PS->FirstUnused = NumPages;
  return PS->getElementAddress(0, 0);
}
void MprotectPage(void *pa, unsigned numPages);

void PoolSlab::mprotect() {
  if (isSingleArray) {
      unsigned NumPages = *(unsigned*)&FirstUnused; 
      MprotectPage((char*)this, NumPages);
  }
  else 
    MprotectPage(this, 1);
}

void PoolSlab::destroy() {
  if (isSingleArray)
    for (unsigned NumPages = *(unsigned*)&FirstUnused; NumPages != 1;--NumPages)
      FreePage((char*)this + (NumPages-1)*PageSize);

  FreePage(this);
}

// allocateSingle - Allocate a single element from this pool, returning -1 if
// there is no space.
int PoolSlab::allocateSingle() {
  // If the slab is a single array, go on to the next slab.  Don't allocate
  // single nodes in a SingleArray slab.
  if (isSingleArray) return -1;

  unsigned SlabSize = PoolSlab::getSlabSize();

  // Check to see if there are empty entries at the end of the slab...
  if (UsedEnd < SlabSize) {
    // Mark the returned entry used
    unsigned short UE = UsedEnd;
    markNodeAllocated(UE);
    setStartBit(UE);
    
    // If we are allocating out the first unused field, bump its index also
    if (FirstUnused == UE)
      FirstUnused++;
    
    // Return the entry, increment UsedEnd field.
    return UsedEnd++;
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
    } while (FU != SlabSize && isNodeAllocated(FU));
    FirstUnused = FU;
    
    return Idx;
  }
  
  return -1;
}

// allocateMultiple - Allocate multiple contiguous elements from this pool,
// returning -1 if there is no space.
int PoolSlab::allocateMultiple(unsigned Size) {
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

    // Increment UsedEnd
    UsedEnd += Size;

    // Return the entry
    return UE;
  }

  // If not, check to see if this node has a declared "FirstUnused" value
  // starting which Size nodes can be allocated
  //
  unsigned Idx = FirstUnused;
  while (Idx+Size <= getSlabSize()) {
    assert(!isNodeAllocated(Idx) && "FirstUsed is not accurate!");

    // Check if there is a continuous array of Size nodes starting FirstUnused
    unsigned LastUnused = Idx+1;
    for (; LastUnused != Idx+Size && !isNodeAllocated(LastUnused); ++LastUnused)
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
      if (Idx == FirstUnused)
        FirstUnused += Size;
      
      // Return the entry
      return Idx;
    }

    // Otherwise, try later in the pool.  Find the next unused entry.
    Idx = LastUnused;
    while (Idx+Size <= getSlabSize() && isNodeAllocated(Idx))
      ++Idx;
  }

  return -1;
}


// containsElement - Return the element number of the specified address in
// this slab.  If the address is not in slab, return -1.
int PoolSlab::containsElement(void *Ptr, unsigned ElementSize) const {
  const void *FirstElement = getElementAddress(0, 0);
  if (FirstElement <= Ptr) {
    unsigned Delta = (char*)Ptr-(char*)FirstElement;
    if (isSingleArray) 
      if (Delta < NumNodesInSlab) return Delta/ElementSize;
    unsigned Index = Delta/ElementSize;
    if (Index < getSlabSize()) {
      if (Delta % ElementSize != 0) {
	printf("Freeing pointer into the middle of an element!");
	abort();
      }
      
      return Index;
    }
  }
  return -1;
}

void PoolSlab::initialize(unsigned NodesPerSlab) {
  
 NumNodesInSlab = NodesPerSlab;
 isSingleArray = 0;  // Not a single array!
 FirstUnused = 0;    // Nothing allocated.
 UsedBegin   = 0;    // Nothing allocated.
 UsedEnd     = 0;    // Nothing allocated.
  
  // Add the slab to the list...
}
// freeElement - Free the single node, small array, or entire array indicated.
void PoolSlab::freeElement(unsigned short ElementIdx) {
  if (!isNodeAllocated(ElementIdx)) return;
  //  assert(isNodeAllocated(ElementIdx) &&
  //         "poolfree: Attempt to free node that is already freed\n");
  //  assert(!isSingleArray && "Cannot free an element from a single array!");

  // Mark this element as being free!
  markNodeFree(ElementIdx);

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
      //printf(": SLAB EMPTY\n");
      FirstUnused = 0;
      UsedBegin = 0;
      UsedEnd = 0;
    } else if (FirstUnused == ElementIdx) {
      // Freed the last node(s) in this slab.
      FirstUnused = ElementIdx;
      UsedEnd = ElementIdx;
    } else {
      UsedEnd = lastNodeAllocated(ElementIdx);
      assert(FirstUnused <= UsedEnd+1 &&
             "FirstUnused field was out of date!");
    }
  }
}

unsigned PoolSlab::lastNodeAllocated(unsigned ScanIdx) {
  // Check the last few nodes in the current word of flags...
  unsigned CurWord = ScanIdx/16;
  unsigned * NodeFlagsVector = &NodeFlagsVector1;
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
    unsigned * NodeFlagsVector = &NodeFlagsVector1;
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
  return ScanIdx;
}


//===----------------------------------------------------------------------===//
//
//  Pool allocator library implementation
//
//===----------------------------------------------------------------------===//

// poolinit - Initialize a pool descriptor to empty
//
void poolinit(PoolTy *Pool, unsigned NodeSize) {
  assert(Pool && "Null pool pointer passed into poolinit!\n");
  DEBUG(printf("pool init %x, %d\n", Pool, NodeSize);)

  // Ensure the page manager is initialized
  InitializePageManager();

  // We must alway return unique pointers, even if they asked for 0 bytes
  Pool->NodeSize = NodeSize ? NodeSize : 1;
  Pool->Ptr1 = Pool->Ptr2 = 0;
  Pool->LargeArrays = 0;
  // For SAFECode, we set FreeablePool to 0 always
  //  Pool->FreeablePool = 0;
  Pool->lastUsed = 0;
  Pool->prevPage[0] = 0;
  Pool->prevPage[1] = 0;
  // Initialize the SlabAddressArray to zero
  for (int i = 0; i < AddrArrSize; ++i) {
    Pool->SlabAddressArray[i] = 0;
  }

  Pool->NumSlabs = 0;
  POOLCHECK(poolcheckinit(Pool, NodeSize);)
  POOLCHECK(Pool->splay = new_splay();)
  POOLCHECK(Pool->PCS = 0;)
  ///  Pool->Slabs = new hash_set<void*>;
  // Call hash_set constructor explicitly
  //   void *SlabPtr = &Pool->Slabs;
  //   new (SlabPtr) hash_set<void*>;
}

void poolmakeunfreeable(PoolTy *Pool) {
  assert(Pool && "Null pool pointer passed in to poolmakeunfreeable!\n");
  //  Pool->FreeablePool = 0;
}

// pooldestroy - Release all memory allocated for a pool
//
void pooldestroy(PoolTy *Pool) {
  DEBUG(printf("pooldestroying %x\n", Pool);)
  assert(Pool && "Null pool pointer passed in to pooldestroy!\n");
  if (Pool->NumSlabs > AddrArrSize) {
    Pool->Slabs->clear();
    delete Pool->Slabs;
  }
  POOLCHECK(free_splay(Pool->splay);)
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

  POOLCHECK(poolcheckdestroy(Pool);)
}


// poolallocarray - a helper function used to implement poolalloc, when the
// number of nodes to allocate is not 1.
static void *poolallocarray(PoolTy* Pool, unsigned Size) {
  assert(Pool && "Null pool pointer passed into poolallocarray!\n");
  if (Size > PoolSlab::getSlabSize(Pool))
    return PoolSlab::createSingleArray(Pool, Size);
 
  PoolSlab *PS = (PoolSlab*)Pool->Ptr1;

  // Loop through all of the slabs looking for one with an opening
  for (; PS; PS = PS->Next) {
    int Element = PS->allocateMultiple(Size);
    if (Element != -1) {
      // We allocated an element.  Check to see if this slab has been completely
      // filled up.  If so, move it to the Ptr2 list.
      if (PS->isFull()) {
        PS->unlinkFromList();
        PS->addToList((PoolSlab**)&Pool->Ptr2);
      }
      return PS->getElementAddress(Element, Pool->NodeSize);
    }
  }
  
  PoolSlab *New = PoolSlab::create(Pool);
  POOLCHECK(poolcheckAddSlab(&Pool->PCS, New);)
  if (Pool->NumSlabs > AddrArrSize) {
    DEBUG(printf("new slab inserting %x \n", (void *)New);)
    Pool->Slabs->insert((void *)New);
  } else if (Pool->NumSlabs == AddrArrSize) {
    // Create the hash_set
    Pool->Slabs = new hash_set<void *>;
    Pool->Slabs->insert((void *)New);
    for (unsigned i = 0; i < AddrArrSize; ++i)
      Pool->Slabs->insert((void *)Pool->SlabAddressArray[i]);
  } else {
    // Insert it in the array
    Pool->SlabAddressArray[Pool->NumSlabs] = (unsigned) New;
  }
  Pool->NumSlabs++;
  
  //  return malloc(Size * Pool->NodeSize);
  int Idx = New->allocateMultiple(Size);
  assert(Idx == 0 && "New allocation didn't return zero'th node?");
  return New->getElementAddress(0, 0);
}

void *poolalloc(PoolTy *Pool, unsigned NumBytes) {
  //  return malloc(Size * Pool->NodeSize);
  void *retAddress = NULL;
  if (!Pool) {
    printf("Null pool pointer passed in to poolalloc!, FAILING\n");
    exit(-1);
  } 
  unsigned NodeSize = Pool->NodeSize;
  unsigned NodesToAllocate1 = NumBytes / NodeSize;
  if (NodesToAllocate1 == 0) {
    //    abort();
  }
  unsigned NodesToAllocate = (NumBytes+NodeSize-1)/NodeSize;
  if (NodesToAllocate > 1) {
    retAddress = poolallocarray(Pool, NodesToAllocate);
    DEBUG(printf("poolalloc: Pool %x NodeSize %d retaddress %x numbytes %d\n",Pool, Pool->NodeSize, retAddress, NumBytes);)
      POOLCHECK(poolcheckregister(Pool->splay, retAddress, NumBytes);)
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
      retAddress = PS->getElementAddress(Element, NodeSize);
      DEBUG(printf("poolalloc: Pool %x NodeSize %d retaddress %x numbytes %d\n",Pool, Pool->NodeSize, retAddress, NumBytes);)
      POOLCHECK(poolcheckregister(Pool->splay, retAddress, NumBytes);)
      return retAddress;
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
	retAddress = PS->getElementAddress(Element, NodeSize);
	DEBUG(printf("poolalloc: Pool %x NodeSize %d retaddress %x numbytes %d\n",Pool, Pool->NodeSize, retAddress, NumBytes);)
        POOLCHECK(poolcheckregister(Pool->splay, retAddress, NumBytes);)
	return retAddress;
      }
    }
  }

  // Otherwise we must allocate a new slab and add it to the list
  PoolSlab *New = PoolSlab::create(Pool);
  POOLCHECK(poolcheckAddSlab(&Pool->PCS, New);)
  
  if (Pool->NumSlabs > AddrArrSize)
    Pool->Slabs->insert((void *)New);
  else if (Pool->NumSlabs == AddrArrSize) {
    // Create the hash_set
    Pool->Slabs = new hash_set<void *>;
    Pool->Slabs->insert((void *)New);
    for (unsigned i = 0; i < AddrArrSize; ++i)
      Pool->Slabs->insert((void *)Pool->SlabAddressArray[i]);
  } else {
    // Insert it in the array
    Pool->SlabAddressArray[Pool->NumSlabs] = (unsigned) New;
  }
  Pool->NumSlabs++;

  int Idx = New->allocateSingle();
  assert(Idx == 0 && "New allocation didn't return zero'th node?");
  retAddress = New->getElementAddress(0, 0);
  DEBUG(printf("poolalloc: Pool %x NodeSize %d retaddress %x numbytes %d\n",Pool, Pool->NodeSize, retAddress, NumBytes);)
  POOLCHECK(poolcheckregister(Pool->splay, retAddress, NumBytes);)
  return retAddress;
}

/*
// SearchForContainingSlab - This implementation uses the hash_set as well
// as the array to search the list of allocated slabs for the node in question
static PoolSlab *SearchForContainingSlab(PoolTy *Pool, void *Node,
                                         unsigned &TheIndex) {
  //  printf("in pool check for pool %x, node %x\n",Pool,Node);
  unsigned NodeSize = Pool->NodeSize;
  void *PS;
  if (!Pool) {
    printf("Empty Pool in pool check FAILING \n");
    exit(-1);
  } 
  assert (Pool->AllocadPool <= 0 && "SearchForContainingSlab not to be called"
          " for alloca'ed pools");
  
  PS = (void*)((long)Node & ~(PageSize-1));
  if (Pool->NumSlabs > AddrArrSize) {
    hash_set<void*> &theSlabs = *Pool->Slabs;
    if (theSlabs.find(PS) == theSlabs.end()) {
      // Check the LargeArrays
      if (Pool->LargeArrays) {
	PoolSlab *PSlab = (PoolSlab*) Pool->LargeArrays;
	unsigned Idx = -1;
	while (PSlab) {
	  assert(PSlab && "poolcheck: node being free'd not found in "
		 "allocation pool specified!\n");
	  Idx = PSlab->containsElement(Node, NodeSize);
	  if (Idx != -1) break;
	  PSlab = PSlab->Next;
	}
	if (Idx == -1) {
	  printf("poolcheck: node being checked not found in pool \n");
	  abort();
	}
	TheIndex = Idx;
	return PSlab;
      } else {
	printf("poolcheck: node being checked not found in pool \n");
	abort();
      }
    } else {
      // Check that Node does not point to PoolSlab meta-data
      if ((PoolSlab *)PS->getElementAddress(0,0) > (long) Node) {
	printf("poolcheck: node being checked points to meta-data \n");
	abort();
      }	
      TheIndex = PS->containsElement(Node, NodeSize);
      return (PoolSlab *)PS;
    }
  } else {
    bool found;
    for (unsigned i = 0; i < AddrArrSize && !found; ++i) {
      if (Pool->SlabAddressArray[i] == (unsigned) PS)
	found = true;
    } 

    if (found) {
      // Check that Node does not point to PoolSlab meta-data
      if ((PoolSlab *)PS->getElementAddress(0,0) > (long) Node) {
	printf("poolcheck: node being checked points to meta-data \n");
	abort();
      }	
      TheIndex = PS->containsElement(Node, NodeSize);
      return (PoolSlab *)PS;
    } else {
      // Check the LargeArrays
      if (Pool->LargeArrays) {
	PoolSlab *PSlab = (PoolSlab*) Pool->LargeArrays;
	unsigned Idx = -1;
	while (PSlab) {
	  assert(PSlab && "poolcheck: node being free'd not found in "
		 "allocation pool specified!\n");
	  Idx = PSlab->containsElement(Node, NodeSize);
	  if (Idx != -1) break;
	  PSlab = PSlab->Next;
	}
	if (Idx == -1) {
	  printf("poolcheck: node being checked not found in pool \n");
	  abort();
	}
	TheIndex = Idx;
	return PSlab;	
      }
      printf("poolcheck: node being checked not found in pool \n");
      abort();
    }
  }
}
*/

#if 0
// SearchForContainingSlab - Do a brute force search through the list of
// allocated slabs for the node in question.
//
static PoolSlab *SearchForContainingSlab(PoolTy *Pool, void *Node,
                                         unsigned &TheIndex) {
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
#endif

void* poolallocatorcheck(PoolTy *Pool, void *Node) {
  PoolSlab *PS = (PoolSlab*)((unsigned)Node & ~(PageSize-1));

  if (Pool->NumSlabs > AddrArrSize) {
    hash_set<void*> &theSlabs = *Pool->Slabs;
    if (theSlabs.find((void*)PS) == theSlabs.end()) {
      // Check the LargeArrays
      if (Pool->LargeArrays) {
	PoolSlab *PSlab = (PoolSlab*) Pool->LargeArrays;
	int Idx = -1;
	while (PSlab) {
	  assert(PSlab && "poolcheck: node being free'd not found in "
		 "allocation pool specified!\n");
	  Idx = PSlab->containsElement(Node, Pool->NodeSize);
	  if (Idx != -1) {
	    Pool->prevPage[Pool->lastUsed] = PS;
	    Pool->lastUsed = (Pool->lastUsed + 1) % 4;
	    return PS;
	    break;
	  }
	  PSlab = PSlab->Next;
	}
	
	if (Idx == -1) {
	  Splay *ref = splay_find_ptr(Pool->splay, (unsigned long) Node);
	  if (ref)   {
	    return 0;
	  }
	  printf("poolcheck1: node being checked not found in pool with right"
		 " alignment\n");
	  abort();
	} else {
	  return 0;
	  //exit(-1);
	}
      } else {
	// here we check for the splay tree
	Splay *ref = splay_find_ptr(Pool->splay, (unsigned long) Node);
	if (ref) {
	  return 0;
	} else {
	  printf("poolcheck2: ref not found "
		 " alignment\n");
	  abort();
	  exit(-1);
	}
      }
    } else {
      unsigned long startaddr = (unsigned long)PS->getElementAddress(0,0);
      if (startaddr > (unsigned long) Node) {
	printf("poolcheck: node being checked points to meta-data \n");
	abort();
      }
      unsigned long offset = ((unsigned long) Node - (unsigned long) startaddr) % Pool->NodeSize;
      if (offset != 0) {
	printf("poolcheck3: node being checked does not have right alignment\n");
	abort();
      }
      Pool->prevPage[Pool->lastUsed] = PS;
      Pool->lastUsed = (Pool->lastUsed + 1) % 4;
      return PS;
    }
  } else {
    bool found = false;
    for (unsigned i = 0; i < AddrArrSize && !found; ++i) {
      if ((unsigned)Pool->SlabAddressArray[i] == (unsigned) PS) {
	found = true;
	Pool->prevPage[Pool->lastUsed] = PS;
	Pool->lastUsed = (Pool->lastUsed + 1) % 4;
      }
    } 

    if (found) {
      // Check that Node does not point to PoolSlab meta-data
      unsigned long startaddr = (unsigned long)PS->getElementAddress(0,0);
      if (startaddr > (unsigned long) Node) {
	printf("poolcheck: node being checked points to meta-data \n");
	exit(-1);
      }
      unsigned long offset = ((unsigned long) Node - (unsigned long) startaddr) % Pool->NodeSize;
      if (offset != 0) {
	printf("poolcheck4: node being checked does not have right alignment\n");
	abort();
      }
      return PS;
    } else {
      // Check the LargeArrays
      if (Pool->LargeArrays) {
	PoolSlab *PSlab = (PoolSlab*) Pool->LargeArrays;
	int Idx = -1;
	while (PSlab) {
	  assert(PSlab && "poolcheck: node being free'd not found in "
		 "allocation pool specified!\n");
	  Idx = PSlab->containsElement(Node, Pool->NodeSize);
	  if (Idx != -1) {
	    Pool->prevPage[Pool->lastUsed] = PS;
	    Pool->lastUsed = (Pool->lastUsed + 1) % 4;
	    break;
	  }
	  PSlab = PSlab->Next;
	}
	if (Idx == -1) {
	  Splay *ref = splay_find_ptr(Pool->splay, (unsigned long) Node);
	  if (ref)    return 0;	  
	  printf("poolcheck6: node being checked not found in pool with right"
		 " alignment\n");
	  abort();
	} else {
	  return PSlab;
	}
      } else {
	Splay *ref = splay_find_ptr(Pool->splay, (unsigned long) Node);
	if (ref) {
	  return 0;
	} else {
	  printf("poolcheck5: ref not found "
		 " alignment check not done \n");
	  abort();
	}	
      }
    }
  }
}

/*
void poolcheckarray(MetaPoolTy **MP, void *NodeSrc, void *NodeResult) {
  MetaPoolTy *MetaPool = *MP;
  if (!MetaPool) {
    printf("Empty meta pool? \n");
    exit(-1);
  }
  //iteratively search through the list
  //Check if there are other efficient data structures.
  hash_set<void *>::iterator PTI = MetaPool->PoolTySet->begin(), PTE = MetaPool->PoolTySet->end();
  PoolTy *srcPool = 0;
  for (; PTI != PTE; ++PTI) {
    PoolTy *Pool = (PoolTy *)*PTI;
    PoolSlab *PS;
    PS = (PoolSlab*)((unsigned long)NodeSrc & ~(PageSize-1));
    if (Pool->prevPage[0] == PS) {
      srcPool = Pool;
    }
    if (Pool->prevPage[1] == PS) {
      srcPool = Pool;
    }    
    if (Pool->prevPage[2] == PS) {
      srcPool = Pool;
    }    
    if (Pool->prevPage[3] == PS) {
      srcPool = Pool;
    }
    if (poolcheckoptim(Pool, NodeSrc)) {
      srcPool = Pool;
    } else continue;
    //Now check for reuslt
    if (poolcheckoptim(srcPool, NodeResult)) {          
        MetaPool->cachePool = srcPool;
        return;
    } else {
        printf("source and dest belong to different pools\n");
        exit(-1);
    }
  }
  printf("poolcheck failure \n");
  exit(-1);
}
*/

void poolfree(PoolTy *Pool, void *Node) {
  assert(Pool && "Null pool pointer passed in to poolfree!\n");
  DEBUG(printf("poolfree  %x %x \n",Pool,Node);)
  PoolSlab *PS;
  int Idx;
  PS = (PoolSlab *)poolallocatorcheck(Pool, Node);
  assert(PS && "poolfree the element not in pool ");
  if (PS->isSingleArray) {
    PS->unlinkFromList();
    unsigned  NumPages = PS->GetNumNodesInSlab() / PageSize;
    
    unsigned NodesPerSlab = PoolSlab::getSlabSize(Pool);

    unsigned Size = sizeof(PoolSlab) + 4*((NodesPerSlab+15)/16) +
      Pool->NodeSize* NodesPerSlab;
    assert(Size <= PageSize && "Trying to allocate a slab larger than a page!");
    for (unsigned i = 0; i <NumPages; ++i) {
      PoolSlab *PSi = (PoolSlab *)((unsigned long)PS + i * PageSize);

      PSi->initialize(NodesPerSlab);
      if (i != 0) {
	PSi->addToList((PoolSlab**)&Pool->Ptr1);
      }
      if (Pool->NumSlabs > AddrArrSize) {
	DEBUG(printf("new slab inserting %x \n", (void *)New);)
	  Pool->Slabs->insert((void *)PSi);
      } else if (Pool->NumSlabs == AddrArrSize) {
	// Create the hash_set
	Pool->Slabs = new hash_set<void *>;
	Pool->Slabs->insert((void *)PSi);
	for (unsigned i = 0; i < AddrArrSize; ++i)
	  Pool->Slabs->insert((void *)Pool->SlabAddressArray[i]);
      } else {
	// Insert it in the array
	Pool->SlabAddressArray[Pool->NumSlabs] = (unsigned) PSi;
      }
      Pool->NumSlabs++;       
    }
    return;
  } 
  Idx = PS->containsElement(Node, Pool->NodeSize);
  assert((Idx != -1) && " node not present, it should have aborted ");

  // If PS was full, it must have been in list #2.  Unlink it and move it to
  // list #1.
  if (PS->isFull()) {
    // Now that we found the node, we are about to free an element from it.
    // This will make the slab no longer completely full, so we must move it to
    // the other list!
    PS->unlinkFromList(); // Remove it from the Ptr2 list.

    PoolSlab **InsertPosPtr = (PoolSlab**)&Pool->Ptr1;

    // If the partially full list has an empty node sitting at the front of the
    // list, insert right after it.
    if (*InsertPosPtr && (*InsertPosPtr)->isEmpty())
      InsertPosPtr = &(*InsertPosPtr)->Next;

    PS->addToList(InsertPosPtr);     // Insert it now in the Ptr1 list.
  }

  // Free the actual element now!
  PS->freeElement(Idx);

  // Ok, if this slab is empty, we unlink it from the of slabs and either move
  // it to the head of the list, or free it, depending on whether or not there
  // is already an empty slab at the head of the list.
  //
  if (PS->isEmpty()) {
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
      //      Pool->Slabs.erase((void *)FirstSlab);
    }
 
    // Link our slab onto the head of the list so that allocations will find it
    // efficiently.    
    PS->addToList((PoolSlab**)&Pool->Ptr1);
  }
}


  void *poolrealloc(PoolTy *Pool, void *Node, unsigned NumBytes) {
    if (Node == 0) return poolalloc(Pool, NumBytes);
    if (NumBytes == 0) {
      poolfree(Pool, Node);
      return 0;
    }
    void *New = poolalloc(Pool, NumBytes);
    //    unsigned Size =
    //FIXME the following may not work in all cases  
    memcpy(New, Node, NumBytes);
    poolfree(Pool, Node);
    return New;
  }

  void poolregister(PoolTy *Pool, void *allocadptr, unsigned NumBytes) {
    POOLCHECK(poolcheckregister(Pool->splay, allocadptr, NumBytes);)
  }
  PoolCheckSlab *poolcheckslab(void *Pool) {
    return ((PoolTy *)Pool)->PCS;
  }

  Splay *poolchecksplay(void *Pool) {
    return ((PoolTy *)Pool)->splay;
  }

  void poolcheckfail (const char * msg) {
    fprintf (stderr, msg);
    fflush (stderr);
    exit (-1);
  }

  void * poolcheckmalloc (unsigned int size) {
    return malloc (size);
  }
