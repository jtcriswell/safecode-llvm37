//===- PoolAllocatorBitMask.cpp - Implementation of poolallocator runtime -===//
// 
//                          The SAFECode Compiler
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file implements various runtime checks used by SAFECode.
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

#include "DebugReport.h"
#include "PoolAllocator.h"
#include "PageManager.h"
#include "ConfigData.h"
#include "RewritePtr.h"

#include "../include/CWE.h"
#include "../include/DebugRuntime.h"

#include <errno.h>

#include <map>
#include <cstdarg>
#include <cstdio>
#include <cassert>

#define TAG unsigned tag

extern FILE * ReportLog;

using namespace llvm;

static inline unsigned char
isInCache (DebugPoolTy * Pool, void * p) {
  if ((Pool->objectCache[0].lower <= p) && (p <= Pool->objectCache[0].upper))
    return 0;

  if ((Pool->objectCache[1].lower <= p) && (p <= Pool->objectCache[1].upper))
    return 1;

  return 2;
}

static inline void
updateCache (DebugPoolTy * Pool, void * Start, void * End) {
  Pool->objectCache[Pool->cacheIndex].lower = Start;
  Pool->objectCache[Pool->cacheIndex].upper = End;
  Pool->cacheIndex = (Pool->cacheIndex) ? 0 : 1;
  return;
}

//
// Provide dummy implementations of the common infrastructure run-time checks
// to appease libLTO linking on Mac OS X.
//
extern "C" {
  void __loadcheck(unsigned char*, size_t);
  void __storecheck(unsigned char*, size_t);
  void __fastloadcheck (unsigned char *, size_t, unsigned char *, size_t);
  void __faststorecheck(unsigned char *, size_t, unsigned char *, size_t);
  unsigned char * __fastgepcheck(unsigned char*, unsigned char*, unsigned char*, size_t);
}

void
__loadcheck(unsigned char* a, size_t b) {
  return;
}

void
__storecheck(unsigned char* a, size_t b) {
  return;
}

void
__fastloadcheck (unsigned char * p, size_t size, unsigned char * q, size_t r) {
  return;
}

void
__faststorecheck (unsigned char * a, size_t b, unsigned char * c, size_t d) {
  return;
}

unsigned char *
__fastgepcheck (unsigned char* a, unsigned char* b, unsigned char* c, size_t d){
  return 0;
}

//
// Function: _barebone_poolcheck()
//
// Description:
//  Perform an accurate load/store check for the given pointer.  This function
//  encapsulates the logic necessary to do the check.
//
// Outputs:
//  ObjStart - The address of the first valid byte of the memory object.
//  ObjEnd   - The address of the last valid byte of the memory object.
//
// Return value:
//  true  - The pointer was found within a valid object within the pool.
//  false - The pointer was not found within a valid object within the pool.
//
static inline bool
_barebone_poolcheck (DebugPoolTy * Pool, void * Node, unsigned length,
                     void * & ObjStart, void * & ObjEnd) {
  //
  // If the pool handle is NULL, claim that we have not found the object.
  //
  if (!Pool) return false;

  //
  // First check the cache of objects to see if the pointer is in there.
  // Otherwise, look through the splay trees for an object in which the
  // pointer points.
  //
  bool found = false;
  unsigned char index = isInCache (Pool, Node);
  if (index < 2) {
    found = true;
    ObjStart = Pool->objectCache[index].lower;
    ObjEnd = Pool->objectCache[index].upper; 
  } else {
    found = Pool->Objects.find (Node, ObjStart, ObjEnd);
  }

  //
  // If the memory access is within bounds, update the cache and return.
  //
  if ((found) && (ObjStart <= Node) && (Node <= ObjEnd)) {
    updateCache (Pool, ObjStart, ObjEnd);
    return true;
  }

  //
  // This may be a singleton object, so search for it within the pool slabs
  // itself.
  //
#if 1
  if ((ObjStart = __pa_bitmap_poolcheck (Pool, Node))) {
    ObjEnd = (unsigned char *) ObjStart + Pool->NodeSize - 1;
    updateCache (Pool, ObjStart, ObjEnd);
    return true;
  }
#endif

  //
  // The node is not found or is not within bounds; fail!
  //
  return false;
}

