//=====- RuntimeChecks.cpp - Implementation of poolallocator runtime -======//
// 
//                          The SAFECode Compiler
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file implements runtime checks used by SAFECode for BaggyBounds.
//
//===----------------------------------------------------------------------===//
// NOTES:
//  1) Some of the bounds checking code may appear strange.  The reason is that
//     it is manually inlined to squeeze out some more performance.  Please
//     don't change it.
//
//===----------------------------------------------------------------------===//

#include "ConfigData.h"
#include "DebugReport.h"
#include "PoolAllocator.h"
#include "RewritePtr.h"

#include "safecode/Runtime/BBMetaData.h"
#include "safecode/Runtime/BBRuntime.h"

#include "../include/CWE.h"

#include <map>
#include <cstdarg>
#include <stdint.h>
#include <string.h>

#define TAG unsigned tag

#include <errno.h>
#include <stdio.h>

extern FILE * ReportLog;
extern unsigned char* __baggybounds_size_table_begin; 
extern unsigned SLOT_SIZE;
extern unsigned SLOTSIZE;
extern unsigned WORD_SIZE;
extern const unsigned int  logregs;
using namespace NAMESPACE_SC;

//
// Function: _barebone_pointers_in_bounds()
//
// Description:
//  This is the internal path for a boundscheck() and boundcheckui() calls.
//
// Inputs:
//  Source   - The source pointer used in the indexing operation (the GEP).
//  Dest     - The result pointer of the indexing operation (the GEP).
//
// Return:
//  0:  The Dest is within the valid object in which Source was found.
//  1:  The Dest is not within the valid object in which Source was found.
//
static inline int
_barebone_pointers_in_bounds(uintptr_t Source, uintptr_t Dest) {
  //
  // Look for the bounds in the table.
  //
  unsigned char e;
  e = __baggybounds_size_table_begin[Source >> SLOT_SIZE];
  // The object is not registed, so it cannot be checked.
  if (e == 0) return 0; 
  //
  // Currently we does not support alignment that is larger than one page size
  // in 32bit Linux
  //
  if (e > 12) return 0;
  //
  // Get the bounds for the object in which Source was found.
  //
  uintptr_t begin = Source & ~((1<<e)-1);
  BBMetaData *data = (BBMetaData*)(begin + (1<<e) - sizeof(BBMetaData));
  if (data->size == 0) return 0;
  uintptr_t end = begin + data->size;
  //
  // If the Dest is within the valid object in which Source was found,
  // return 0; else return 1.
  //
  return !(begin <= Source && Source < end && begin <= Dest && Dest < end);
}

//
// Function: _barebone_boundscheck()
//
// Description:
//  Perform an accurate bounds check for the given pointer.  This function
//  encapsulates the logic necessary to do the check.
//
// Return value:
//  The dest pointer if it is in bounds, else an OOB pointer.
//  
//
static inline void*
_barebone_boundscheck (uintptr_t Source, uintptr_t Dest) {

  uintptr_t val = 1 ;
  void * RealSrc = (void *)Source;
  void * RealDest = (void *)Dest;

  //
  // Check the bounds of the pointers.
  //
  val = _barebone_pointers_in_bounds(Source, Dest);
  if(!val) return RealDest;

  //
  // Either:
  //  1) Dest is not within the valid object in which Source was found or
  //  2) Source is an OOB pointer.
  //
  if (!isRewritePtr((void *)Source)) {
    // Dest is not within the valid object in which Source was found.
    RealDest = rewrite_ptr(NULL, RealDest, 0, 0, 0, 0);
    return RealDest;
  }

  //
  // This means that Source is an OOB pointer. Compute the original source.
  //
  RealSrc = pchk_getActualValue(NULL, (void *)Source);
  //
  // Compute the real result pointer.
  //
  RealDest = (void *)((intptr_t) RealSrc + Dest - Source);
  //
  // Re-check the real result pointer.
  //
  val = _barebone_pointers_in_bounds((uintptr_t)RealSrc, (uintptr_t)RealDest);
  if (!val) return RealDest;
   
  RealDest = rewrite_ptr(NULL, RealDest, 0, 0, 0, 0);
  return RealDest;
}

