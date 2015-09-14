//===- PoolSlab.h - --*- C++ -*--------------------------------------------===//
// 
//                          The SAFECode Compiler
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file declares the interfaces of slabs which used by pool allocators.
//
// This uses the 'Ptr1' field to maintain a linked list of slabs that are either
// empty or are partially allocated from.  The 'Ptr2' field of the PoolTy is
// used to track a linked list of slabs which are full, ie, all elements have
// been allocated from them.
//
//===----------------------------------------------------------------------===//

#ifndef _POOLSLAB_H_
#define _POOLSLAB_H_

#include "../include/BitmapAllocator.h"
#include "../include/PageManager.h"

#include <cassert>

namespace llvm {

//===----------------------------------------------------------------------===//
//
//  PoolSlab implementation
//
//===----------------------------------------------------------------------===//

// PoolSlab Structure - Hold multiple objects of the current node type.
// Invariants: FirstUnused <= UsedEnd
//
struct PoolSlab {
  PoolSlab **PrevPtr, *Next;
  bool isSingleArray;   // If this slab is used for exactly one array
  unsigned allocated; // Number of bytes allocated
  PoolSlab * Canonical; // For stack slabs, the canonical page

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

  // Size of Slab - For single array slabs, specifies the size of the slab in
  //                bytes from beginning to end (including slab header).
  //
public:
  unsigned int SizeOfSlab;

private:
  // NodeFlagsVector - This array contains two bits for each node in this pool
  // slab.  The first (low address) bit indicates whether this node has been
  // allocated, and the second (next higher) bit indicates whether this is the
  // start of an allocation.
  //
  // This is a variable sized array, which has 2*NumNodesInSlab bits (rounded up
  // to 4 bytes).
  unsigned NodeFlagsVector[1];
  
  bool isNodeAllocated(unsigned NodeNum) {
    return NodeFlagsVector[NodeNum/16] & (1 << (NodeNum & 15));
  }

  void markNodeAllocated(unsigned NodeNum) {
    NodeFlagsVector[NodeNum/16] |= 1 << (NodeNum & 15);
  }

  void markNodeFree(unsigned NodeNum) {
    NodeFlagsVector[NodeNum/16] &= ~(1 << (NodeNum & 15));
  }

  void setStartBit(unsigned NodeNum) {
    NodeFlagsVector[NodeNum/16] |= 1 << ((NodeNum & 15)+16);
  }

public:
  bool isStartOfAllocation(unsigned NodeNum) {
    return NodeFlagsVector[NodeNum/16] & (1 << ((NodeNum & 15)+16));
  }
  
private:
  void clearStartBit(unsigned NodeNum) {
    NodeFlagsVector[NodeNum/16] &= ~(1 << ((NodeNum & 15)+16));
  }

  void assertOkay (void) {
    assert (FirstUnused <= UsedEnd);
    assert ((UsedEnd == getSlabSize()) || (!isNodeAllocated(UsedEnd)));
    assert ((FirstUnused == getSlabSize()) || (!isNodeAllocated(FirstUnused)));
  }
public:
  // create - Create a new (empty) slab and add it to the end of the Pools list.
  static PoolSlab *create(BitmapPoolTy *Pool);

  // createSingleArray - Create a slab for a large singlearray with NumNodes
  // entries in it, returning the pointer into the pool directly.
  static void *createSingleArray(BitmapPoolTy  *Pool, unsigned NumNodes);

  // getSlabSize - Return the number of nodes that each slab should contain.
  static unsigned getSlabSize(BitmapPoolTy  *Pool) {
    // We need space for the header...
    unsigned NumNodes = PageSize-sizeof(PoolSlab);
    
    // We need space for the NodeFlags...
    // FIXME:
    //  We unconditionally round up a byte.  We should only do that if
    //  necessary.
    unsigned NodeFlagsBytes = (NumNodes/Pool->NodeSize * 2 / 8) + 1;
    NumNodes -= (NodeFlagsBytes+3) & ~3;  // Round up to int boundaries.

    // Divide the remainder among the nodes!
    return NumNodes / Pool->NodeSize;
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

  // isEmpty - This is a quick check to see if this slab is completely empty or
  // not.
  bool isEmpty() const { return UsedEnd == 0; }

  // isFull - This is a quick check to see if the slab is completely allocated.
  //
  bool isFull() const { return isSingleArray || (FirstUnused == getSlabSize()); }

  // allocateSingle - Allocate a single element from this pool, returning -1 if
  // there is no space.
  int allocateSingle();

  // allocateMultiple - Allocate multiple contiguous elements from this pool,
  // returning -1 if there is no space.
  int allocateMultiple(unsigned Num);

  // getElementAddress - Return the address of the specified element.
  void *getElementAddress(unsigned ElementNum, unsigned ElementSize) {
    char *Data = (char*)&NodeFlagsVector[((unsigned)NumNodesInSlab+15)/16];
    return &Data[ElementNum*ElementSize];
  }
  
  const void *getElementAddress(unsigned ElementNum, unsigned ElementSize)const{
    const char *Data =
      (const char *)&NodeFlagsVector[(unsigned)(NumNodesInSlab+15)/16];
    return &Data[ElementNum*ElementSize];
  }

  // containsElement - Return the element number of the specified address in
  // this slab.  If the address is not in slab, return -1.
  int containsElement(void *Ptr, unsigned ElementSize) const;

  // freeElement - Free the single node, small array, or entire array indicated.
  void freeElement(unsigned short ElementIdx);
  
  // getSize --- size of an allocation
  unsigned getSize(void *Node, unsigned ElementSize);
  
  // lastNodeAllocated - Return one past the last node in the pool which is
  // before ScanIdx, that is allocated.  If there are no allocated nodes in this
  // slab before ScanIdx, return 0.
  unsigned lastNodeAllocated(unsigned ScanIdx);
};

//===----------------------------------------------------------------------===//
//
//  StackSlab implementation
//
//===----------------------------------------------------------------------===//

//
// Structure: StackSlab
//
// Description:
//  A stack slab is similar to a pool slab but simple and smaller.  It is used
//  for stack allocations that have been promoted to the heap.
struct StackSlab {
public:
  // Pointer to canonical address of stack slab
  StackSlab * Canonical;

  // Pointers for linking in the stack slab
  StackSlab **PrevPtr, *Next;

  // Top of stack
  unsigned int * tos;

  // Data for the stack
  unsigned int data[1020];

  static StackSlab *create (void * p) {
    StackSlab *SS = (StackSlab*) p;
    SS->tos = &(SS->data[0]);
    return SS;
  }

  unsigned char * allocate (unsigned int size) {
    //
    // We will return a pointer to the current top of stack.
    //
    unsigned char * retvalue = (unsigned char *) (tos);

    //
    // Adjust the top of stack down to the next free object.
    //
    size = (size + 3) & (~3u);
    unsigned int NumberOfInts = size / sizeof (unsigned int);
    tos += NumberOfInts;
    assert (tos < &(data[1020]));
    return retvalue;
  }

  void clear (void) {
    tos = data;
  }

  void addToList(StackSlab **PrevPtrPtr) {
    StackSlab *InsertBefore = *PrevPtrPtr;
    *PrevPtrPtr = this;
    PrevPtr = PrevPtrPtr;
    Next = InsertBefore;
    if (InsertBefore) InsertBefore->PrevPtr = &Next;
  }

  void unlinkFromList() {
    *PrevPtr = Next;
    if (Next) Next->PrevPtr = PrevPtr;
  }
};

}
#endif