//
// Function: poolcheck_debug()
//
// Description:
//  This function performs a load/store check.  It ensures that the given
//  pointer points into a valid memory object.
//
void
poolcheck_debug (DebugPoolTy *Pool,
                 void *Node,
                 unsigned length,
                 TAG,
                 const char * SourceFilep,
                 unsigned lineno) {
  //
  // If the memory access is zero bytes in length, don't report an error.
  // This can happen on memcpy() and memset() calls that are instrumented
  // with load/store checks.
  //
  if (length == 0)
    return;

  //
  // Check to see if the pointer points to an object within the pool.  If it
  // does, check to see if the last byte read/written will be within the same
  // object.  If so, then the check succeeds, so just return to the caller.
  //
  void * ObjStart, *ObjEnd;
  unsigned char * NodeEnd = (unsigned char *)(Node) + length - 1;
  if (_barebone_poolcheck (Pool, Node, length, ObjStart, ObjEnd)) {
    if (!((ObjStart <= NodeEnd) && (NodeEnd <= ObjEnd))) {
      DebugViolationInfo v;
      v.type = ViolationInfo::FAULT_LOAD_STORE,
        v.faultPC = __builtin_return_address(0),
        v.faultPtr = NodeEnd,
        v.CWE = CWEBufferOverflow,
        v.SourceFile = SourceFilep,
        v.lineNo = lineno,
        v.PoolHandle = Pool;
      ReportMemoryViolation(&v);
    }

    return;
  }

  //
  // Look for the object within the splay tree of external objects.
  //
  if (ExternalObjects->find (Node, ObjStart, ObjEnd)) {
    if ((ObjStart <= Node) && (Node <= ObjEnd)) {
      if (!((ObjStart <= NodeEnd) && (NodeEnd <= ObjEnd))) {
        DebugViolationInfo v;
        v.type = ViolationInfo::FAULT_LOAD_STORE,
          v.faultPC = __builtin_return_address(0),
          v.faultPtr = NodeEnd,
          v.CWE = CWEBufferOverflow,
          v.SourceFile = SourceFilep,
          v.lineNo = lineno,
          v.PoolHandle = Pool;
        ReportMemoryViolation(&v);
      }
    }

    return;
  }

  //
  // If the pointer is within the errno variable, go ahead and let it be.
  // For some reason, errno may not be registered in the ExternalObjects splay
  // tree with the same location as during program startup, so we'll check it
  // again now.
  //
  unsigned char * errnoPtr = (unsigned char *) &errno;
  if ((errnoPtr == Node) && (length <= sizeof (errno)))
    return;

  //
  // If it's a rewrite pointer, convert it back into its original value so
  // that we can print the real faulting address.
  //
  if (isRewritePtr (Node)) {
    Node = pchk_getActualValue (Pool, Node);
  }

  //
  // If dangling pointer detection is enabled, see if the pointer points within
  // a freed memory object.  If so, this is a dangling pointer error.
  // Otherwise, it is just a regular load/store error.
  //
  DebugViolationInfo v;
  v.type = ViolationInfo::FAULT_LOAD_STORE,
    v.faultPC = __builtin_return_address(0),
    v.faultPtr = Node,
    v.CWE = CWEDP,
    v.SourceFile = SourceFilep,
    v.lineNo = lineno,
    v.PoolHandle = Pool;

  ReportMemoryViolation(&v);
  return;
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
poolcheckalign_debug (DebugPoolTy *Pool, void *Node, unsigned Offset, TAG, const char * SourceFile, unsigned lineno) {
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
  // First check the cache of objects to see if the pointer is in there.
  //
  void * S = 0;
  void * end = 0;
  bool found = false;
  unsigned char index = isInCache (Pool, Node);
  if (index < 2) {
    S   = Pool->objectCache[index].lower;
    end = Pool->objectCache[index].upper;
    found = true;
  }

  //
  // Look for the object in the splay of regular objects.
  //
  if (!found)
    found = Pool->Objects.find (Node, S, end);

  //
  // If we can't find the object in the splay tree, try to find it in the pool
  // itself.
  //
  if (!found) {
#if 1
    if (void * start = __pa_bitmap_poolcheck (Pool, Node)) {
      S = start;
      end = (unsigned char *)start + Pool->NodeSize - 1;
      found = true;
    }
#endif
  }

  //
  // Determine whether the alignment of the object is correct.  Note that Node
  // may be pointing to an array of objects, so we need to take the offset of
  // the pointer from the beginning of the object modulo the size of a single
  // array element.  In this run-time, the size of a single element is stored
  // within the pool descriptor.
  //
  if (found) {
    unsigned char * Nodep = (unsigned char *) Node;
    unsigned char * Sp = (unsigned char *) S;
    unsigned Alignment = ((Nodep - Sp) % Pool->NodeSize);
    if (Alignment == Offset) {
      return;
    }
  }

  //
  // The object has not been found.  Provide an error.
  //
  if (logregs) {
    fprintf (stderr, "Violation(A): %p: %p %d %d\n", (void *) Pool, Node, Offset, Pool->NodeSize);
    fflush (stderr);
  }

  unsigned char * Sp = (unsigned char *) S;
  unsigned char * endp = (unsigned char *) end;

  AlignmentViolation v;
  v.type = ViolationInfo::FAULT_ALIGN,
    v.faultPC = __builtin_return_address(0),
    v.faultPtr = Node,
    v.CWE = CWEBufferOverflow,
    v.PoolHandle = Pool,
    v.dbgMetaData = 0,
    v.SourceFile = SourceFile,
    v.lineNo = lineno,
    v.objStart = (found ? Sp : 0),
    v.objLen = (found ? (endp - Sp) + 1: 0);
    v.alignment = Offset;

  ReportMemoryViolation(&v);
}

void
poolcheckui_debug (DebugPoolTy *Pool,
                   void *Node,
                   unsigned length,
                   TAG,
                   const char * SourceFilep,
                   unsigned lineno) {
  //
  // If the memory access is zero bytes in length, don't report an error.
  // This can happen on memcpy() and memset() calls that are instrumented
  // with load/store checks.
  //
  if (length == 0)
    return;

  //
  // Check to see if the pointer points to an object within the pool.  If it
  // does, check to see if the last byte read/written will be within the same
  // object.  If so, then the check succeeds, so just return to the caller.
  //
  void * ObjStart, *ObjEnd;
  unsigned char * NodeEnd = (unsigned char *)(Node) + length - 1;
  if (_barebone_poolcheck (Pool, Node, length, ObjStart, ObjEnd)) {
    if (!((ObjStart <= NodeEnd) && (NodeEnd <= ObjEnd))) {
      DebugViolationInfo v;
      v.type = ViolationInfo::FAULT_LOAD_STORE,
        v.faultPC = __builtin_return_address(0),
        v.faultPtr = NodeEnd,
        v.CWE = CWEBufferOverflow,
        v.SourceFile = SourceFilep,
        v.lineNo = lineno,
        v.PoolHandle = Pool;
      ReportMemoryViolation(&v);
    }

    return;
  }

  //
  // Look for the object within the splay tree of external objects.
  // Always look in these splay tree because some objects (namely argv strings)
  // are stored in this splay tree.
  //
  int fs = 0;
  if ((fs = ExternalObjects->find (Node, ObjStart, ObjEnd))) {
    if ((ObjStart <= Node) && (Node <= ObjEnd)) {
      if (!((ObjStart <= NodeEnd) && (NodeEnd <= ObjEnd))) {
        DebugViolationInfo v;
        v.type = ViolationInfo::FAULT_LOAD_STORE,
          v.faultPC = __builtin_return_address(0),
          v.faultPtr = NodeEnd,
          v.CWE = CWEBufferOverflow,
          v.SourceFile = SourceFilep,
          v.lineNo = lineno,
          v.PoolHandle = Pool;
        ReportMemoryViolation(&v);
      }
    }

    return;
  }

  //
  // If it's a rewrite pointer, convert it back into its original value so
  // that we can print the real faulting address.
  //
  ObjStart = 0;
  ObjEnd = 0;
  if (isRewritePtr (Node)) {
    ObjStart = RewrittenObjs()[Node].first;
    ObjEnd   = RewrittenObjs()[Node].second;
    Node = pchk_getActualValue (Pool, Node);
  }

  //
  // The node is not found or is not within bounds.  Report a warning but keep
  // going.
  //
  if (logregs) {
    fprintf (stderr, "PoolcheckUI failed(%p:%x): %p %p from %p\n", 
        (void*)Pool, fs, (void*)Node, ObjEnd, __builtin_return_address(0));
    fflush (stderr);
  }

  //
  // Report a potential (but not certain) load/store violation if we find that
  // the value is a rewrite pointer.
  //
  if (ObjStart) {
    //
    // Attempt to get information on where the memory object was allocated.
    //
    void * start;
    void * end;
    PDebugMetaData debugmetadataptr = 0;
    int fs =dummyPool.DPTree.find (ObjStart, start, end, debugmetadataptr);

    OutOfBoundsViolation v;
    v.type = ViolationInfo::FAULT_LOAD_STORE,
      v.faultPC = __builtin_return_address(0),
      v.faultPtr = Node,
      v.CWE = CWEBufferOverflow,
      v.SourceFile = SourceFilep,
      v.lineNo = lineno,
      v.PoolHandle = Pool;
      v.objStart = ObjStart;
      v.objLen = (unsigned)((char*) ObjEnd - (char*)(ObjStart)) + 1;
    if (fs) {
      v.dbgMetaData = debugmetadataptr;
    }
    ReportMemoryViolation(&v);
  }

  //
  // If the pointer is a NULL pointer, report an error for that.
  //
  if (Node == 0) {
    DebugViolationInfo v;
    v.type = ViolationInfo::FAULT_LOAD_STORE,
      v.faultPC = __builtin_return_address(0),
      v.faultPtr = Node,
      v.CWE = CWENull,
      v.SourceFile = SourceFilep,
      v.lineNo = lineno,
      v.PoolHandle = Pool;
    ReportMemoryViolation(&v);
  }

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
//  true  - The object was found in the pool.
//  false - The object was not found in the pool.
//
static bool 
boundscheck_lookup (DebugPoolTy * Pool, void * & Source, void * & End ) {
  //
  // If there is a pool, then search for the object within the pool and return
  // its bounds.
  //
  if (Pool) {
    //
    // First check the cache of objects to see if the pointer is in there.
    //
    unsigned char index = isInCache (Pool, Source);
    if (index < 2) {
      Source = Pool->objectCache[index].lower;
      End    = Pool->objectCache[index].upper;
      return true;
    }

    //
    // Search the splay tree.  If we find the object, add it to the cache.
    //
    if (Pool->Objects.find(Source, Source, End)) {
      updateCache (Pool, Source, End);
      return true;
    }

    //
    // Perhaps we are indexing on a singleton object.  Do a poolcheck to
    // get the object bounds and recheck the pointer.
    //
#if 1
    if (void * start = __pa_bitmap_poolcheck (Pool, Source)) {
      Source = start;
      End = (unsigned char *)start + Pool->NodeSize - 1;
      updateCache (Pool, Source, End);
      return true;
    }
#endif
  }

  //
  // No pool was given, so we cannot find the object.  Let the run-time use the
  // slow path to search for externally allocated objects, argv pointers, and
  // the like.
  //
  return false;
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
static void *
boundscheck_check (bool found, void * ObjStart, void * ObjEnd,
                   DebugPoolTy * Pool,
                   void * Source, void * Dest, bool CanFail,
                   const char * SourceFile, unsigned int lineno) {
  //
  // Determine if this is a rewrite pointer that is being indexed.  If so,
  // compute the original value, re-do the indexing operation, and rewrite the
  // value back.
  //
  if (isRewritePtr (Source)) {
    //
    // Get the real pointer value (which is outside the bounds of a valid
    // object).
    //
    void * RealSrc = pchk_getActualValue (Pool, Source);

    //
    // Compute the real result pointer (the value the GEP would really have on
    // the original pointer value).
    //
    Dest = (void *)((intptr_t) RealSrc + ((intptr_t) Dest - (intptr_t) Source));

    //
    // Retrieve the original bounds of the object.
    //
    getOOBObject (Source, ObjStart, ObjEnd);

    //
    // Redo the bounds check.  If it succeeds, return the real value.
    // Otherwise, just continue on with the rest of the failed bounds check
    // processing as before.
    //
    if (__builtin_expect (((ObjStart <= Dest) && ((Dest <= ObjEnd))), 1)) {
      if (logregs) {
        fprintf (stderr, "unrewrite(1): (0x%p) -> (0x%p, 0x%p) \n", Source, RealSrc, Dest);
        fflush (stderr);
      }
      return Dest;
    }

    //
    // Pretend this was an index off of the original out of bounds pointer
    // value and continue processing
    //
    if (logregs) {
      fprintf (stderr, "unrewrite(2): %p -> %p, Dest: %p, Obj: %p - %p\n", Source, RealSrc, Dest, ObjStart, ObjEnd);
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
                 ObjStart, ObjEnd, Source, Dest, (void*)__builtin_return_address(0), ptr, SourceFile, lineno);
        fflush (ReportLog);
      }
      return ptr;
    } else {
      intptr_t allocPC = 0;
      unsigned allocID = 0;
      unsigned char * allocSF = (unsigned char *) "<Unknown>";
      unsigned allocLN = 0;
      PDebugMetaData debugmetadataptr = NULL;
      void * S , * end;
      if (dummyPool.DPTree.find(ObjStart, S, end, debugmetadataptr)) {
        allocPC = ((intptr_t) (debugmetadataptr->allocPC)) - 5;
        allocID  = debugmetadataptr->allocID;
        allocSF  = (unsigned char *) debugmetadataptr->SourceFile;
        allocLN  = debugmetadataptr->lineno;
      }

      OutOfBoundsViolation v;
      v.type = ViolationInfo::FAULT_OUT_OF_BOUNDS,
        v.faultPC = __builtin_return_address(0),
        v.faultPtr = Dest,
        v.CWE = CWEBufferOverflow,
        v.dbgMetaData = debugmetadataptr,
        v.PoolHandle = Pool, 
        v.SourceFile = SourceFile,
        v.lineNo = lineno,
        v.objStart = ObjStart,
        v.objLen = (unsigned)((char*) ObjEnd - (char*)(ObjStart)) + 1;

      ReportMemoryViolation(&v);
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
        fprintf (ReportLog, "boundscheck: NULL Index: %x %x %p %p at pc=%p at %s (%d)\n",
                 0, 4096, (void*)Source, (void*)Dest, (void*)__builtin_return_address(0), SourceFile, lineno);
        fflush (ReportLog);
      }
      return Dest;
    } else {
      if ((ConfigData.StrictIndexing == false) ||
          (((uintptr_t) Dest) == 4096)) {
        if (logregs) {
          fprintf (ReportLog, "boundscheck: rewrite(3): %x %x %p %p at pc=%p at %s (%d)\n",
                   0, 4096, (void*)Source, (void*)Dest, (void*)__builtin_return_address(0), SourceFile, lineno);
          fflush (ReportLog);
        }
        return rewrite_ptr (Pool,
                            Dest,
                            (void *)0,
                            (void *)4096,
                            SourceFile,
                            lineno);
      } else {
        OutOfBoundsViolation v;
        v.type = ViolationInfo::FAULT_OUT_OF_BOUNDS,
          v.faultPC = __builtin_return_address(0),
          v.faultPtr = Dest,
          v.CWE = CWEBufferOverflow,
          v.dbgMetaData = NULL,
          v.PoolHandle = Pool, 
          v.SourceFile = NULL,
          v.lineNo = 0,
          v.objStart = 0,
          v.objLen = 4096;

        ReportMemoryViolation(&v);
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
    bool fs = ExternalObjects->find(Source, S, end);
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
                     S, end, Source, Dest, (void*)__builtin_return_address(0),
                     ptr, SourceFile, lineno);
          fflush (ReportLog);
          return ptr;
        }
        
        OutOfBoundsViolation v;
        v.type = ViolationInfo::FAULT_OUT_OF_BOUNDS,
          v.faultPC = __builtin_return_address(0),
          v.faultPtr = Dest,
          v.CWE = CWEBufferOverflow,
          v.dbgMetaData = NULL,
          v.PoolHandle = Pool, 
          v.SourceFile = SourceFile,
          v.lineNo = lineno,
          v.objStart = S,
          v.objLen = (unsigned)((char*) end - (char*)(S)) + 1;

        ReportMemoryViolation(&v);
      }
    }
  }

  //
  // We cannot find the object.  Continue execution.
  //
  if (CanFail) {
    OutOfBoundsViolation v;
    v.type = ViolationInfo::FAULT_OUT_OF_BOUNDS,
      v.faultPC = __builtin_return_address(0),
      v.faultPtr = Dest,
      v.CWE = CWEBufferOverflow,
      v.PoolHandle = Pool, 
      v.dbgMetaData = NULL,
      v.SourceFile = SourceFile,
      v.lineNo = lineno,
      v.objStart = 0,
      v.objLen = 0;
    
    ReportMemoryViolation(&v);

  }

  //
  // Perform one last-ditch check for incomplete nodes.  It may be possible
  // that we're doing a GEP off a pointer into a freed object.  If dangling
  // pointer detection is enabled, we can determine if the source pointer
  // was freed and reject the indexing operation.
  //
  PDebugMetaData debugmetadataptr = NULL;
  if ((ConfigData.RemapObjects) &&
      (dummyPool.DPTree.find (Source, ObjStart, ObjEnd, debugmetadataptr))) {
    //
    // If the indexing operation stays within the bounds of a freed object,
    // then don't flag an error.  Dereferences of the pointer will flag an
    // error.
    //
    if (__builtin_expect (((ObjStart <= Dest) && ((Dest <= ObjEnd))), 1)) {
      return Dest;
    }

    //
    // Otherwise, do what we always do: either rewrite the pointer or generate
    // an array indexing error report.
    //
    if ((ConfigData.StrictIndexing == false) ||
        (((char *) Dest) == (((char *)ObjEnd)+1))) {
      void * ptr = rewrite_ptr (Pool, Dest, ObjStart, ObjEnd, SourceFile, lineno);
      if (logregs) {
        fprintf (ReportLog, "boundscheck: rewrite(4): %p %p %p %p at pc=%p to %p at %s (%d)\n",
                 ObjStart, ObjEnd, Source, Dest, (void*)__builtin_return_address(0), ptr, SourceFile, lineno);
        fflush (ReportLog);
      }
      return ptr;
    } else {
      intptr_t allocPC = 0;
      unsigned allocID = 0;
      unsigned char * allocSF = (unsigned char *) "<Unknown>";
      unsigned allocLN = 0;
      allocPC = ((intptr_t) (debugmetadataptr->allocPC)) - 5;
      allocID  = debugmetadataptr->allocID;
      allocSF  = (unsigned char *) debugmetadataptr->SourceFile;
      allocLN  = debugmetadataptr->lineno;

      OutOfBoundsViolation v;
      v.type = ViolationInfo::FAULT_OUT_OF_BOUNDS,
        v.faultPC = __builtin_return_address(0),
        v.faultPtr = Dest,
        v.CWE = CWEBufferOverflow,
        v.dbgMetaData = debugmetadataptr,
        v.PoolHandle = Pool, 
        v.SourceFile = SourceFile,
        v.lineNo = lineno,
        v.objStart = ObjStart,
        v.objLen = (unsigned)((char*) ObjEnd - (char*)(ObjStart)) + 1;

      ReportMemoryViolation(&v);
      return Dest;
    }
  }
  return Dest;
}