//
// Function: poolcheck_debug()
//
// Description:
//  This function performs a load/store check.  It ensures that the given
//  pointer points into a valid memory object.
//
void
bb_poolcheck_debug (DebugPoolTy *Pool,
                 void *Node,
                 unsigned length,
                 TAG,
                 const char * SourceFilep,
                 unsigned lineno) {
  // If the address being checked is errno, then the check can pass.
  unsigned char * errnoPtr = (unsigned char *) &errno;
  if ((unsigned char *)Node == errnoPtr) return;

  //
  // Check if is an OOB pointer
  //
  if (isRewritePtr(Node)) {
    DebugViolationInfo v;
    v.type = ViolationInfo::FAULT_LOAD_STORE,
    v.faultPC = __builtin_return_address(0),
    v.faultPtr = Node,
    v.CWE = CWEBufferOverflow,
    v.SourceFile = SourceFilep,
    v.lineNo = lineno;

    ReportMemoryViolation(&v);
    return;
  }

  // 
  // Check to see if the pointer points to an object within the pool.  If it
  // does, check to see if the last byte read/written will be within the same
  // object.  If so, then the check succeeds, so just return to the caller.
  //
  unsigned char e;
  e = __baggybounds_size_table_begin[(uintptr_t)Node >> SLOT_SIZE];
  if (e == 0) return;

  uintptr_t ObjStart = (uintptr_t)Node & ~((1<<e)-1);
  BBMetaData *data = (BBMetaData*)(ObjStart + (1<<e) - sizeof(BBMetaData));
  uintptr_t ObjEnd = ObjStart + data->size - 1;

  uintptr_t NodeEnd = (uintptr_t)Node + length -1;
  if (!(ObjStart <= NodeEnd) && (NodeEnd <= ObjEnd)) {
    DebugViolationInfo v;
    v.type = ViolationInfo::FAULT_LOAD_STORE,
    v.faultPC = __builtin_return_address(0),
    v.faultPtr = (void *)NodeEnd,
    v.CWE = CWEBufferOverflow,
    v.SourceFile = SourceFilep,
    v.lineNo = lineno,

    ReportMemoryViolation(&v);
    return;
  }
  
  return;
}

void
bb_poolcheckui_debug (DebugPoolTy *Pool,
                 void *Node,
                 unsigned length,
                 TAG,
                 const char * SourceFilep,
                 unsigned lineno) {
  // If the address being checked is errno, then the check can pass.
  unsigned char * errnoPtr = (unsigned char *) &errno;
  if ((unsigned char *)Node == errnoPtr) return;
  //
  // Check if is an OOB pointer
  //
  if (isRewritePtr(Node)) {
    DebugViolationInfo v;
    v.type = ViolationInfo::FAULT_LOAD_STORE,
    v.faultPC = __builtin_return_address(0),
    v.faultPtr = Node,
    v.CWE = CWEBufferOverflow,
    v.SourceFile = SourceFilep,
    v.lineNo = lineno;

    ReportMemoryViolation(&v);
    return;
  }

  // 
  // Check to see if the pointer points to an object within the pool.  If it
  // does, check to see if the last byte read/written will be within the same
  // object.  If so, then the check succeeds, so just return to the caller.
  //
  unsigned char e;
  e = __baggybounds_size_table_begin[(uintptr_t)Node >> SLOT_SIZE];
  if (e == 0) return;

  uintptr_t ObjStart = (uintptr_t)Node & ~((1<<e)-1);
  BBMetaData *data = (BBMetaData*)(ObjStart + (1<<e) - sizeof(BBMetaData));
  uintptr_t ObjEnd = ObjStart + data->size - 1;

  uintptr_t NodeEnd = (uintptr_t)Node + length -1;
  if (!(ObjStart <= NodeEnd) && (NodeEnd <= ObjEnd)) {
    DebugViolationInfo v;
    v.type = ViolationInfo::FAULT_LOAD_STORE,
    v.faultPC = __builtin_return_address(0),
    v.faultPtr = (void *)NodeEnd,
    v.CWE = CWEBufferOverflow,
    v.SourceFile = SourceFilep,
    v.lineNo = lineno,

    ReportMemoryViolation(&v);
    return;
  }
  
  return;
}

