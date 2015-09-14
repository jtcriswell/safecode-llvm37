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
// empty or are partially allocated from.  The 'Ptr2' field of the PoolTy is
// used to track a linked list of slabs which are full, ie, all elements have
// been allocated from them.
//
//===----------------------------------------------------------------------===//
// NOTES:
//  1) Some of the bounds checking code may appear strange.  The reason is that
//     it is manually inlined to squeeze out some more performance.  Please
//     don't change it.
//
//  2) This run-time performs MMU re-mapping of pages to perform dangling
//     pointer detection.  A "shadow" address is the address of a memory block
//     that has been remapped to a new virtal address; the shadow address is
//     returned to the caller on allocation and is unmapped on deallocation.
//     A "canonical" address is the virtual address of memory as it is mapped
//     in the pool slabs; the canonical address is remapped to different shadow
//     addresses each time that particular piece of memory is allocated.
//
//     In normal operation, the shadow address and canonical address are
//     identical.
//
//===----------------------------------------------------------------------===//

#include "ConfigData.h"
#include "PoolAllocator.h"
#include "PageManager.h"
#include "Report.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <signal.h>
#include <ucontext.h>
#include <sys/mman.h>
#if 0
#include <sys/ucontext.h>
#endif

#include <pthread.h>

#define DEBUG(x)

#ifndef SC_DEBUGTOOL
#define DISABLED_IN_PRODUCTION_VERSION assert(0 && "Disabled in production version")
#else
#define DISABLED_IN_PRODUCTION_VERSION
#endif

// global variable declarations
extern unsigned PPageSize;
#if SC_DEBUGTOOL
static unsigned globalallocID = 0;
static unsigned globalfreeID = 0;
static PoolTy dummyPool;
static unsigned dummyInitialized = 0;
#endif

#if SC_ENABLE_OOB
static unsigned char * invalidptr = 0;
#endif

static void * globalTemp = 0;
unsigned poolmemusage = 0;

/// UNUSED in production version
FILE * ReportLog = 0;

// Configuration for C code; flags that we should stop on the first error
unsigned StopOnError = 0;

// Invalid address range
#if !defined(__linux__)
unsigned InvalidUpper = 0x00000000;
unsigned InvalidLower = 0x00000003;
#endif

// Splay tree of external objects
extern RangeSplaySet<> ExternalObjects;

// Records Out of Bounds pointer rewrites; also used by OOB rewrites for
// exactcheck() calls
static PoolTy OOBPool;

// Map between rewrite pointer and source file information
std::map<void *, void*>    RewriteSourcefile;
std::map<void *, unsigned> RewriteLineno;

// Map between a real value and its rewritten value
std::map<void *, void *>   RewrittenPointers;

// Map between the start of a real object and the set of OOB pointers
// associated with it
std::map<void *, std::vector<void*> > RewrittenStart;

// Record from which object an OOB pointer originates
std::map<void *, std::pair<void *, void * > > RewrittenObjs;

#ifndef SC_DEBUGTOOL
/// It should be always zero in production version 
static /*const*/ unsigned logregs = 0;
#else
/* Set to 1 to log object registrations */
static /*const*/ unsigned logregs = 0;
#endif

#if SC_DEBUGTOOL
// signal handler
static void bus_error_handler(int, siginfo_t *, void *);

// creates a new PtrMetaData structure to record pointer information
static inline void updatePtrMetaData(PDebugMetaData, unsigned, void *);
static PDebugMetaData createPtrMetaData (unsigned,
                                         unsigned,
                                         void *,
                                         void *,
                                         void *,
                                         char * SourceFile = "",
                                         unsigned lineno = 0);
