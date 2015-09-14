//===- PoolCheck.cpp - Implementation of poolallocator runtime -===//
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

#include "PoolCheck.h"
#ifdef LLVA_KERNEL
#include <stdarg.h>
#endif
#define DEBUG(x) 

//===----------------------------------------------------------------------===//
extern "C" {
extern unsigned PageSize;

void poolcheckinit(void *Pool, unsigned NodeSize) {
  //to be called from poolinit
  //do nothing for now. 
}

// poolcheckdestroy - to be called from pooldestroy
//
void poolcheckdestroy(void *Pool) {
  //Do nothing as of now since all MEtaPools are global 
  //  free_splay(Pool->splay);
}


void AddPoolDescToMetaPool(MetaPoolTy **MP, void *P) {
  MetaPoolTy  *MetaPoolPrev = *MP;
  MetaPoolTy *MetaPool = *MP;
  if (MetaPool) {
    MetaPool = MetaPool->next;
  } else {
    *MP = (MetaPoolTy *) poolcheckmalloc (sizeof(MetaPoolTy));
    (*MP)->Pool = P;
    (*MP)->next = 0;
    return;
  }
  while (MetaPool) {
    MetaPool = MetaPool->next;
    MetaPoolPrev = MetaPool;
  }
  //MetaPool is null;
  MetaPoolPrev->next = (MetaPoolTy *) poolcheckmalloc (sizeof(MetaPoolTy));
  MetaPoolPrev->next->Pool = P;
  MetaPoolPrev->next->next = 0;
}


bool poolcheckoptim(void *Pool, void *Node) {
  //PageSize needs to be modified accordingly
  void *PS = (void *)((unsigned)Node & ~(PageSize-1));
  PoolCheckSlab * PCS = poolcheckslab(Pool);
  while (PCS) {
    if (PCS->Slab == PS) return true;
    //we can optimize by moving it to the front of the list
    PCS = PCS->nextSlab;
  }
  // here we check for the splay tree
  Splay *psplay = poolchecksplay(Pool);
  Splay *ref = splay_find_ptr(psplay, (unsigned long) Node);
  if (ref) {
    return true;
  }
  return false;
}


inline bool refcheck(Splay *splay, void *Node) {
  unsigned long base = (unsigned long) (splay->key);
  unsigned long length = (unsigned long) (splay->val);
  unsigned long result = (unsigned long) Node;
  if ((result >= base) && (result < (base + length))) return true;
  return false;
                                                        
}


bool poolcheckarrayoptim(MetaPoolTy *Pool, void *NodeSrc, void *NodeResult) {
  Splay *psplay = poolchecksplay(Pool);
  splay *ref = splay_find_ptr(psplay, (unsigned long)NodeSrc);
  if (ref) {
    return refcheck(ref, NodeResult);
  } 
  return false;
}

void poolcheckarray(MetaPoolTy **MP, void *NodeSrc, void *NodeResult) {
  MetaPoolTy *MetaPool = *MP;
  if (!MetaPool) {
    poolcheckfail ("Empty meta pool? \n");
  }
  //iteratively search through the list
  //Check if there are other efficient data structures.
  while (MetaPool) {
    MetaPoolTy *Pool = (MetaPoolTy *)(MetaPool->Pool);
    if (poolcheckarrayoptim(Pool, NodeSrc, NodeResult)) return ;
    MetaPool = MetaPool->next;
  }
  poolcheckfail ("poolcheck failure \n");
}

void poolcheck(MetaPoolTy **MP, void *Node) {
  MetaPoolTy *MetaPool = *MP;
  if (!MetaPool) {
    poolcheckfail ("Empty meta pool? \n");
  }
  //    iteratively search through the list
  //Check if there are other efficient data structures.
  
  while (MetaPool) {
    void *Pool = MetaPool->Pool;
    if (poolcheckoptim(Pool, Node))   return;
    MetaPool = MetaPool->next;
  }
  poolcheckfail ("poolcheck failure \n");
}


void poolcheckAddSlab(PoolCheckSlab **PCSPtr, void *Slab) {
  PoolCheckSlab  *PCSPrev = *PCSPtr;
  PoolCheckSlab *PCS = *PCSPtr;
  if (PCS) {
    PCS = PCS->nextSlab;
  } else {
    *PCSPtr = (PoolCheckSlab *) poolcheckmalloc (sizeof(PoolCheckSlab));
    (*PCSPtr)->Slab = Slab;
    (*PCSPtr)->nextSlab = 0;
    return;
  }
  while (PCS) {
    PCS = PCS->nextSlab;
    PCSPrev = PCS;
  }
  //PCS is null;
  PCSPrev->nextSlab = (PoolCheckSlab *) poolcheckmalloc (sizeof(PoolCheckSlab));
  PCSPrev->nextSlab->Slab = Slab;
  PCSPrev->nextSlab->nextSlab = 0;
}


  void exactcheck(int a, int b) {
    if ((0 > a) || (a >= b)) {
      poolcheckfail ("exact check failed\n");
    }
  }

  //
  // Disable this for kernel code.  I'm not sure how kernel code handles
  // va_list type functions.
  //
#ifdef LLVA_KERNEL
  void funccheck(unsigned num, void *f, void *g, ...) {
    va_list ap;
    unsigned i = 0;
    if (f == g) return;
    i++;
    va_start(ap, g);
    for ( ; i != num; ++i) {
      void *h = va_arg(ap, void *);
      if (f == h) {
	return;
      }
    }
    abort();
  }
#endif

  void poolcheckregister(Splay *splay, void * allocaptr, unsigned NumBytes) {
    splay_insert_ptr(splay, (unsigned long)(allocaptr), NumBytes);
  }
}