extern "C" void
poolcheckui_debug (DebugPoolTy *Pool,
                 void *Node,
                 unsigned length, TAG,
                 const char * SourceFilep,
                 unsigned lineno) {
  bb_poolcheckui_debug(Pool, Node, length, tag, SourceFilep, lineno);
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
void
bb_poolcheckalign_debug (DebugPoolTy *Pool, 
                         void *Node, 
                         unsigned Offset, TAG, 
                         const char * SourceFile, 
                         unsigned lineno) {
  //
  // Check if is an OOB pointer
  //
  if (isRewritePtr(Node)) {

    //
    // The object has not been found.  Provide an error.
    //
    DebugViolationInfo v;
    v.type = ViolationInfo::FAULT_LOAD_STORE,
    v.faultPC = __builtin_return_address(0),
    v.faultPtr = Node,
    v.CWE = CWEBufferOverflow,
    v.SourceFile = SourceFile,
    v.lineNo = lineno;

    ReportMemoryViolation(&v);
  }
  return;
}

void
bb_poolcheckui (DebugPoolTy *Pool, void *Node) {
  return bb_poolcheckui_debug(Pool, Node, 1, 0, NULL, 0);
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
bb_boundscheck_debug (DebugPoolTy * Pool, 
                      void * Source, 
                      void * Dest, TAG, 
                      const char * SourceFile, 
                      unsigned lineno) {
  if (!isRewritePtr((void *)Source) && (Source == Dest)) return Dest;
  return _barebone_boundscheck((uintptr_t)Source, (uintptr_t)Dest);
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
bb_boundscheckui_debug (DebugPoolTy * Pool,
                     void * Source,
                     void * Dest, TAG,
                     const char * SourceFile,
                     unsigned int lineno) {
  return  _barebone_boundscheck((uintptr_t)Source, (uintptr_t)Dest);
}

extern "C" void *
boundscheckui_debug (DebugPoolTy * Pool,
                     void * Source,
                     void * Dest, TAG,
                     const char * SourceFile,
                     unsigned int lineno) {
  return bb_boundscheckui_debug(Pool, Source, Dest, tag, SourceFile, lineno);
}


/// Stubs

void
bb_poolcheck (DebugPoolTy *Pool, void *Node) {
  bb_poolcheck_debug(Pool, Node, 1, 0, NULL, 0);
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
bb_boundscheck (DebugPoolTy * Pool, void * Source, void * Dest) {
  return bb_boundscheck_debug(Pool, Source, Dest, 0, NULL, 0);
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
bb_boundscheckui (DebugPoolTy * Pool, void * Source, void * Dest) {
  return bb_boundscheckui_debug (Pool, Source, Dest, 0, NULL, 0);
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
bb_poolcheckalign (DebugPoolTy *Pool, void *Node, unsigned Offset) {
  bb_poolcheckalign_debug(Pool, Node, Offset, 0, NULL, 0);
}

/*void *
pchk_getActualValue (DebugPoolTy * Pool, void * ptr) {
  uintptr_t Source = (uintptr_t)ptr;
  if (isRewritePtr(Source)) {
    //Source = getActualValue(Source);
  }
  
  return (void*)Source;
}*/

//
// Function: __sc_bb_funccheck()
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
__sc_bb_funccheck (void *f,
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
// Function: fastlscheck()
//
// Description:
//  This function performs a fast load/store check.  If the check fails, it
//  will *not* attempt to do pointer rewriting.
//
// Inputs:
//  base   - The address of the first byte of a memory object.
//  result - The pointer that is being checked.
//  size   - The size of the object in bytes.
//  lslen  - The length of the data accessed in memory.
//

extern "C" void
fastlscheck_debug(const char *base, const char *result, unsigned size,
                   unsigned lslen,
                   unsigned tag,
                   const char * SourceFile,
                   unsigned lineno) {
  // If the address being checked is errno, then the check can pass.
  char * errnoPtr = (char *) &errno;
  if (result == errnoPtr) return;

  //
  // If the pointer is within the object, the check passes.  Return the checked
  // pointer.
  //
  const char * end = result + lslen - 1;
  if ((result >= base) && (result < (base + size))) {
    if ((end >= base) && (end < (base + size))) {
      return;
    }
  }

  //
  // Check failed. Provide an error.
  //
  DebugViolationInfo v;
  v.type = ViolationInfo::FAULT_LOAD_STORE,
  v.faultPC = __builtin_return_address(0),
  v.faultPtr = result,
  v.CWE = CWEBufferOverflow,
  v.dbgMetaData = NULL,
  v.SourceFile = SourceFile,
  v.lineNo = lineno;
  
  ReportMemoryViolation(&v);
  
  return;
}

//
// Function: poolcheck_freeui_debug()
//
// Description:
//  Check that freeing the pointer is correct.  Permit incomplete and unknown
//  pointers.
//
void
bb_poolcheck_freeui_debug (DebugPoolTy *Pool,
                      void * ptr,
                      unsigned tag,
                      const char * SourceFilep,
                      unsigned lineno) {
  //
  // Ignore frees of NULL pointers.  These are okay.
  //
  if (ptr == NULL)
    return;

  //
  // Retrieve the bounds information for the object.  Use the pool that tracks
  // debug information since we're in debug mode.
  //
  unsigned char e;
  e = __baggybounds_size_table_begin[(uintptr_t)ptr >> SLOT_SIZE];

  uintptr_t ObjStart = (uintptr_t)ptr & ~((1<<e)-1);
  BBMetaData *data = (BBMetaData*)(ObjStart + (1<<e) - sizeof(BBMetaData));
  uintptr_t ObjLen = data->size;

  //
  // Determine if we're freeing a pointer that doesn't point to the beginning
  // of an object.  If so, report an error.
  //
  if ((uintptr_t)ptr != ObjStart) {
    OutOfBoundsViolation v;
    v.type = ViolationInfo::FAULT_INVALID_FREE,
      v.faultPC = __builtin_return_address(0),
      v.faultPtr = ptr,
      v.CWE = CWEFreeNotStart,
      v.SourceFile = SourceFilep,
      v.lineNo = lineno,
      v.objStart = (void *)ObjStart;
      v.objLen = ObjLen;
    ReportMemoryViolation(&v);
  }

  return;
}

extern "C" void
poolcheck_freeui_debug (DebugPoolTy *Pool,
                      void * ptr,
                      unsigned tag,
                      const char * SourceFilep,
                      unsigned lineno) {
  bb_poolcheck_freeui_debug(Pool, ptr, tag, SourceFilep, lineno);
}


//
//
// Function: poolcheck_free_debug()
//
// Description:
//  Check that freeing the pointer is correct.
//
void
bb_poolcheck_free_debug (DebugPoolTy *Pool,
                      void * ptr,
                      unsigned tag,
                      const char * SourceFilep,
                      unsigned lineno) {
  bb_poolcheck_freeui_debug(Pool, ptr, tag, SourceFilep, lineno);
}

//
// Function: poolcheck_free()
//
// Description:
//  Check that freeing the pointer is correct.
//
void
bb_poolcheck_free (DebugPoolTy *Pool, void * ptr) {
  bb_poolcheck_free_debug(Pool, ptr, 0, NULL, 0);
}

//
// Function: poolcheck_freeui()
//
// Description:
//  The incomplete version of poolcheck_free().
//
void
poolcheck_freeui (DebugPoolTy *Pool, void * ptr) {
  bb_poolcheck_freeui_debug(Pool, ptr, 0, NULL, 0);
}

//
// Function: nullstrlen()
//
// Description:
//  This version of strlen() will return zero for NULL pointers.
//
extern "C" size_t nullstrlen (const char * s);
size_t
nullstrlen (const char * s) {
  if (s)
    return strlen (s);
  return 0;
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
extern "C" void
funccheck (void *f, void * targets[]) {
  __sc_bb_funccheck(f, targets, 0, NULL, 0);
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
extern "C" void
funccheck_debug (void *f,
                 void * targets[],
                 TAG,
                 const char * SourceFilep,
                 unsigned lineno) {
  __sc_bb_funccheck(f, targets, 0, SourceFilep, lineno);
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
extern "C" void
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
extern "C" void
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