#endif

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
  static PoolSlab *create(PoolTy *Pool);

  // createSingleArray - Create a slab for a large singlearray with NumNodes
  // entries in it, returning the pointer into the pool directly.
  static void *createSingleArray(PoolTy *Pool, unsigned NumNodes);

  // getSlabSize - Return the number of nodes that each slab should contain.
  static unsigned getSlabSize(PoolTy *Pool) {
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

// create - Create a new (empty) slab and add it to the end of the Pools list.
PoolSlab *
PoolSlab::create(PoolTy *Pool) {
  unsigned NodesPerSlab = getSlabSize(Pool);

  unsigned Size = sizeof(PoolSlab) + 4*((NodesPerSlab+15)/16) +
    Pool->NodeSize*getSlabSize(Pool);
  assert(Size <= PageSize && "Trying to allocate a slab larger than a page!");
  PoolSlab *PS = (PoolSlab*)AllocatePage();

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
PoolSlab::createSingleArray(PoolTy *Pool, unsigned NumNodes) {
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
    Pool->SlabAddressArray[Pool->NumSlabs] = PS;
  }
  Pool->NumSlabs++;
  
  PS->addToList((PoolSlab**)&Pool->LargeArrays);

  PS->allocated   = 0xffffffff;    // No bytes allocated.
  PS->isSingleArray = 1;
  PS->NumNodesInSlab = NodesPerSlab;
  PS->SizeOfSlab     = (NumPages * PageSize);
  *(unsigned*)&PS->FirstUnused = NumPages;
  return PS->getElementAddress(0, 0);
}

void
PoolSlab::destroy() {
  if (isSingleArray)
    for (unsigned NumPages = *(unsigned*)&FirstUnused; NumPages != 1;--NumPages)
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

//===----------------------------------------------------------------------===//
//
//  Pool allocator library implementation
//
//===----------------------------------------------------------------------===//

//
// Function: isRewritePtr()
//
// Description:
//  Determines whether the specified pointer value is a rewritten value for an
//  Out-of-Bounds pointer value.
//
// Return value:
//  true  - The pointer value is an OOB pointer rewrite value.
//  false - The pointer value is the actual value of the pointer.
//
static bool
isRewritePtr (void * p) {
  unsigned ptr = (unsigned) p;

  if ((InvalidLower < ptr ) && (ptr < InvalidUpper))
    return true;
  return false;
}

//
// Function: pool_init_runtime()
//
// Description:
//  This function is called to initialize the entire SAFECode run-time.  It
//  configures the various run-time options for SAFECode and performs other
//  initialization tasks.
//
// Inputs:
//  Dangling   - Set to non-zero to enable dangling pointer detection.
//  RewriteOOB - Set to non-zero to enable Out-Of-Bounds pointer rewriting.
//  Termiante  - Set to non-zero to have SAFECode terminate when an error
//               occurs.
//
void
pool_init_runtime (unsigned Dangling, unsigned RewriteOOB, unsigned Terminate) {
  //
  // Configure the allocator.
  //
  ConfigData.RemapObjects = Dangling;
  ConfigData.StrictIndexing = !(RewriteOOB);
  StopOnError = Terminate;

  //
  // Allocate a range of memory for rewrite pointers.
  //
#if !defined(__linux__)
  const unsigned invalidsize = 1 * 1024 * 1024 * 1024;
  void * Addr = mmap (0, invalidsize, 0, MAP_SHARED | MAP_ANON, -1, 0);
  if (Addr == MAP_FAILED) {
     perror ("mmap:");
     fflush (stdout);
     fflush (stderr);
     assert(0 && "valloc failed\n");
  }
  madvise (Addr, invalidsize, MADV_FREE);
  InvalidLower = (unsigned int) Addr;
  InvalidUpper = (unsigned int) Addr + invalidsize;
#endif

  //
  // Leave initialization of the Report logfile to the reporting routines.
  // The libc stdio functions may have not been initialized by this point, so
  // we cannot rely upon them working.
  //
  ReportLog = stderr;

  //
  // Install hooks for catching allocations outside the scope of SAFECode.
  //
  if (ConfigData.TrackExternalMallocs) {
    extern void installAllocHooks(void);
    installAllocHooks();
  }

#if SC_DEBUGTOOL  
  //
  // Initialize the dummy pool.
  //
  poolinit (&dummyPool, 1);

  //
  // Initialize the signal handlers for catching errors.
  //
  struct sigaction sa;
  bzero (&sa, sizeof (struct sigaction));
  sa.sa_sigaction = bus_error_handler;
  sa.sa_flags = SA_SIGINFO;
  if (sigaction(SIGBUS, &sa, NULL) == -1) {
    fprintf (stderr, "sigaction installer failed!");
    fflush (stderr);
  }
  if (sigaction(SIGSEGV, &sa, NULL) == -1) {
    fprintf (stderr, "sigaction installer failed!");
    fflush (stderr);
  }
#endif

  return;
}

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
poolinit(PoolTy *Pool, unsigned NodeSize) {
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
  Pool->AllocadPool = -1;
  Pool->allocaptr = 0;
  Pool->lastUsed = 0;
  Pool->prevPage[0] = 0;
  Pool->prevPage[1] = 0;
  // Initialize the SlabAddressArray to zero
  for (int i = 0; i < AddrArrSize; ++i) {
    Pool->SlabAddressArray[i] = 0;
  }
  Pool->NumSlabs = 0;
#if 0
  Pool->RegNodes = new std::map<void*,unsigned>;
#endif

  //
  // Call the in-place constructor for the splay tree of objects and, if
  // applicable, the set of Out of Bound rewrite pointers and the splay tree
  // used for dangling pointer detection.
  //
  new (&(Pool->Objects)) RangeSplaySet<>();
#if SC_ENABLE_OOB 
  new (&(Pool->OOB)) RangeSplayMap<void *>();
#endif
#if SC_DEBUGTOOL
  new (&(Pool->DPTree)) RangeSplayMap<PDebugMetaData>();
#endif
}

void
poolmakeunfreeable(PoolTy *Pool) {
  assert(Pool && "Null pool pointer passed in to poolmakeunfreeable!\n");
  //  Pool->FreeablePool = 0;
}

// pooldestroy - Release all memory allocated for a pool
//
// FIXME: determine how to adjust debug logs when 
//        pooldestroy is called
void
pooldestroy(PoolTy *Pool) {
  DISABLED_IN_PRODUCTION_VERSION ;
  assert(Pool && "Null pool pointer passed in to pooldestroy!\n");
  Pool->Objects.clear();

  if (Pool->AllocadPool) return;

  // Remove all registered pools
#if 0
  delete Pool->RegNodes;
#endif

  if (Pool->NumSlabs > AddrArrSize) {
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
// FIXME: look into globalTemp, make it a pass by reference arg instead of
//          a global variable.
// FIXME: determine whether Size is bytes or number of nodes.
//
static void *
poolallocarray(PoolTy* Pool, unsigned Size) {
  DISABLED_IN_PRODUCTION_VERSION ;
  assert(Pool && "Null pool pointer passed into poolallocarray!\n");
  
  // check to see if we need to allocate a single large array
  if (Size > PoolSlab::getSlabSize(Pool)) {
    if (logregs) {
      fprintf(stderr, " poolallocarray:694: Size = %d, SlabSize = %d\n", Size, PoolSlab::getSlabSize(Pool));
      fflush(stderr);
    }
    globalTemp = (PoolSlab*) PoolSlab::createSingleArray(Pool, Size);
    uintptr_t offset = (uintptr_t)globalTemp & (PPageSize - 1);
    void * retAddress = (void *)((uintptr_t)(globalTemp) & ~(PPageSize - 1));

    if (logregs) {
      fprintf(stderr, " poolallocarray:704: globalTemp = 0x%p, offset = 0x%08lx, retAddress = 0x%p\n",
              globalTemp, offset, retAddress);
      fflush(stderr);
    }
    return (void*) ((char*)retAddress + offset);
  }
 
  PoolSlab *PS = (PoolSlab*)Pool->Ptr1;
  unsigned offset;

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
      
      //
      // FIXME:
      //  We may have some inter-procedural communication via globalTemp.  We
      //  need to fix that if it exists.
      //

      //
      // Set the globalTemp variable to the address of the newly allocated
      // memory.
      //
      globalTemp = PS->getElementAddress(Element, Pool->NodeSize);

      //
      // Find the offset of the object within the physical page to which it
      // belongs.
      //
      offset = (uintptr_t)globalTemp & (PPageSize - 1); 

      //
      // Remap the page to get a shadow page (used for dangling pointer
      // detection).
      //
      void * RemappedPage = RemapObject(globalTemp, Size*Pool->NodeSize);

      if (logregs) {
        fprintf(stderr, " poolallocarray:735: globalTemp = %p\n", globalTemp);
        fprintf(stderr, " poolallocarray:737: Element = 0x%0x\n", Element);
        fprintf(stderr, " poolallocarray:739: = NodeSize = 0x%0x\n", Pool->NodeSize);
        fprintf(stderr ," poolallocarray:736: Page = %p, offset = 0x%08x, retAddress = %p\n",
              RemappedPage, offset, (char*)RemappedPage + offset);
        fflush (stderr);
      }

      return (void*) ((char*)RemappedPage + offset);
    }
  }
  
  PoolSlab *New = PoolSlab::create(Pool);
  //  printf("new slab created %x \n", New);
  if (Pool->NumSlabs > AddrArrSize)
    Pool->Slabs->insert((void *)New);
  else if (Pool->NumSlabs == AddrArrSize) {
    // Create the hash_set
    Pool->Slabs = new hash_set<void *>;
    Pool->Slabs->insert((void *)New);
    for (unsigned i = 0; i < AddrArrSize; ++i)
      Pool->Slabs->insert((void *)Pool->SlabAddressArray[i]);
  }
  else {
    // Insert it in the array
    Pool->SlabAddressArray[Pool->NumSlabs] = New;
  }
  
  Pool->NumSlabs++;
  
  int Idx = New->allocateMultiple(Size);
  assert(Idx == 0 && "New allocation didn't return zero'th node?");
  
  // insert info into adl splay tree for poolcheck runtime
  //unsigned NodeSize = Pool->NodeSize;
  globalTemp = New->getElementAddress(0, 0);
  //adl_splay_insert(&(Pool->Objects), globalTemp, 
  //          (unsigned)((Size*NodeSize) - NodeSize + 1), (Pool));
  
  // remap page to get a shadow page (dangling pointer detection library)
  New = (PoolSlab *) RemapObject(globalTemp, Size*Pool->NodeSize);
  offset = (uintptr_t)globalTemp & (PPageSize - 1);
  if (logregs) {
    fprintf(stderr, " poolallocarray:774: globalTemp = 0x%p\n, offset = 0x%x\n", (void*)globalTemp, offset);
    fprintf(stderr, " poolallocarray:775: New = 0x%p, Size = %d, retAddress = 0x%p\n",
        (void*)New, Size, (char*)New + offset);
  }
  return (void*) ((char*)New + offset);
}

//
// Function: poolargvregister()
//
// Description:
//  Register all of the argv strings in the external object pool.
//
void
poolargvregister (int argc, char ** argv) {
  for (unsigned index=0; index < argc; ++index) {
    if (logregs) {
      fprintf (stderr, "poolargvregister: %x %x: %s\n", argv[index], strlen(argv[index]), argv[index]);
      fflush (stderr);
    }
    ExternalObjects.insert(argv[index], argv[index] + strlen (argv[index]));
  }

  return;
}

//
// Function: __barebone_poolregister()
//
// Description:
//  This function implements the basic functionality of adding an object to
//  a pool's splay trees.  It is an internal function used by the poolalloc()
//  and poolregister() functions for adding an object to a pool.
//
static inline void
__barebone_poolregister (PoolTy *Pool, void * allocaptr, unsigned NumBytes) {
  //
  // If the pool is NULL or the object has zero length, don't do anything.
  //
  if ((!Pool) || (NumBytes == 0)) return;

  //
  // Add the object to the pool's splay of valid objects.
  //
  if (!(Pool->Objects.insert(allocaptr, (char*) allocaptr + NumBytes - 1))) {
    fprintf (stderr, "Object Already Registered: %x: %d\n", allocaptr, NumBytes);
    fflush (stderr);
    assert (0 && "__barebone_poolregister: Object Already Registered!\n");
    abort();
  }
}

//
// Function: poolregister()
//
// Description:
//  Register the memory starting at the specified pointer of the specified size
//  with the given Pool.
//
void
poolregister (PoolTy *Pool, void * allocaptr, unsigned NumBytes) {
    //
    // Record information about this allocation in the global debugging
    // structure.
#if SC_DEBUGTOOL
    PDebugMetaData debugmetadataPtr;
    globalallocID++;
    debugmetadataPtr = createPtrMetaData (globalallocID,
                                          globalfreeID,
                                          __builtin_return_address(0),
                                          0,
                                          globalTemp, "<unknown>", 0);
    dummyPool.DPTree.insert (allocaptr,
                             (char*) allocaptr + NumBytes - 1,
                             debugmetadataPtr);
#endif

  //
  // Do the actual registration.
  //
  if (allocaptr) __barebone_poolregister (Pool, allocaptr, NumBytes);

  //
  // Provide some debugging information on the pool register.
  //
  if (logregs) {
    fprintf (ReportLog, "poolregister: %p %p %x\n", Pool, (void*)allocaptr, NumBytes);
    fflush (ReportLog);
  }
}

//
// Function: poolunregister()
//
// Description:
//  Remove the specified object from the set of valid objects in the Pool.
//
// Inputs:
//  Pool      - The pool in which the object should belong.
//  allocaptr - A pointer to the object to remove.
//
// Notes:
//  Note that this function currently deallocates debug information about the
//  allocation.  This is safe because this function is only called on stack
//  objects.  This is less-than-ideal because we lose debug information about
//  the allocation of the stack object if it is later dereferenced outside its
//  function (dangling pointer), but it is currently too expensive to keep that
//  much debug information around.
//
//  TODO: What are the restrictions on allocaptr?
//
void
poolunregister(PoolTy *Pool, void * allocaptr) {
  //
  // If no pool was specified, then do nothing.
  //
  if (!Pool) return;

  //
  // Remove the object from the pool's splay tree.
  //
  Pool->Objects.remove (allocaptr);

  // Canonical pointer for the pointer we're freeing
  void * CanonNode = allocaptr;

#if SC_DEBUGTOOL
  //
  // Increment the ID number for this deallocation.
  //
  globalfreeID++;

  // The start and end of the object as registered in the dangling pointer
  // object metapool
  void * start, * end;

  // FIXME: figure what NumPPAge and len are for
  unsigned len = 1;
  unsigned NumPPage = 0;
  unsigned offset = (unsigned)((long)allocaptr & (PPageSize - 1));
  PDebugMetaData debugmetadataptr = 0;
  
  //
  // Retrieve the debug information about the node.  This will include a
  // pointer to the canonical page.
  //
  bool found = dummyPool.DPTree.find (allocaptr, start, end, debugmetadataptr);

  //
  // If we cannot find the meta-data for this pointer, then the free is
  // invalid.  Report it as an error and then continue executing if possible.
  //
  if (!found) {
    ReportInvalidFree ((unsigned)__builtin_return_address(0),
                       allocaptr,
                       "<Unknown>",
                       0);
    return;
  }

  // Assert that we either didn't find the object or we found the object *and*
  // it has meta-data associated with it.
  assert ((!found || (found && debugmetadataptr)) &&
          "poolfree: No debugmetadataptr\n");

  if (logregs) {
    fprintf(stderr, "poolfree:1387: start = 0x%08x, end = 0x%x,  offset = 0x%08x\n", (unsigned)start, (unsigned)(end), offset);
    fprintf(stderr, "poolfree:1388: len = %d\n", len);
    fflush (stderr);
  }

  //
  // If dangling pointer detection is not enabled, remove the object from the
  // dangling pointer splay tree.  The memory object's memory will be reused,
  // and we don't want to match it for subsequently allocated objects.
  //
  if (!(ConfigData.RemapObjects)) {
    dummyPool.DPTree.remove (allocaptr);
  }

  // figure out how many pages does this object span to
  //  protect the pages. First we sum the offset and len
  //  to get the total size we originally remapped.
  //  Then, we determine if this sum is a multiple of
  //  physical page size. If it is not, then we increment
  //  the number of pages to protect.
  //  FIXME!!!
  NumPPage = (len / PPageSize) + 1;
  if ( (len - (NumPPage-1) * PPageSize) > (PPageSize - offset) )
    NumPPage++;

  //
  // If this is a remapped pointer, find its canonical address.
  //
  if (ConfigData.RemapObjects) {
    CanonNode = debugmetadataptr->canonAddr;
    updatePtrMetaData (debugmetadataptr,
                       globalfreeID,
                       __builtin_return_address(0));
  }

  if (logregs) {
    fprintf(stderr, " poolfree:1397: NumPPage = %d\n", NumPPage);
    fprintf(stderr, " poolfree:1398: canonical address is 0x%x\n", (unsigned)CanonNode);
    fflush (stderr);
  }
#endif

#if 0
  //
  // Remove rewrite pointers for this object.
  //
  if (RewrittenStart.find (allocaptr) != RewrittenStart.end()) {
    for (unsigned index=0; index < RewrittenStart[allocaptr].size(); ++index) {
      void * invalidptr = RewrittenStart[allocaptr][index];
      if (logregs) {
        fprintf (stderr, "Removing write ptr %x -> %x\n", allocaptr, invalidptr);
        fflush (stderr);
      }
      Pool->OOB.remove (invalidptr);
      OOBPool.OOB.remove (invalidptr); 
      RewrittenPointers.erase (allocaptr);
    }

    RewrittenStart.erase (allocaptr);
  }
#endif

  if (logregs) {
    fprintf (stderr, "pooluregister: %p\n", allocaptr);
  }
}

//
// Function: pool_protect_object()
//
// Description:
//  This function modifies the page protections of an object so that it is no
//  longer writeable.
//
// Inputs:
//  Node - A pointer to the beginning of the object that should be marked as
//         read-only.
// Notes:
//  This function should only be called when dangling pointer detection is
//  enabled.
//
void
pool_protect_object (void * Node) {
#if SC_DEBUGTOOL
  // The start and end of the object as registered in the dangling pointer
  // object metapool
  void * start, * end;

  //
  // Retrieve the debug information about the node.  This will include a
  // pointer to the canonical page.
  //
  PDebugMetaData debugmetadataptr = 0;
  bool found = dummyPool.DPTree.find (Node, start, end, debugmetadataptr);

  // Assert that we either didn't find the object or we found the object *and*
  // it has meta-data associated with it.
  assert ((!found || (found && debugmetadataptr)) &&
          "poolfree: No debugmetadataptr\n");

  //
  // If the object is not found, return.
  //
  if (!found) return;

  //
  // Determine the number of pages that the object occupies.
  //
  unsigned len = (unsigned)end - (unsigned)start;
  unsigned offset = (unsigned)((long)Node & (PPageSize - 1));
  unsigned NumPPage = (len / PPageSize) + 1;
  if ( (len - (NumPPage-1) * PPageSize) > (PPageSize - offset) )
    NumPPage++;

  // Protect the shadow pages of the object
  ProtectShadowPage((void *)((long)Node & ~(PPageSize - 1)), NumPPage);
#endif
  return;
}

extern "C" void frag(PoolTy * Pool);
void
frag(PoolTy * Pool) {
  unsigned long totalalloc=0;
  unsigned long total=0;
  for (PoolSlab *PS = (PoolSlab*)Pool->Ptr1; PS; PS = PS->Next) {
    total += PS->getSlabSize();
    totalalloc += PS->allocated;
    fprintf (stderr, "%2f\n", (double) PS->allocated * 100 / (double) PS->getSlabSize());
  }
  fprintf (stderr, "%lu %lu %2f\n", totalalloc, total, (double) totalalloc * 100 / (double) total);
  fflush (stderr);
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
poolalloc(PoolTy *Pool, unsigned NumBytes) {
  DISABLED_IN_PRODUCTION_VERSION ;
  void *retAddress = NULL;
  assert(Pool && "Null pool pointer passed into poolalloc!\n");

#if 0
  // Ensure that stack objects and heap objects d not belong to the same pool.
  if (Pool->AllocadPool != -1) {
    if (Pool->AllocadPool != 0) {
    printf(" Did not Handle this case, an alloa and malloc point to");
    printf("same DSNode, Will happen in stack safety \n");
    exit(-1);
    }
  }
  else {
    Pool->AllocadPool = 0;
  }
#endif
   
  // Ensure that we're always allocating at least 1 byte.
  if (NumBytes == 0)
    NumBytes = 1;

  //
  // Calculate the number of nodes within the pool to allocate for an object
  // of the specified size.
  //
  unsigned NodeSize = Pool->NodeSize;
  unsigned NodesToAllocate = (NumBytes + NodeSize - 1)/NodeSize;
  unsigned offset = 0;
  
  // Call a helper function if we need to allocate more than 1 node.
  if (NodesToAllocate > 1) {
    if (logregs) {
      fprintf(stderr, " poolalloc:848: Allocating more than 1 node for %d bytes\n", NumBytes); fflush(stderr);
    }

    //
    // Allocate the memory.
    //
    retAddress = poolallocarray(Pool, NodesToAllocate);
    
    //if ((unsigned)retAddress > 0x2f000000 && logregs == 0)
    //  logregs = 1;
    
    if (logregs) {
      fprintf(stderr, " poolalloc:863: Pool=%p, retAddress = 0x%p NumBytes = %d globalTemp = 0x%p pc = 0x%p\n", (void*) (Pool),
          (void*)retAddress, NumBytes, (void*)globalTemp, __builtin_return_address(0)); fflush(stderr);
    }
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
      
      globalTemp = PS->getElementAddress(Element, NodeSize);
      offset = (uintptr_t)globalTemp & (PPageSize - 1);
      if (logregs) {
        fprintf(stderr, " poolalloc:885: canonical page = 0x%p offset = 0x%08x\n", (void*)globalTemp, offset);
      }
      //adl_splay_insert(&(Pool->Objects), globalTemp, NumBytes, (Pool));
      
      // remap page to get a shadow page for dangling pointer library
      PS = (PoolSlab *) RemapObject(globalTemp, NumBytes);
      retAddress = (void*) ((char*)PS + offset);

      if (logregs) {
        fprintf(stderr, " poolalloc:900: Pool=%p, retAddress = 0x%p, NumBytes = %d\n", (void*)(Pool), (void*)retAddress, NumBytes);
      }
      assert (retAddress && "poolalloc(2): Returning NULL!\n");
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
        
        globalTemp = PS->getElementAddress(Element, NodeSize);
        offset = (uintptr_t)globalTemp & (PPageSize - 1);
      
        // remap page to get a shadow page for dangling pointer library
        PS = (PoolSlab *) RemapObject(globalTemp, NumBytes);
        retAddress = (void*) ((char*)PS + offset);
        if (logregs) {
          fprintf (stderr, " poolalloc:932: PS = 0x%p, retAddress = 0x%p, NumBytes = %d, offset = 0x%08x\n",
                (void*)PS, (void*)retAddress, NumBytes, offset);
        }
        assert (retAddress && "poolalloc(3): Returning NULL!\n");
        return retAddress;
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
  
  if (Pool->NumSlabs > AddrArrSize)
    Pool->Slabs->insert((void *)New);
  else if (Pool->NumSlabs == AddrArrSize) {
    // Create the hash_set
    Pool->Slabs = new hash_set<void *>;
    Pool->Slabs->insert((void *)New);
    for (unsigned i = 0; i < AddrArrSize; ++i)
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
  globalTemp = New->getElementAddress(0, 0);
  offset = (uintptr_t)globalTemp & (PPageSize - 1);
  
  if (logregs) {
    fprintf(stderr, " poolalloc:973: element at 0x%p, offset=0x%08x\n", (void*)globalTemp, offset);
  }
  
  // remap  page to get a shadow page for dangling pointer library
  New = (PoolSlab *) RemapObject(globalTemp, NumBytes);
  offset = (uintptr_t)globalTemp & (PPageSize - 1);
  retAddress = (void*) ((char*)New + offset);

  if (logregs) {
    fprintf (stderr, " poolalloc:990: New = 0x%p, retAddress = 0x%p, NumBytes = %d, offset = 0x%08x pc=0x%p\n",
          (void*)New, (void*)retAddress, NumBytes, offset, __builtin_return_address(0));
  }
  assert (retAddress && "poolalloc(4): Returning NULL!\n");
  return retAddress;
}

#if SC_DEBUGTOOL
//
// Function: poolalloc_debug()
//
// Description:
//  This function is just like poolalloc() except that it associates a source
//  file and line number with the allocation.
//
// Notes:
//  This function does *not* register the allocated object within the splay
//  tree.  That is done by poolregister().
//
void *
poolalloc_debug (PoolTy *Pool,
                 unsigned NumBytes,
                 void * SourceFilep,
                 unsigned lineno) {
  //
  // Ensure that we're allocating at least one byte.
  //
  if (NumBytes == 0) NumBytes = 1;

  // Do some initial casting for type goodness
  char * SourceFile = (char *)(SourceFilep);

  // Perform the allocation and determine its offset within the physical page.
  void * canonptr = __barebone_poolalloc(Pool, NumBytes);
  uintptr_t offset = (((uintptr_t)(canonptr)) & (PPageSize-1));

  // Remap the object if necessary.
  void * shadowpage = RemapObject (canonptr, NumBytes);
  void * shadowptr = (unsigned char *)(shadowpage) + offset;

  //
  // Create the meta data object containing the debug information and the
  // mapping from shadow address to canonical address.
  //
  PDebugMetaData debugmetadataPtr;
  globalallocID++;
  debugmetadataPtr = createPtrMetaData (globalallocID,
                                        globalfreeID,
                                        __builtin_return_address(0),
                                        0,
                                        canonptr, SourceFile, lineno);
  dummyPool.DPTree.insert (shadowptr,
                           (char*) shadowptr + NumBytes - 1,
                           debugmetadataPtr);

  if (logregs) {
    fprintf(stderr, "poolalloc_debug: Pool=%p, addr=%p, size=%d, %s, %d\n", Pool, shadowptr, NumBytes, SourceFile, lineno);
    fflush (stderr);
  }

  // Return the shadow pointer.
  return shadowptr;
}

//
// Function: poolregister_debug()
//
// Description:
//  Register the memory starting at the specified pointer of the specified size
//  with the given Pool.  This version will also record debug information about
//  the object being registered.
//
// Notes:
//  This function should never be used to register an object which can be
//  freed via a heap free function.  The only objects registered with this
//  function should be globals and stack objects.
//
void
poolregister_debug (PoolTy *Pool,
                    void * allocaptr,
                    unsigned NumBytes,
                    void * SourceFilep,
                    unsigned lineno) {
  // Do some initial casting for type goodness
  char * SourceFile = (char *)(SourceFilep);

  //
  // Create the meta data object containing the debug information for this
  // pointer.  These pointers will never be shadowed, but we want to record
  // information about the allocation in case a bounds check on this object
  // fails.
  //
  PDebugMetaData debugmetadataPtr;
  globalallocID++;
  debugmetadataPtr = createPtrMetaData (globalallocID,
                                        globalfreeID,
                                        __builtin_return_address(0),
                                        0,
                                        allocaptr, SourceFile, lineno);
  dummyPool.DPTree.insert (allocaptr,
                           (char*) allocaptr + NumBytes - 1,
                           debugmetadataPtr);

  //
  // Call the real poolregister() function to register the object.
  //
  if (logregs) {
    fprintf (ReportLog, "poolregister_debug: %p: %p %d: %s %d\n", Pool, (void*)allocaptr, NumBytes, SourceFile, lineno);
    fflush (ReportLog);
  }
  __barebone_poolregister (Pool, allocaptr, NumBytes);
}

//
// Function: poolcalloc_debug()
//
// Description:
//  This is the same as pool_calloc but with source level debugging
//  information.
//
// Inputs:
//  Pool        - The pool from which to allocate the elements.
//  Number      - The number of elements to allocate.
//  NumBytes    - The size of each element in bytes.
//  SourceFilep - A pointer to the source filename in which the caller is
//                located.
//  lineno      - The line number at which the call occurs in the source code.
//
// Return value:
//  NULL - The allocation did not succeed.
//  Otherwise, a fresh pointer to the allocated memory is returned.
//
// Notes:
//  Note that this function calls poolregister() directly because the SAFECode
//  transforms do not add explicit calls to poolregister().
//
void *
poolcalloc_debug (PoolTy *Pool,
                  unsigned Number,
                  unsigned NumBytes,
                  void * SourceFilep,
                  unsigned lineno) {
  void * New = poolalloc_debug (Pool, Number * NumBytes, SourceFilep, lineno);
  if (New) {
    bzero (New, Number * NumBytes);
    poolregister (Pool, New, Number * NumBytes);
  }
  if (logregs) {
    fprintf (ReportLog, "poolcalloc_debug: %p: %p %x: %s %d\n", Pool, (void*)New, Number * NumBytes, SourceFilep, lineno);
    fflush (ReportLog);
  }
  return New;
}

//
// Function: poolfree_debug()
//
// Description:
//  This function is identical to poolfree() except that it relays source-level
//  debug information to the error reporting routines.
//
void
poolfree_debug (PoolTy *Pool,
                void * Node,
                void * SourceFile,
                unsigned lineno) {
  PDebugMetaData debugmetadataptr = 0;
  void * start, * end;

  if (logregs) {
    fprintf(stderr, "poolfree_debug: Pool=%p, addr=%p, %s, %d\n", Pool, Node, SourceFile, lineno);
    fflush (stderr);
  }

  //
  // Check whether the pointer is valid.
  //
#if 0
  if (!dummyPool.DPTree.find (Node, start, end, debugmetadataptr)) {
    ReportInvalidFree ((unsigned)__builtin_return_address(0),
                       Node,
                       (char *) SourceFile,
                       lineno);
    return;
  } else {
    poolfree (Pool, Node);
  }
#else
  poolfree (Pool, Node);
#endif
}
#endif

void *
poolrealloc(PoolTy *Pool, void *Node, unsigned NumBytes) {
  //
  // If the object has never been allocated before, allocate it now.
  //
  if (Node == 0) {
    void * New = poolalloc(Pool, NumBytes);
    poolregister (Pool, New, NumBytes);
    return New;
  }

  //
  // Reallocate an object to 0 bytes means that we wish to free it.
  //
  if (NumBytes == 0) {
    pool_protect_object (Node);
    poolunregister(Pool, Node);
    poolfree(Pool, Node);
    return 0;
  }

  //
  // Otherwise, we need to change the size of the allocated object.  For now,
  // we will simply allocate a new object and copy the data from the old object
  // into the new object.
  //
  void *New;
  if ((New = poolalloc(Pool, NumBytes)) == 0)
    return 0;

  //
  // Get the bounds of the old object.  If we cannot get the bounds, then
  // simply fail the allocation.
  //
  void * S, * end;
  if ((!(Pool->Objects.find (Node, S, end))) || (S != Node)) {
    return 0;
  }

  //
  // Register the new object with the pool.
  //
  poolregister (Pool, New, NumBytes);

  //
  // Determine the number of bytes to copy into the new object.
  //
  unsigned length = NumBytes;
  if ((((unsigned)(end)) - ((unsigned)(S)) + 1) < NumBytes) {
    length = ((unsigned)(end)) - ((unsigned)(S)) + 1;
  }

  //
  // Copy the contents of the old object into the new object.
  //
  memcpy(New, Node, length);

  //
  // Invalidate the old object and its bounds and return the pointer to the
  // new object.
  //
  pool_protect_object (Node);
  poolunregister(Pool, Node);
  poolfree(Pool, Node);
  return New;
}

//
// Function: poolcalloc()
//
// Description:
//  This function is the pool allocation equivalent of calloc().  It allocates
//  an array of elements and zeros out the memory.
//
// Inputs:
//  Pool     - The pool from which to allocate the elements.
//  Number   - The number of elements to allocate.
//  NumBytes - The size of each element in bytes.
//
// Return value:
//  NULL - The allocation did not succeed.
//  Otherwise, a fresh pointer to the allocated memory is returned.
//
// Notes:
//  Note that this function calls poolregister() directly because the SAFECode
//  transforms do not add explicit calls to poolregister().
//
void *
poolcalloc (PoolTy *Pool, unsigned Number, unsigned NumBytes) {
  void * New = poolalloc (Pool, Number * NumBytes);
  if (New) {
    bzero (New, Number * NumBytes);
    poolregister (Pool, New, Number * NumBytes);
  }
  return New;
}

void *
poolstrdup(PoolTy *Pool, char *Node) {
  if (Node == 0) return 0;

  unsigned int NumBytes = strlen(Node) + 1;
  void *New = poolalloc(Pool, NumBytes);
  if (New) {
    memcpy(New, Node, NumBytes+1);
  }
  return New;
}

//
// Function: pool_newstack()
//
// Description:
//  Create a new pool slab for the given function invocation.
//
void
pool_newstack (PoolTy * Pool) {
  // Pointer to the new pool slab
  StackSlab * PS;

  //
  // Get a new stack slab.  Either reuse an old one or create a new one.
  //
  assert ((sizeof (StackSlab) <= 4096) && "StackSlab too big!\n");
  if (Pool->FreeStackSlabs) {
    PS = (StackSlab *)(Pool->FreeStackSlabs);
    PS->unlinkFromList();
  } else {
    PS = (StackSlab*) valloc (sizeof (StackSlab));
  }

  //
  // Ensure we have a pool slab.
  //
  assert (PS && "pool_newstack: Can't create new slab\n");

  //
  // Remap the stack slab into a new virtual address space.
  //
  PS->Canonical = PS;
  PS = (StackSlab *) RemapObject (PS, sizeof (StackSlab));

  //
  // Initialize it.
  //
  PS = StackSlab::create (PS);

  //
  // Link the shadow slab into the set of stack slabs.
  //
  PS->addToList((StackSlab**)&Pool->StackSlabs);
#if 1
fprintf (stderr, "\nnewstack: %p %p\n", (void*)PS, (void*)PS->Canonical);
fflush (stderr);
#endif
  return;
}

//
// Function: pool_alloca()
//
// Description:
//  This function is a replacement heap allocator for stack allocations which
//  have been promoted to the heap.
//
// Note:
//  This function is only used when the PAConvertUnsafeAllocas pass is used in
//  place of the ConvertUnsafeAllocas pass.
//
void *
pool_alloca (PoolTy * Pool, unsigned int NumBytes) {
  DISABLED_IN_PRODUCTION_VERSION;

  // The address of the allocated object
  void * retAddress;

  // Ensure that we're always allocating at least 1 byte.
  if (NumBytes == 0)
    NumBytes = 1;

  // Allocate memory from the function's single slab.
  assert (Pool->StackSlabs && "pool_alloca: No call to newstack!\n");
  globalTemp = ((StackSlab *)(Pool->StackSlabs))->allocate (NumBytes);

  //
  // Allocate and remap the object.
  //
  retAddress = globalTemp;

  //
  // Record information about this allocation in the global debugging
  // structure.
  // FIXME: Need to ensure MetaData is correct for debugging
  //
#if SC_DEBUGTOOL
  PDebugMetaData debugmetadataPtr;
  globalallocID++;
  debugmetadataPtr = createPtrMetaData (globalallocID,
                                        globalfreeID,
                                        __builtin_return_address(0),
                                        0,
                                        globalTemp);

  dummyPool.DPTree.insert( retAddress, (char*) retAddress + NumBytes - 1,
                     debugmetadataPtr);
#endif

  // Register the object in the splay tree.  Keep track of its debugging data
  // with the splay node tag so that we can quickly map shadow address back
  // to the canonical address.
  //
  // globalTemp is the canonical page address
  Pool->Objects.insert(retAddress, (char*) retAddress + NumBytes - 1);
    
  assert (retAddress && "pool_alloca(1): Returning NULL!\n");
  return retAddress;
}

void
pool_delstack (PoolTy * Pool) {
  StackSlab * PS = (StackSlab *)(Pool->StackSlabs);

  //
  // Get the canonical page corresponding to this slab.
  //
#if 1
fprintf (stderr, "delstack: %p\n", (void*)PS);
fflush (stderr);
#endif

  //
  // Remove the slab from the list.
  //
  PS->unlinkFromList();

  //
  // Deallocate all elements and add the slab into the set of free slabs.
  //
  PS->Canonical->addToList((StackSlab**)&Pool->FreeStackSlabs);

#if 1
  //
  // Make the stack page inaccessible.
  //
  ProtectShadowPage (PS, 1);
#endif

  return;
}

// SearchForContainingSlab - Do a brute force search through the list of
// allocated slabs for the node in question.
//
static PoolSlab *
SearchForContainingSlab(PoolTy *Pool, void *Node, unsigned &TheIndex) {
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
// Function: _barebone_poolcheck()
//
// Description:
//  Perform an accurate load/store check for the given pointer.  This function
//  encapsulates the logic necessary to do the check.
//
// Return value:
//  true  - The pointer was found within a valid object within the pool.
//  false - The pointer was not found within a valid object within the pool.
//
static inline bool
_barebone_poolcheck (PoolTy * Pool, void * Node) {
  void * S, * end;

  //
  // If the pool handle is NULL, return successful.
  //
  if (!Pool) return true;

  //
  // Look through the splay trees for an object in which the pointer points.
  //
  bool fs = Pool->Objects.find(Node, S, end);
  if ((fs) && (S <= Node) && (Node <= end)) {
    return true;
  }

  //
  // The node is not found or is not within bounds; fail!
  //
  return false;
}

void
poolcheck (PoolTy *Pool, void *Node) {
  if (_barebone_poolcheck (Pool, Node))
    return;

  //
  // Look for the object within the splay tree of external objects.
  //
  int fs = 0;
  void * S, *end;
	if (1) {
		S = Node;
		fs = ExternalObjects.find (Node, S, end);
		if ((fs) && (S <= Node) && (Node <= end)) {
			return;
		}
	}

  //
  // We cannot find the pointer anywhere!  Time to fail a load/store check!
  //
  ReportLoadStoreCheck (Node, __builtin_return_address(0), "<Unknown>", 0);

  return;
}

void
poolcheck_debug (PoolTy *Pool, void *Node, void * SourceFilep, unsigned lineno) {
  if (_barebone_poolcheck (Pool, Node))
    return;
 
  //
  // Look for the object within the splay tree of external objects.
  //
  int fs = 0;
  void * S, *end;
	if (1) {
		S = Node;
		fs = ExternalObjects.find (Node, S, end);
		if ((fs) && (S <= Node) && (Node <= end)) {
			return;
		}
	}

  //
  // If it's a rewrite pointer, convert it back into its original value so
  // that we can print the real faulting address.
  //
  if (isRewritePtr (Node)) {
    Node = pchk_getActualValue (Pool, Node);
  }

  ReportLoadStoreCheck (Node,
                        __builtin_return_address(0),
                        (char *)SourceFilep,
                        lineno);
  return;
}

void
poolcheckui (PoolTy *Pool, void *Node) {
  if (_barebone_poolcheck (Pool, Node))
    return;

  //
  // Look for the object within the splay tree of external objects.
  //
  int fs = 0;
  void * S, *end;
	if (ConfigData.TrackExternalMallocs) {
		S = Node;
		fs = ExternalObjects.find (Node, S, end);
		if ((fs) && (S <= Node) && (Node <= end)) {
			return;
		}
	}

  //
  // The node is not found or is not within bounds.  Report a warning but keep
  // going.
  //
  fprintf (stderr, "PoolcheckUI failed(%p:%x): %p %p from %p\n", 
      (void*)Pool, fs, (void*)Node, end, __builtin_return_address(0));
  fflush (stderr);
  return;
}

//
// Function: boundscheck_lookup()
//
// Description:
//  Perform the lookup for a bounds check.
//
// Inputs:
//  Source - The pointer to look up within the set of valid objects.
//
// Outputs:
//  Source - If the object is found within the pool, this is the address of the
//           first valid byte of the object.
//
//  End    - If the object is found within the pool, this is the address of the
//           last valid byte of the object.
//
// Return value:
//  Returns true if the object is found.
//
bool 
boundscheck_lookup (PoolTy * Pool, void * & Source, void * & End ) {
  // Search for object for Source in splay tree, return length 
  return Pool->Objects.find(Source, Source, End);
}

//
// Function: boundscheck_check()
//
// Description:
//  This is the slow path for a boundscheck() and boundcheckui() calls.
//
// Inputs:
//  ObjStart - The address of the first valid byte of the object.
//  ObjEnd   - The address of the last valid byte of the object.
//  Pool     - The pool in which the pointer belong.
//  Source   - The source pointer used in the indexing operation (the GEP).
//  Dest     - The result pointer of the indexing operation (the GEP).
//  CanFail  - Flags whether the check can fail (for complete DSNodes).
//
// Note:
//  If ObjLen is zero, then the lookup says that Source was not found within
//  any valid object.
//
void *
boundscheck_check (bool found, void * ObjStart, void * ObjEnd, PoolTy * Pool,
                   void * Source, void * Dest, bool CanFail,
                   void * SourceFile, unsigned int lineno) {
  //
  // Determine if this is a rewrite pointer that is being indexed.  If so,
  // compute the original value, re-do the indexing operation, and rewrite the
  // value back.
  //
  if (isRewritePtr (Source)) {
    //
    // Get the real pointer value (which is outside the bounds of a valid
    // object.
    //
    void * RealSrc = pchk_getActualValue (Pool, Source);

    //
    // Compute the real result pointer (the value the GEP would really have on
    // the original pointer value).
    //
    Dest = (void *)((unsigned) RealSrc + ((unsigned) Dest - (unsigned) Source));

    //
    // Retrieve the original bounds of the object.
    //
    ObjStart = RewrittenObjs[Source].first;
    ObjEnd   = RewrittenObjs[Source].second;

    //
    // Redo the bounds check.  If it succeeds, return the real value.
    // Otherwise, just continue on with the rest of the failed bounds check
    // processing as before.
    //
    if (__builtin_expect (((ObjStart <= Dest) && ((Dest <= ObjEnd))), 1))
      return Dest;

    //
    // Pretend this was an index off of the original out of bounds pointer
    // value and continue processing
    //
    if (logregs) {
      fprintf (stderr, "unrewrite: (0x%x) -> (0x%x, 0x%x) \n", Source, RealSrc, Dest);
      fflush (stderr);
    }

    found = true;
    Source = RealSrc;
  }

  //
  // Now, we know that the pointer is out of bounds.  If we indexed off the
  // beginning or end of a valid object, determine if we can rewrite the
  // pointer into an OOB pointer.  Whether we can or not depends upon the
  // SAFECode configuration.
  //
  if (found) {
    if ((ConfigData.StrictIndexing == false) ||
        (((char *) Dest) == (((char *)ObjEnd)+1))) {
      void * ptr = rewrite_ptr (Pool, Dest, ObjStart, ObjEnd, SourceFile, lineno);
      if (logregs) {
        fprintf (ReportLog, "boundscheck: rewrite(1): %p %p %p %p at pc=%p to %p at %s (%d)\n",
                 ObjStart, ObjEnd, Source, Dest, (void*)__builtin_return_address(1), ptr, SourceFile, lineno);
        fflush (ReportLog);
      }
      return ptr;
    } else {
      unsigned allocPC = 0;
      unsigned allocID = 0;
      unsigned char * allocSF = (unsigned char *) "<Unknown>";
      unsigned allocLN = 0;
#if SC_DEBUGTOOL
      PDebugMetaData debugmetadataptr;
      unsigned freeID = 0;
      void * S , * end;
      if (dummyPool.DPTree.find(ObjStart, S, end, debugmetadataptr)) {
        allocPC = ((unsigned) (debugmetadataptr->allocPC)) - 5;
        allocID  = debugmetadataptr->allocID;
        allocSF  = (unsigned char *) debugmetadataptr->SourceFile;
        allocLN  = debugmetadataptr->lineno;
      }
#endif
      ReportBoundsCheck ((uintptr_t)Source,
                         (uintptr_t)Dest,
                         (unsigned)allocID,
                         (unsigned)allocPC,
                         (uintptr_t)__builtin_return_address(1),
                         (uintptr_t)ObjStart,
                         (unsigned)((char*) ObjEnd - (char*)(ObjStart)) + 1,
                         (unsigned char *)(SourceFile),
                         lineno,
                         allocSF,
                         allocLN);
      return Dest;
    }
  }

  /*
   * Allow pointers to the first page in memory provided that they remain
   * within that page.  Loads and stores using such pointers will fault.  This
   * allows indexing of NULL pointers without error.
   */
  if ((((unsigned char *)0) <= Source) && (Source < (unsigned char *)(4096))) {
    if ((((unsigned char *)0) <= Dest) && (Dest < (unsigned char *)(4096))) {
      if (logregs) {
        fprintf (ReportLog, "boundscheck: NULL Index: %p %p %p %p at pc=%p at %s (%d)\n",
                 0, 4096, Source, Dest, (void*)__builtin_return_address(1), SourceFile, lineno);
        fflush (ReportLog);
      }
      return Dest;
    } else {
      if ((ConfigData.StrictIndexing == false) ||
          (((uintptr_t) Dest) == 4096)) {
        if (logregs) {
          fprintf (ReportLog, "boundscheck: rewrite(3): %p %p %p %p at pc=%p at %s (%d)\n",
                   0, 4096, Source, Dest, (void*)__builtin_return_address(1), SourceFile, lineno);
          fflush (ReportLog);
        }
        return rewrite_ptr (Pool,
                            Dest,
                            (void *)0,
                            (void *)4096,
                            SourceFile,
                            lineno);
      } else {
        ReportBoundsCheck ((uintptr_t)Source,
                           (uintptr_t)Dest,
                           (unsigned)0,
                           (unsigned)0,
                           (uintptr_t)__builtin_return_address(1),
                           (unsigned)0,
                           (unsigned)4096,
                           (unsigned char *)(SourceFile),
                           lineno,
                           (unsigned char *) "<Unknown>",
                           0);
      }
    }
  }

  //
  // Attempt to look for the object in the external object splay tree.
  // Do this even if we're not tracking external allocations because a few
  // other objects without associated pools (e.g., argv pointers) may be
  // registered in here.
  //
  if (1) {
    void * S, * end;
    bool fs = ExternalObjects.find(Source, S, end);
    if (fs) {
      if ((S <= Dest) && (Dest <= end)) {
        return Dest;
      } else {
        if ((ConfigData.StrictIndexing == false) ||
            (((char *) Dest) == (((char *)end)+1))) {
          void * ptr = rewrite_ptr (Pool, Dest, S, end, SourceFile, lineno);
          if (logregs)
            fprintf (ReportLog,
                     "boundscheck: rewrite(2): %p %p %p %p at pc=%p to %p at %s (%d)\n",
                     S, end, Source, Dest, (void*)__builtin_return_address(1),
                     ptr, SourceFile, lineno);
          fflush (ReportLog);
          return ptr;
        }

        ReportBoundsCheck ((uintptr_t)Source,
                           (uintptr_t)Dest,
                           (unsigned)0,
                           (unsigned)0,
                           (uintptr_t)__builtin_return_address(1),
                           (uintptr_t)S,
                           (unsigned)((char*) end - (char*)(S)) + 1,
                           (unsigned char *)(SourceFile),
                           lineno,
                           (unsigned char *) "<Unknown>",
                           0);
      }
    }
  }

  //
  // We cannot find the object.  Continue execution.
  //
  if (CanFail) {
    ReportBoundsCheck ((uintptr_t)Source,
                       (uintptr_t)Dest,
                       (unsigned)0,
                       (unsigned)0,
                       (uintptr_t)__builtin_return_address(1),
                       (uintptr_t)0,
                       (unsigned)0,
                       (unsigned char *)(SourceFile),
                       lineno,
                       (unsigned char *) "<Unknown>",
                       0);
  }

  return Dest;
}

//
// Function: boundscheck()
//
// Description:
//  Perform a precise bounds check.  Ensure that Source is within a valid
//  object within the pool and that Dest is within the bounds of the same
//  object.
//
void *
boundscheck (PoolTy * Pool, void * Source, void * Dest) {
  // This code is inlined at all boundscheck() calls

  // Search the splay for Source and return the bounds of the object
  void * ObjStart = Source, * ObjEnd;
  bool ret = boundscheck_lookup (Pool, ObjStart, ObjEnd); 

  // Check if destination lies in the same object
  if (__builtin_expect ((ret && (ObjStart <= Dest) &&
                        ((Dest <= ObjEnd))), 1)) {
    return Dest;
  } else {
    //
    // Either:
    //  1) A valid object was not found in splay tree, or
    //  2) Dest is not within the valid object in which Source was found
    //
    return boundscheck_check (ret, ObjStart, ObjEnd, Pool, Source, Dest, true, NULL, 0);  
  }
}

//
// Function: boundscheck_debug()
//
// Description:
//  Identical to boundscheck() except that it takes additional debug info
//  parameters.
//
void *
boundscheck_debug (PoolTy * Pool, void * Source, void * Dest, void * SourceFile, unsigned lineno) {
  // This code is inlined at all boundscheck() calls

  // Search the splay for Source and return the bounds of the object
  void * ObjStart = Source, * ObjEnd;
  bool ret = boundscheck_lookup (Pool, ObjStart, ObjEnd); 

  // Check if destination lies in the same object
  if (__builtin_expect ((ret && (ObjStart <= Dest) &&
                        ((Dest <= ObjEnd))), 1)) {
    return Dest;
  } else {
    //
    // Either:
    //  1) A valid object was not found in splay tree, or
    //  2) Dest is not within the valid object in which Source was found
    //
    return boundscheck_check (ret, ObjStart, ObjEnd, Pool, Source, Dest, true, SourceFile, lineno);  
  }
}

//
// Function: boundscheckui()
//
// Description:
//  Perform a bounds check (with lookup) on the given pointers.
//
// Inputs:
//  Pool - The pool to which the pointers (Source and Dest) should belong.
//  Source - The Source pointer of the indexing operation (the GEP).
//  Dest   - The result of the indexing operation (the GEP).
//
void *
boundscheckui (PoolTy * Pool, void * Source, void * Dest) {
  // This code is inlined at all boundscheckui calls

  // Search the splay for Source and return the bounds of the object
  void * ObjStart = Source, * ObjEnd;
  bool ret = boundscheck_lookup (Pool, ObjStart, ObjEnd); 

  // Check if destination lies in the same object
  if (__builtin_expect ((ret && (ObjStart <= Dest) &&
                        ((Dest <= ObjEnd))), 1)) {
    return Dest;
  } else {
    //
    // Either:
    //  1) A valid object was not found in splay tree, or
    //  2) Dest is not within the valid object in which Source was found
    //
    return boundscheck_check (ret, ObjStart, ObjEnd, Pool, Source, Dest, false, NULL, 0);
  }
}

//
// Function: boundscheckui_debug()
//
// Description:
//  Identical to boundscheckui() but with debug information.
//
// Inputs:
//  Pool       - The pool to which the pointers (Source and Dest) should
//               belong.
//  Source     - The Source pointer of the indexing operation (the GEP).
//  Dest       - The result of the indexing operation (the GEP).
//  SourceFile - The source file in which the check was inserted.
//  lineno     - The line number of the instruction for which the check was
//               inserted.
//
void *
boundscheckui_debug (PoolTy * Pool,
                     void * Source,
                     void * Dest,
                     void * SourceFile,
                     unsigned int lineno) {
  // This code is inlined at all boundscheckui calls

  // Search the splay for Source and return the bounds of the object
  void * ObjStart = Source, * ObjEnd;
  bool ret = boundscheck_lookup (Pool, ObjStart, ObjEnd); 

  // Check if destination lies in the same object
  if (__builtin_expect ((ret && (ObjStart <= Dest) &&
                        ((Dest <= ObjEnd))), 1)) {
    return Dest;
  } else {
    //
    // Either:
    //  1) A valid object was not found in splay tree, or
    //  2) Dest is not within the valid object in which Source was found
    //
    return boundscheck_check (ret,
                              ObjStart,
                              ObjEnd,
                              Pool,
                              Source,
                              Dest,
                              false,
                              SourceFile,
                              lineno);
  }
}

//
// Function: rewrite_ptr()
//
// Description:
//  Take the given pointer and rewrite it to an Out Of Bounds (OOB) pointer.
//
// Inputs:
//  Pool       - The pool in which the pointer should be located (but isn't).
//               This value can be NULL if the caller doesn't know the pool.
//  p          - The pointer that needs to be rewritten.
//  ObjStart   - The address of the first valid byte of the object.
//  ObjEnd     - The address of the last valid byte of the object.
//  SourceFile - The name of the source file in which the check requesting the
//               rewrite is located.
//  lineno     - The line number within the source file in which the check
//               requesting the rewrite is located.
//
void *
rewrite_ptr (PoolTy * Pool,
             void * p,
             void * ObjStart,
             void * ObjEnd,
             void * SourceFile,
             unsigned lineno) {
#if SC_DEBUGTOOL
  //
  // If this pointer has already been rewritten, do not rewrite it again.
  //
  if (RewrittenPointers.find (p) != RewrittenPointers.end()) {
    return RewrittenPointers[p];
  }
#endif

#if SC_ENABLE_OOB
  //
  // Calculate a new rewrite pointer.
  //
  if (invalidptr == 0) invalidptr = (unsigned char*)InvalidLower;
  ++invalidptr;

  //
  // Ensure that we haven't run out of rewrite pointers.
  //
  void* P = invalidptr;
  if ((uintptr_t) P == InvalidUpper) {
    fprintf (stderr, "rewrite: out of rewrite ptrs: %x %x, pc=%x\n",
             InvalidLower, InvalidUpper, invalidptr);
    fflush (stderr);
    return p;
  }

  //
  // If no pool was specified (as is the case for an ExactCheck), use a
  // special Out of Bounds Pointer pool.
  //
  if (!Pool) Pool = &OOBPool;

  //
  // Determine if this pointer value has already been rewritten.  If so, just
  // return the previously rewritten value.  Otherwise, insert a mapping from
  // rewrite pointer to original pointer into the pool.
  //
  Pool->OOB.insert (invalidptr, ((unsigned char *)(invalidptr)), p);
#if SC_DEBUGTOOL
  //
  // If debugging tool support is enabled, then insert it into the global
  // OOB pool as well; this will ensure that we can find the pointer on a
  // memory protection violation (on faults, we don't have Pool handle
  // information).
  //
  if (logregs) {
    extern FILE * ReportLog;
    fprintf (ReportLog, "rewrite: %p: %p -> %p\n", Pool, p, invalidptr);
    fflush (ReportLog);
  }

  OOBPool.OOB.insert (invalidptr, ((unsigned char *)(invalidptr)), p);
  RewriteSourcefile[invalidptr] = SourceFile;
  RewriteLineno[invalidptr] = lineno;
  RewrittenPointers[p] = invalidptr;
  RewrittenObjs[invalidptr] = std::make_pair(ObjStart, ObjEnd);

#if 0
  //
  // Record the mapping between the start of an object and the set of rewrite
  // pointers created from it.  This is used to invalidate the rewrite pointers
  // when the object is freed.
  //
  if (RewrittenStart.find (ObjStart) != RewrittenStart.end ()) {
    RewrittenStart[ObjStart].push_back (invalidptr);
  } else {
    std::vector<void*> v;
    v.push_back(invalidptr);
    RewrittenStart[ObjStart] = v;
  }
#endif
#endif
  return invalidptr;
#else
  return p;
#endif
}

//
// Function: getActualValue()
//
// Description:
//  If src is an out of object pointer, get the original value.
//
void *
pchk_getActualValue (PoolTy * Pool, void * p) {
#if SC_DEBUGTOOL
  //
  // If the pointer is not within the rewrite pointer range, then it is not a
  // rewritten pointer.  Simply return its current value.
  //
  if (((uintptr_t)p <= InvalidLower) || ((uintptr_t)p >= InvalidUpper)) {
    return p;
  }

  void * src = 0;
  void* tag = 0;
  void * end = 0;

  //
  // Look for the pointer in the pool's OOB pointer list.  If we find it,
  // return its actual value.
  //
  if (Pool->OOB.find(p, src, end, tag)) {
    if (logregs) {
      fprintf (ReportLog, "getActualValue(1): %x: %x -> %x\n", Pool, p, tag);
      fflush (ReportLog);
    }
    return tag;
  }

  //
  // If we can't find the pointer in the pool's OOB list, perhaps it's in the
  // global OOB Pool (this can happen when it's rewritten by an exact check).
  //
  if (OOBPool.OOB.find (p, src, end, tag)) {
    if (logregs) {
      fprintf (ReportLog, "getActualValue(2): %x: %x -> %x\n", &OOBPool, p, tag);
      fflush (ReportLog);
    }
    return tag;
  }

  //
  // If we can't find the pointer, no worries.  If the program tries to use the
  // pointer, another SAFECode checks should flag a failure.  In this case,
  // just return the pointer.
  //
  if (logregs) {
    fprintf (ReportLog, "getActualValue(3): %x: %x -> %x\n", Pool, p, p);
    fflush (ReportLog);
  }
  return p;
#else
  // The function should be disabled at runtime
  assert (0 && "This function should be disabled at runtime!"); 
#endif
}

#if 0
// Check that Node falls within the pool and within start and (including)
// end offset
void
poolcheckalign (PoolTy *Pool, void *Node, unsigned StartOffset, 
                unsigned EndOffset) {
  PoolSlab *PS;
  if (StartOffset >= Pool->NodeSize || EndOffset >= Pool->NodeSize) {
    printf("Error: Offset specified exceeded node size");
    exit(-1);
  }

  if (Pool->AllocadPool > 0) {
    if (Pool->allocaptr <= Node) {
     unsigned diffPtr = (unsigned)Node - (unsigned)Pool->allocaptr;
     unsigned offset = diffPtr % Pool->NodeSize;
     if ((diffPtr  < (unsigned)Pool->AllocadPool ) && (offset >= StartOffset) &&
         (offset <= EndOffset))
       return;
    }
    assert(0 && "poolcheckalign failure FAILING \n");
    exit(-1);    
  }
  
  PS = (PoolSlab*)((long)Node & ~(PageSize-1));

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
            break;
          }
          PSlab = PSlab->Next;
        }

        if (Idx == -1) {
          fprintf(stderr, "poolcheck1: node being checked not found in pool with right"
           " alignment\n");
          fflush(stderr);
      abort();
          exit(-1);
        } else {
          //exit(-1);
        }
      } else {
        fprintf(stderr, "poolcheck2: node being checked not found in pool with right"
               " alignment\n");
        fflush(stderr);
    abort();
        exit(-1);
      }
    } else {
      unsigned long startaddr = (unsigned long)PS->getElementAddress(0,0);
      if (startaddr > (unsigned long) Node) {
        fprintf(stderr, "poolcheck: node being checked points to meta-data \n");
        fflush(stderr);
    abort();
        exit(-1);
      }
      unsigned long offset = ((unsigned long) Node - (unsigned long) startaddr) % Pool->NodeSize;
      if ((offset < StartOffset) || (offset > EndOffset)) {
        fprintf(stderr, "poolcheck3: node being checked does not have right alignment\n");
        fflush(stderr);
    abort();
        exit(-1);
      }
      Pool->prevPage[Pool->lastUsed] = PS;
      Pool->lastUsed = (Pool->lastUsed + 1) % 4;
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
      if ((offset < StartOffset) || (offset > EndOffset)) {
        fprintf(stderr, "poolcheck4: node being checked does not have right alignment\n");
        fflush(stderr);
    abort();
        exit(-1);
      }
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
          fprintf(stderr, "poolcheck6: node being checked not found in pool with right"
           " alignment\n");
          fflush(stderr);
      abort();
          exit(-1);
        }
      } else {
        fprintf(stderr, "poolcheck5: node being checked not found in pool with right"
               " alignment %x %x\n", (unsigned)Pool, (unsigned)Node);
        fflush(stderr);
    abort();
      }
    }
  }
}
#else
//
// Function: poolcheckalign()
//
// Description:
//  Ensure that the given pointer is both within an object in the pool *and*
//  points to the correct offset within the pool.
//
// Inputs:
//  Pool   - The pool in which the pointer should be found.
//  Node   - The pointer to check.
//  Offset - The offset, in bytes, that the pointer should be to the beginning
//           of objects found in the pool.
//
void
poolcheckalign (PoolTy *Pool, void *Node, unsigned Offset) {
  //
  // Let null pointers go if the alignment is zero; such pointers are aligned.
  //
  if ((Node == 0) && (Offset == 0))
    return;

  //
  // If no pool was specified, return.
  //
  if (!Pool) return;

  //
  // Look for the object in the splay of regular objects.
  //
  void * S, * end;
  bool t = Pool->Objects.find(Node, S, end);

  if ((t) && (((unsigned char *)Node - (unsigned char *)S) == (int)Offset)) {
      return;
  }

  /*
   * The object has not been found.  Provide an error.
   */
  ReportLoadStoreCheck (Node, __builtin_return_address(0), "<Unknown>", 0);
}

