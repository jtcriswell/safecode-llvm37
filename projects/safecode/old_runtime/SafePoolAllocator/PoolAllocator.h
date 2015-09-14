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

#include "safecode/Config/config.h"
#include "poolalloc/ADT/HashExtras.h"
#include "poolalloc_runtime/Support/SplayTree.h"

#include <stdarg.h>
#include <string.h>

#include <map>
#include <utility>

#define AddrArrSize 2

extern unsigned poolmemusage;
//unsigned PCheckPassed = 1;

//
// Structure: DebugMetaData
//
// Description:
//  This structure contains information on the error to be reported.
//
// Fields:
//  allocID    : The ID number of the allocation of the object.
//  freeID     : The ID number of the deallocation of the object.
//  allocPC    : The program counter at which the object was last allocated.
//  freePC     : The program counter at which the object was last deallocated.
//  canonAddr  : The canonical address of the memory reference.
//  SourceFile : A string containing the source file to which the erring
//               instruction is found.
//  lineno     : The line number in the source file to which the erring
//               instruction is found.
//
typedef struct DebugMetaData {
  unsigned allocID;
  unsigned freeID;
  void * allocPC;
  void * freePC;
  void * canonAddr;

  // Source filename
  void * SourceFile;

  // Line number
  unsigned lineno;
} DebugMetaData;
typedef DebugMetaData * PDebugMetaData;


typedef struct PoolTy {
  // Splay tree used for object registration
  RangeSplaySet<> Objects;

#if SC_ENABLE_OOB 
  // Splay tree used for out of bound objects
  RangeSplayMap<void *> OOB;
#endif

  // FIXME: should be dangling pointer macro
#if SC_DEBUGTOOL
  // Splay tree used by dangling pointer runtime
  RangeSplayMap<PDebugMetaData> DPTree;
#endif

  // Linked list of slabs used for stack allocations
  void * StackSlabs;

  // Linked list of slabs available for stack allocations
  void * FreeStackSlabs;

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
  void* SlabAddressArray[AddrArrSize];

  // The number of slabs allocated. Large arrays are not counted
  unsigned NumSlabs;

  // Large arrays. In SAFECode, these are currently not freed or reused. 
  // A better implementation could split them up into single slabs for reuse,
  // upon being freed.
  void *LargeArrays;
  void *FreeLargeArrays;

  void *prevPage[4];
  unsigned short lastUsed;

  // Interpretation of AllocadPool variable in original SAFECode:
  //  AllocadPool = -1 : unused so far
  //  AllocadPool 0    : used only for mallocs
  //  AllocadPool >0   : used for only allocas indicating the size 
  short AllocadPool;
  void *allocaptr;
#if 0
  std::map<void *,unsigned> * RegNodes;
#endif
} PoolTy;

extern "C" {
  void pool_init_runtime(unsigned Dangling,
                         unsigned RewriteOOB,
                         unsigned Terminate);
  void poolinit(PoolTy *Pool, unsigned NodeSize);
  void poolmakeunfreeable(PoolTy *Pool);
  void pooldestroy(PoolTy *Pool);
  void * poolalloc(PoolTy *Pool, unsigned NumBytes);
  void * poolrealloc(PoolTy *Pool, void *Node, unsigned NumBytes);
  void * poolcalloc (PoolTy *Pool, unsigned Number, unsigned NumBytes);
  void * poolstrdup(PoolTy *Pool, char *Node);

  void poolargvregister (int argc, char ** argv);
  void poolregister(PoolTy *Pool, void *allocaptr, unsigned NumBytes);
  void poolunregister(PoolTy *Pool, void *allocaptr);
  void pool_protect_object (void * Node);
  void poolfree(PoolTy *Pool, void *Node);
  void poolcheck(PoolTy *Pool, void *Node);
  void poolcheckui(PoolTy *Pool, void *Node);
  void poolcheckoptim(PoolTy *Pool, void *Node);
  void * boundscheck   (PoolTy * Pool, void * Source, void * Dest);
  int boundscheckui_lookup (PoolTy * Pool, void * Source) __attribute__ ((const, noinline));
  void * boundscheckui_check (int len, PoolTy * Pool, void * Source, void * Dest) __attribute__ ((noinline));
  void * boundscheckui (PoolTy * Pool, void * Source, void * Dest);
  void funccheck (unsigned num, void *f, void *g, ...);
  void poolstats(void);
  void poolcheckalign(PoolTy *Pool, void *Node, unsigned Offset);

  void pool_newstack (PoolTy * Pool);
  void pool_delstack (PoolTy * Pool);
  void * pool_alloca (PoolTy * Pool, unsigned int NumBytes);

  void * rewrite_ptr (PoolTy *, void *, void *, void *, void *, unsigned);

  void * poolalloc_debug (PoolTy *P, unsigned Size, void * SrcFle, unsigned no);
  void * poolcalloc_debug (PoolTy *P, unsigned Num, unsigned Size, void * S, unsigned no);
  void poolregister_debug (PoolTy *P, void * p, unsigned size, void * SF,
                           unsigned lineno);
  void   poolfree_debug (PoolTy *P, void * ptr, void * SrcFle, unsigned no);
  void   poolcheck_debug (PoolTy *P, void * Node, void * SrcFle, unsigned no);
  void   poolcheckalign_debug (PoolTy *P, void *Node, unsigned Offset, void * SourceFile, unsigned lineno);
  void * boundscheck_debug (PoolTy * P, void * S, void * D, void * SFile, unsigned int lineno);
  void * boundscheckui_debug (PoolTy * P, void * S, void * D, void * SFile, unsigned int lineno);
  void * pchk_getActualValue (PoolTy * Pool, void * src);

  //void protect_shadowpage();
 
  // Barebone allocators, which only do allocations
  // They should not be used directly.

  void __barebone_poolinit(PoolTy *Pool, unsigned NodeSize);
  void __barebone_pooldestroy(PoolTy *Pool);
  void __barebone_poolfree(PoolTy *Pool, void *Node);
  void * __barebone_poolalloc(PoolTy *Pool, unsigned NumBytes);
  void * __barebone_pool_alloca(PoolTy * Pool, unsigned int NumBytes);
}

/// Template class to implement
/// realloc, calloc, strdup based on a particular implementation 
/// of a pool allocator 
template <class AllocatorT>
class PoolAllocatorFacade {
  PoolAllocatorFacade();
  ~PoolAllocatorFacade();
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