//
// Function: boundscheck_debug()
//
// Description:
//  Identical to boundscheck() except that it takes additional debug info
//  parameters.
//
// FIXME: this function is marked as noinline due to LLVM bug 4562
// http://llvm.org/bugs/show_bug.cgi?id=4562
//
// the attribute should be taken once the bug is fixed.
void * __attribute__((noinline))
boundscheck_debug (DebugPoolTy * Pool, void * Source, void * Dest, TAG, const char * SourceFile, unsigned lineno) {
  // This code is inlined at all boundscheck() calls

  // Search the splay for Source and return the bounds of the object
  void * ObjStart = Source, * ObjEnd = 0;
  bool ret = boundscheck_lookup (Pool, ObjStart, ObjEnd); 

  if (logregs) {
    fprintf (stderr, "boundscheck_debug(%d): %d: %p - %p\n", tag, ret, ObjStart, ObjEnd);
    fflush (stderr);
  }

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
boundscheckui_debug (DebugPoolTy * Pool,
                     void * Source,
                     void * Dest, TAG,
                     const char * SourceFile,
                     unsigned int lineno) {
  // This code is inlined at all boundscheckui calls

  // Search the splay for Source and return the bounds of the object
  void * ObjStart = Source, * ObjEnd = 0;
  bool ret = boundscheck_lookup (Pool, ObjStart, ObjEnd); 

  if (logregs) {
    fprintf (stderr, "boundscheckui_debug: %p: %p - %p\n", (void *) Pool, ObjStart, ObjEnd);
    fflush (stderr);
  }

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
// Function: funccheck()
//
// Description:
//  Determine whether the specified function pointer is one of the functions
//  in the given list.
//
// Inputs:
//  f         - The function pointer that we are testing.
//  targets   - Pointer to a list of potential targets.
//
void
funccheck (void *f, void * targets[]) {
  unsigned index = 0;
  while (targets[index]) {
    if (f == targets[index])
      return;
    ++index;
  }

  DebugViolationInfo v;
  v.type = ViolationInfo::FAULT_CALL,
    v.faultPC = __builtin_return_address(0),
    v.faultPtr = f,
    v.CWE = CWEBufferOverflow,
    v.SourceFile = "Unknown",
    v.lineNo = 0;

  ReportMemoryViolation(&v);
  return;
}

//
// Function: funccheck_debug()
//
// Description:
//  Determine whether the specified function pointer is one of the functions
//  in the given list.
//
// Inputs:
//  f         - The function pointer that we are testing.
//  targets   - Pointer to a list of potential targets.
//
void
funccheck_debug (void *f,
                 void * targets[],
                 TAG,
                 const char * SourceFilep,
                 unsigned lineno) {
  unsigned index = 0;
  while (targets[index]) {
    if (f == targets[index])
      return;
    ++index;
  }

  DebugViolationInfo v;
  v.type = ViolationInfo::FAULT_CALL,
    v.faultPC = __builtin_return_address(0),
    v.faultPtr = f,
    v.CWE = CWEBufferOverflow,
    v.SourceFile = SourceFilep,
    v.lineNo = lineno;

  ReportMemoryViolation(&v);
  return;
}

//
// Function: funccheckui()
//
// Description:
//  Determine whether the specified function pointer is one of the functions
//  in the given list.  However, the list may be incomplete.
//
// Inputs:
//  f         - The function pointer that we are testing.
//  targets   - Pointer to a list of potential targets.
//
void
funccheckui (void *f, void * targets[]) {
  //
  // For now, do nothing.  If the list could be incomplete, we don't know when
  // a target is valid.
  //
  return;
}

//
// Function: funccheckui_debug()
//
// Description:
//  Determine whether the specified function pointer is one of the functions
//  in the given list.  However, the list may be incomplete.
//
// Inputs:
//  f         - The function pointer that we are testing.
//  targets   - Pointer to a list of potential targets.
//
void
funccheckui_debug (void *f,
                   void * targets[],
                   TAG,
                   const char * SourceFilep,
                   unsigned lineno) {
  //
  // For now, do nothing.  If the list could be incomplete, we don't know when
  // a target is valid.
  //
  return;
}

/// Stubs

void
poolcheck (DebugPoolTy *Pool, void *Node, unsigned length) {
  poolcheck_debug(Pool, Node, length, 0, NULL, 0);
}

void
poolcheckui (DebugPoolTy *Pool, void *Node, unsigned length) {
  //
  // In production mode, do not report an error if an incomplete load/store
  // check fails.  The fact that it is incomplete means that we can't tell for
  // certain whether the pointer is invalid or if it points to a memory object
  // that belongs to a diffeent pool or comes from external code.
  //
  return;
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
boundscheck (DebugPoolTy * Pool, void * Source, void * Dest) {
  return boundscheck_debug(Pool, Source, Dest, 0, NULL, 0);
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
boundscheckui (DebugPoolTy * Pool, void * Source, void * Dest) {
  return boundscheckui_debug (Pool, Source, Dest, 0, NULL, 0);
}

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
poolcheckalign (DebugPoolTy *Pool, void *Node, unsigned Offset) {
  poolcheckalign_debug(Pool, Node, Offset, 0, NULL, 0);
}