//
// Function: poolcheckalign_debug()
//
// Description:
//  Identical to poolcheckalign() but with additional debug info parameters.
//
// Inputs:
//  Pool   - The pool in which the pointer should be found.
//  Node   - The pointer to check.
//  Offset - The offset, in bytes, that the pointer should be to the beginning
//           of objects found in the pool.
//
// FIXME:
//  For now, this does nothing, but it should, in fact, do a run-time check.
//
void
poolcheckalign_debug (PoolTy *Pool, void *Node, unsigned Offset, void * SourceFile, unsigned lineno) {
  //
  // Let null pointers go if the alignment is zero; such pointers are aligned.
  //
  if ((Node == 0) && (Offset == 0))
    return;

  //
  // If no pool was specified, return.
  //
  if (!Pool) return;

  //
  // Look for the object in the splay of regular objects.
  //
  void * S, * end;
  bool t = Pool->Objects.find(Node, S, end);

  if ((t) && (((unsigned char *)Node - (unsigned char *)S) == (int)Offset)) {
      return;
  }

  //
  // The object has not been found.  Provide an error.
  //
  ReportLoadStoreCheck (Node, __builtin_return_address(0), (char *)SourceFile, lineno);
}

#endif

//
// Function: poolfree()
//
// Description:
//  Mark the object specified by the given pointer as free and available for
//  allocation for new objects.
//
// Inputs:
//  Pool - The pool to which the pointer should belong.
//  Node - A pointer to the beginning of the object to free.  For dangling
//         pointer detection, this is a pointer to the shadow page.
//
// Notes:
//  This routine should be resistent to several types of deallocation errors:
//    o) Deallocating an object which does not exist within the pool.
//    o) Deallocating an already-free object.
//
void
poolfree(PoolTy *Pool, void *Node) {
  DISABLED_IN_PRODUCTION_VERSION;
  assert(Pool && "Null pool pointer passed in to poolfree!\n");
  PoolSlab *PS;
  int Idx;
  
  if (logregs) {
    fprintf(stderr, "poolfree: 1368: Pool=%p, addr=%p\n", Pool, Node);
    fflush (stderr);
  }

  // Canonical pointer for the pointer we're freeing
  void * CanonNode = Node;

  if (1) {                  // THIS SHOULD BE SET FOR SAFECODE!
    unsigned TheIndex;
    PS = SearchForContainingSlab(Pool, CanonNode, TheIndex);
    Idx = TheIndex;
    assert (PS && "poolfree: No poolslab found for object!\n");
    PS->freeElement(Idx);
  } else {
    // Since it is undefined behavior to free a node which has not been
    // allocated, we know that the pointer coming in has to be a valid node
    // pointer in the pool.  Mask off some bits of the address to find the base
    // of the pool.
    assert((PageSize & (PageSize-1)) == 0 && "Page size is not a power of 2??");
    PS = (PoolSlab*)((long)Node & ~(PageSize-1));

    if (PS->isSingleArray) {
      PS->unlinkFromList();
      return;
    }

    Idx = PS->containsElement(Node, Pool->NodeSize);
    assert((int)Idx != -1 && "Node not contained in slab??");
  }

#if SC_DEBUGTOOL
  //
  // Ensure that the pointer is valid; if not, warn the user.
  //
  assert (PS && "PS is NULL!\n");
#else
  //
  // If we could not find the slab in which the node belongs, then we were
  // passed an invalid pointer.  Simply ignore it.
  //
  if (!PS) return;
#endif
  
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
 
#if SC_DEBUGTOOL
  //
  // FIXME: The code to mark the shadow page as read-only needs to occur in
  //        poolunregister().  However, that, in turn, requires that
  //        poolunregister() be able to differentiate between stack objects
  //        (which should not be marked inaccessible) and heap objects (which
  //        should be marked inaccessible).
  //  
#if 0
  // Protect the shadow pages
  ProtectShadowPage((void *)((long)Node & ~(PPageSize - 1)), NumPPage);
#endif
  
  //
  // An object has been freed.  Set up a signal handler to catch any dangling
  // pointer references.
  //
  // FIXME:
  //  This code was placed here because it does not appear to work when placed
  //  in poolinit().
  //
  struct sigaction sa;
  bzero (&sa, sizeof (struct sigaction));
  sa.sa_sigaction = bus_error_handler;
  sa.sa_flags = SA_SIGINFO;
  if (sigaction(SIGBUS, &sa, NULL) == -1) {
    fprintf (stderr, "sigaction installer failed!");
    fflush (stderr);
  }
  if (sigaction(SIGSEGV, &sa, NULL) == -1) {
    fprintf (stderr, "sigaction installer failed!");
    fflush (stderr);
  }
#endif

  return; 
}

//===----------------------------------------------------------------------===//
//
// Dangling pointer runtime functions
//
//===----------------------------------------------------------------------===//

//
// Function: createPtrMetaData()
//  Allocates memory for a DebugMetaData struct and fills up the appropriate
//  fields so to keep a record of the pointer's meta data
//
// Inputs:
//  AllocID - A unique identifier for the allocation.
//  FreeID  - A unique identifier for the deallocation.
//  AllocPC - The program counter at which the object was allocated.
//  FreePC  - The program counter at which the object was freed.
//  Canon   - The canonical address of the memory object.
//
#if SC_DEBUGTOOL
static PDebugMetaData
createPtrMetaData (unsigned AllocID,
                   unsigned FreeID,
                   void * AllocPC,
                   void * FreePC,
                   void * Canon,
                   char * SourceFile,
                   unsigned lineno) {
  // FIXME:
  //  This will cause an allocation that is registered as an external
  //  allocation.  We need to use some internal allocation routine.
  //
  PDebugMetaData ret = (PDebugMetaData) malloc (sizeof(DebugMetaData));
  ret->allocID = AllocID;
  ret->freeID = FreeID;
  ret->allocPC = AllocPC;
  ret->freePC = FreePC;
  ret->canonAddr = Canon;
  ret->SourceFile = SourceFile;
  ret->lineno = lineno;

  return ret;
}

static inline void
updatePtrMetaData (PDebugMetaData debugmetadataptr,
                   unsigned globalfreeID,
                   void * paramFreePC) {
  debugmetadataptr->freeID = globalfreeID;
  debugmetadataptr->freePC = paramFreePC;
  return;
}
#endif

#if SC_DEBUGTOOL

//
// Function: bus_error_handler()
//
// Description:
//  This is the signal handler that catches bad memory references.
//
static void
bus_error_handler (int sig, siginfo_t * info, void * context) {
  signal(SIGBUS, NULL);

  // Cast parameters to the desired type
  ucontext_t * mycontext = (ucontext_t *) context;

  //
  // Get the address causing the fault.
  //
  void * faultAddr = info->si_addr, *end;
  PDebugMetaData debugmetadataptr;
  int fs = 0;

#if SC_DEBUGTOOL
  //
  // Attempt to look up dangling pointer information for the faulting pointer.
  //
  fs = dummyPool.DPTree.find (info->si_addr, faultAddr, end, debugmetadataptr);

  //
  // If there is no dangling pointer information for the faulting pointer,
  // perhaps it is an Out of Bounds Rewrite Pointer.  Check for that now.
  //
  if (0 == fs) {
    unsigned program_counter = 0;
#if defined(__APPLE__)
#if defined(i386) || defined(__i386__) || defined(__x86__)
    program_counter = mycontext->uc_mcontext->__ss.__eip;
#endif
#endif
    extern FILE * ReportLog;
    void * start = faultAddr;
    void * tag = 0;
    void * end;
#if SC_ENABLE_OOB
    if (OOBPool.OOB.find (faultAddr, start, end, tag)) {
      char * Filename = (char *)(RewriteSourcefile[faultAddr]);
      unsigned lineno = RewriteLineno[faultAddr];
      ReportOOBPointer (program_counter,
                        tag,
                        faultAddr,
                        RewrittenObjs[faultAddr].first,
                        RewrittenObjs[faultAddr].second,
                        Filename,
                        lineno);
      abort();
    }
#endif
    extern FILE * ReportLog;
    fprintf(ReportLog, "signal handler: no debug meta data for %x", faultAddr);
    fflush(ReportLog);
    abort();
  }
#endif
 
  // FIXME: Correct the semantics for calculating NumPPage 
  unsigned NumPPage;
  unsigned offset = (unsigned) ((long)info->si_addr & (PPageSize - 1) );
  unsigned int len = (unsigned char *)(end) - (unsigned char *)(faultAddr) + 1;
  NumPPage = (len / PPageSize) + 1;
  if ( (len - (NumPPage-1) * PPageSize) > (PPageSize - offset) )
    NumPPage++;
 
  // This is necessary so that the program continues execution,
  //  especially in debugging mode 
  UnprotectShadowPage((void *)((long)info->si_addr & ~(PPageSize - 1)), NumPPage);
  
  //void* S = info->si_addr;
  // printing reports
  void * address = 0;
  unsigned program_counter = 0;
  unsigned alloc_pc = 0;
  unsigned free_pc = 0;
  unsigned allocID = 0;
  unsigned freeID = 0;

#if defined(__APPLE__)
#if defined(i386) || defined(__i386__) || defined(__x86__)
  program_counter = mycontext->uc_mcontext->__ss.__eip;
#endif
  alloc_pc = ((unsigned) (debugmetadataptr->allocPC)) - 5;
  free_pc  = ((unsigned) (debugmetadataptr->freePC)) - 5;
  allocID  = debugmetadataptr->allocID;
  freeID   = debugmetadataptr->freeID;
#endif
  
  ReportDanglingPointer (address, program_counter,
                         alloc_pc, allocID,
                         free_pc, freeID);

  // reinstall the signal handler for subsequent faults
  struct sigaction sa;
  sa.sa_sigaction = bus_error_handler;
  sa.sa_flags = SA_SIGINFO;
  if (sigaction(SIGBUS, &sa, NULL) == -1)
    printf("sigaction installer failed!");
  if (sigaction(SIGSEGV, &sa, NULL) == -1)
    printf("sigaction installer failed!");
  
  return;
}

#endif
 

//
// Function: funccheck()
//
// Description:
//  Determine whether the specified function pointer is one of the functions
//  in the given list.
//
// Inputs:
//  num - The number of function targets in the DSNode.
//  f   - The function pointer that we are testing.
//  g   - The first function given in the DSNode.
//
void
funccheck (unsigned num, void *f, void *g, ...) {
  va_list ap;
  unsigned i = 0;

  // Test against the first function in the list
  if (f == g) return;
  i++;
  va_start(ap, g);
  for ( ; i != num; ++i) {
    void *h = va_arg(ap, void *);
    if (f == h) {
      return;
    }
  }
  if (logregs) {
  fprintf(stderr, "funccheck failed(num=%d): %p %p\n", num, f, g);
  fflush(stderr);
  }
  abort();
}

void
poolstats() {
  fprintf (stderr, "pool mem usage %d\n", poolmemusage);
  fflush (stderr);
}

/// NEW CODES START HERE

static void * __barebone_poolallocarray(PoolTy* Pool, unsigned Size);

/// Barebone pool alloc
/// Barebone pool alloc only deals with the allocation
/// it does not handle stuffs like splay trees and remapping
/// these can be done by wrappers.

void *
__barebone_poolalloc(PoolTy *Pool, unsigned NumBytes) {
  void *retAddress = NULL;
  assert(Pool && "Null pool pointer passed into poolalloc!\n");
   
  // Ensure that we're always allocating at least 1 byte.
  if (NumBytes == 0)
    NumBytes = 1;

  unsigned NodeSize = Pool->NodeSize;
  unsigned NodesToAllocate = (NumBytes + NodeSize - 1)/NodeSize;
  unsigned offset = 0;
  
  // Call a helper function if we need to allocate more than 1 node.
  if (NodesToAllocate > 1) {
    retAddress = __barebone_poolallocarray(Pool, NodesToAllocate);
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
      
      globalTemp = PS->getElementAddress(Element, NodeSize);
      assert (globalTemp && "poolalloc(2): Returning NULL!\n");
      return globalTemp;
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
        
        globalTemp = PS->getElementAddress(Element, NodeSize);
        offset = (uintptr_t)globalTemp & (PPageSize - 1);

        assert (globalTemp && "poolalloc(3): Returning NULL!\n");
        return globalTemp;
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
  
  if (Pool->NumSlabs > AddrArrSize)
    Pool->Slabs->insert((void *)New);
  else if (Pool->NumSlabs == AddrArrSize) {
    // Create the hash_set
    Pool->Slabs = new hash_set<void *>;
    Pool->Slabs->insert((void *)New);
    for (unsigned i = 0; i < AddrArrSize; ++i)
      Pool->Slabs->insert((void *)Pool->SlabAddressArray[i]);
  }
  else {
    // Insert it in the array
    Pool->SlabAddressArray[Pool->NumSlabs] =  New;
  }
  Pool->NumSlabs++;

  int Idx = New->allocateSingle();
  assert(Idx == 0 && "New allocation didn't return zero'th node?");
  globalTemp = New->getElementAddress(0, 0);
  offset = (uintptr_t)globalTemp & (PPageSize - 1);
  
  assert (globalTemp && "poolalloc(4): Returning NULL!\n");
  return globalTemp;
}

static void *
__barebone_poolallocarray(PoolTy* Pool, unsigned Size) {
  assert(Pool && "Null pool pointer passed into poolallocarray!\n");
  
  // check to see if we need to allocate a single large array
  if (Size > PoolSlab::getSlabSize(Pool)) {
    globalTemp = (PoolSlab*) PoolSlab::createSingleArray(Pool, Size);
    return (void*) (globalTemp);
  }
 
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
      
      // insert info into adl splay tree for poolcheck runtime
      //unsigned NodeSize = Pool->NodeSize;
      globalTemp = PS->getElementAddress(Element, Pool->NodeSize);
      return (void*) globalTemp;
    }
  }
  
  PoolSlab *New = PoolSlab::create(Pool);
  //  printf("new slab created %x \n", New);
  if (Pool->NumSlabs > AddrArrSize)
    Pool->Slabs->insert((void *)New);
  else if (Pool->NumSlabs == AddrArrSize) {
    // Create the hash_set
    Pool->Slabs = new hash_set<void *>;
    Pool->Slabs->insert((void *)New);
    for (unsigned i = 0; i < AddrArrSize; ++i)
      Pool->Slabs->insert((void *)Pool->SlabAddressArray[i]);
  }
  else {
    // Insert it in the array
    Pool->SlabAddressArray[Pool->NumSlabs] =  New;
  }
  
  Pool->NumSlabs++;
  
  int Idx = New->allocateMultiple(Size);
  assert(Idx == 0 && "New allocation didn't return zero'th node?");
  
  // insert info into adl splay tree for poolcheck runtime
  //unsigned NodeSize = Pool->NodeSize;
  globalTemp = New->getElementAddress(0, 0);
  //adl_splay_insert(&(Pool->Objects), globalTemp, 
  //          (unsigned)((Size*NodeSize) - NodeSize + 1), (Pool));
 
  return (void*) globalTemp;
}

void *
__barebone_pool_alloca(PoolTy * Pool, unsigned int NumBytes) {
  // The address of the allocated object
  void * retAddress;

  // Ensure that we're always allocating at least 1 byte.
  if (NumBytes == 0)
    NumBytes = 1;

  // Allocate memory from the function's single slab.
  assert (Pool->StackSlabs && "pool_alloca: No call to newstack!\n");
  globalTemp = ((StackSlab *)(Pool->StackSlabs))->allocate (NumBytes);

  //
  // Allocate and remap the object.
  //
  retAddress = globalTemp;

  assert (retAddress && "pool_alloca(1): Returning NULL!\n");
  return retAddress;
}

void
__barebone_poolfree(PoolTy *Pool, void *Node) {
  assert(Pool && "Null pool pointer passed in to poolfree!\n");
  PoolSlab *PS;
  int Idx;

  // Canonical pointer for the pointer we're freeing
  void * CanonNode = Node;

  if (1) {                  // THIS SHOULD BE SET FOR SAFECODE!
    unsigned TheIndex;
    PS = SearchForContainingSlab(Pool, CanonNode, TheIndex);
    Idx = TheIndex;
    assert (PS && "poolfree: No poolslab found for object!\n");
    PS->freeElement(Idx);
  }

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

void
__barebone_pooldestroy(PoolTy *Pool) { 
  assert(Pool && "Null pool pointer passed in to pooldestroy!\n");

  if (Pool->AllocadPool) return;

  // Remove all registered pools
#if 0
  delete Pool->RegNodes;
#endif

  if (Pool->NumSlabs > AddrArrSize) {
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

/// it seems that Mac OS doesn't support weak alias very well.
/// Use call instead, in fact, there is no performance penalty because
/// of inlining. 
/// __attribute__ ((weak, alias ("poolinit")));
void
__barebone_poolinit(PoolTy *Pool, unsigned NodeSize) {
	poolinit(Pool, NodeSize);
}
