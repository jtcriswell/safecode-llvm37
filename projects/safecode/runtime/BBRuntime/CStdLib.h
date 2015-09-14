//===------- CStdLib.h - CStdLib runtime helper functions -----------------===//
// 
//                          The SAFECode Compiler
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file provides all external functions included by the CStdLib pass.
//
//===----------------------------------------------------------------------===//

#ifndef _CSTDLIB_H
#define _CSTDLIB_H
#include <stdio.h>

#include "CStdLibSupport.h"
#include "../include/CWE.h"
#include "../include/strnlen.h"

#include "DebugReport.h"
#include "PoolAllocator.h"
#include "safecode/Runtime/BBMetaData.h"

#include <iostream>

#define TAG unsigned tag
#define SRC_INFO const char *SourceFile, unsigned lineNo

// Default versions of arguments to debug functions
#define DEFAULT_TAG 0
#define DEFAULT_SRC_INFO "<Unknown>", 0
#define DEFAULTS DEFAULT_TAG, DEFAULT_SRC_INFO
#define SRC_INFO_ARGS SourceFile, lineNo

extern unsigned char* __baggybounds_size_table_begin;
extern unsigned SLOT_SIZE;

using namespace safecode;

namespace {

//
// Various violation types
//

static inline void
OOB_VIOLATION(const void *fault_ptr,
              DebugPoolTy *handle,
              const void *start,
              size_t len,
              SRC_INFO) {
  OutOfBoundsViolation v;
  v.type        = ViolationInfo::FAULT_OUT_OF_BOUNDS;
  v.faultPC     = __builtin_return_address(0);
  v.faultPtr    = fault_ptr;
  v.CWE         = CWEBufferOverflow;
  v.SourceFile  = SourceFile;
  v.lineNo      = lineNo;
  v.PoolHandle  = handle;
  v.objStart    = start;
  v.objLen      = len;
  v.dbgMetaData = NULL;
  ReportMemoryViolation(&v);
}

static inline void
WRITE_VIOLATION(const void *fault_ptr,
                DebugPoolTy *handle,
                size_t dst_sz,
                size_t src_sz,
                SRC_INFO) {
  WriteOOBViolation v;
  v.type = ViolationInfo::FAULT_WRITE_OUT_OF_BOUNDS;
  v.faultPC = __builtin_return_address(0); 
  v.faultPtr = fault_ptr;
  v.CWE      = CWEBufferOverflow;
  v.SourceFile = SourceFile;
  v.lineNo =     lineNo;
  v.PoolHandle = handle;
  v.dstSize =    dst_sz;
  v.srcSize =    src_sz;
  v.dbgMetaData = NULL;
  ReportMemoryViolation(&v);
}

static inline void
LOAD_STORE_VIOLATION(const void *fault_ptr,
                     DebugPoolTy *handle,
                     SRC_INFO) {
  DebugViolationInfo v;
  v.faultPC = __builtin_return_address(0);
  v.faultPtr = fault_ptr;
  v.CWE      = CWEBufferOverflow;
  v.dbgMetaData = NULL;
  v.PoolHandle = handle;
  v.SourceFile = SourceFile;
  v.lineNo = lineNo;
  ReportMemoryViolation(&v);
}

static inline void
C_LIBRARY_VIOLATION(const void *fault_ptr,
                    DebugPoolTy *handle,
                    const char *function,
                    SRC_INFO) {
  CStdLibViolation v;
  v.type = ViolationInfo::FAULT_CSTDLIB;
  v.faultPC = __builtin_return_address(0);
  v.faultPtr = fault_ptr;
  v.CWE      = CWEBufferOverflow;
  v.SourceFile = SourceFile;
  v.lineNo     = lineNo;
  v.PoolHandle = handle;
  v.function   = function;
  v.dbgMetaData = NULL;
  ReportMemoryViolation(&v);
}

// 
// Check for string termination.
//
// @param start  This is a pointer to the start of the string.
// @param end    The end of the object. String is not scanned farther than here.
// @param p      Reference to size object. Filled with the length of the string if
//               string is terminated, otherwised filled with the size of the object.
// @return       Returns true if the string is terminated within bounds (ie., 
//               if the nul terminator occurs between string and end, inclusive).
//               Returns false if no nul terminator was found.
//
// Note that start and end should be valid boundaries for a valid object.
//
static inline bool
isTerminated(const char *start, void *end, size_t &p) {
  size_t max = 1 + ((char *)end - (const char *)start), len;
  len = _strnlen((const char *)start, max);
  p = len;
  if (len == max)
    return false;
  else
    return true;
}

// 
// Check for object overlap.
//
// @param ptr1Start  The start of the first memory object
// @param ptr1End    The end of the first memory object or the bound that writing
//                   operation actually touch.
// @param ptr2Start  The start of the second memory object
// @param ptr2End    The end of the second memory object or the bound that writing
//                   operation actually touch.
//
// @return           Whether these 2 memory object overlaps
// 
static inline bool
isOverlapped(const void* ptr1Start, 
             const void* ptr1End, 
             const void* ptr2Start, 
             const void* ptr2End) {
  if( ((long int)ptr1Start>(long int)ptr2End && (long int)ptr1End>(long int)ptr2Start) || 
      ((long int)ptr1Start<(long int)ptr2End && (long int)ptr1End<(long int)ptr2Start)  )
    return false;
  return true;
}

// 
// Searches inside the given pool for the memory object associated with the
// the given address. If the memory object is found, it sets the poolBegin
// and poolEnd pointers to point to the first and last valid positions of
// the memory object, and returns true. If the memory object is not found in
// the pool, or the pool is NULL, it attemps to find the object in the external
// objects pool. When the object is not found in either pool, the function
// returns false.
//
// @param   pool       The pool handle which is expected to contain the object.
// @param   address    The address for which the bounding object is sought.
// @param   poolBegin  Reference to a pointer to set to the beginning of
//                     the memory object.
// @param   poolEnd    Reference to a pointer to set to the last valid address
//                     the memory object.
// @return  Returns true if the object was found, false otherwise.
// 
static inline bool
pool_find(DebugPoolTy *pool, void *address, void *&poolBegin, void *&poolEnd) {

  if (address == NULL)
    return false;

  // Retrieve memory area's bounds from pool handle.
  /*if ((pool && pool->Objects.find(address, poolBegin, poolEnd)) || 
      ExternalObjects->find(address, poolBegin, poolEnd))
    return true;*/
  unsigned char e;
  e = __baggybounds_size_table_begin[(uintptr_t)address>>SLOT_SIZE];
  if (e == 0) return false;
  //if (e > 12) return false;
  poolBegin =(void *) ((uintptr_t)address & ~((1<<e)-1));
  BBMetaData *data = (BBMetaData *)((uintptr_t)poolBegin + (1<<e) - sizeof(BBMetaData));
  if (data->size == 0) return false;
  poolEnd = (void *) ((uintptr_t)poolBegin + data->size);
  return true;
}

//
// Macros for determining the completeness of pointer arguments using the
// completeness bitwise vector.
//
#define ARG1_COMPLETE(c) ((bool) (c & 0x1))
#define ARG2_COMPLETE(c) ((bool) (c & 0x2))

// Return the number of bytes between a and b, inclusive.
static inline size_t byte_range(const void *a, const void *b) {
  return 1 + (char *) b - (char *) a;
}

// Use this stream for reporting errors.
static std::ostream &err = std::cerr;

//
// This function attempts to verify that given string pointer points to
// a valid string that is terminated within its memory object's boundaries.
// For strings that are marked complete, if the string is discovered to be
// not in its pool, or unterminated within memory object boundaries,
// the function reports a violation and returns false.
// For strings not marked complete, the function attempts to do the same
// checks as for complete pointers, except that it assumes the string was
// valid if the string's memory object is not found in the pool.
//
// The function returns true if no memory violations were discovered, and
// false when there was a violation.
//
// @param   string     The pointer to the string to be checked.
// @param   pool       The pool that should be searched for the memory object
//                     that contains the string.
// @param   complete   This is a boolean value which is true if the string
//                     pointer was reported complete by DSA, and false if not.
//                     If the string is incomplete, no errors are reported
//                     if it does not exist in the pool and is non-NULL.
// @param   function   The name of the C library function for debug reporting
//                     purposes.
// @param   SRC_INFO   Source and line info debug information.
// @return             Returns true if no violations were discoverd, and false
//                     if the pointer does not point to a valid string and a 
//                     memory violation was reported.
//                     Note that if the function returns true, the pointer may
//                     still not point to a valid string if the pointer was
//                     incomplete.
//
static inline bool
validStringCheck(const char *string,
                 DebugPoolTy *pool,
                 bool complete,
                 const char *function,
                 SRC_INFO) {
  void *objStart, *objEnd;
  size_t len;

  // Check if the string is NULL. If it is, report this as an error.
  if (string == NULL) {
    err << "String pointer is NULL!\n";
    C_LIBRARY_VIOLATION(string, pool, function, SRC_INFO_ARGS);
    return false;
  }

  // Retrieve the string from the pool. If no string is found and the pointer
  // is not complete, return true. Otherwise report an error and return false.
  if (!pool_find(pool, (void *)string, objStart, objEnd)) {
    if (complete) {
      err << "String not found in pool!\n";
      LOAD_STORE_VIOLATION(string, pool, SRC_INFO_ARGS);
      return false;
    }
    else
      return true;
  }

  // Do a termination check.
  if (!isTerminated(string, objEnd, len)) {
    err << "String is not terminated within object bounds!\n";
    C_LIBRARY_VIOLATION(string, pool, function, SRC_INFO_ARGS);
    return false;
  }

  return true;
}

//
// Check to see if the memory region between the location pointed to by Buf
// and the end of the same memory object is of at least the given minimum size.
//
// This function will look up the buffer object in the pool to determine its
// size. If the pointer is complete and not found in the pool the function
// will report an error. If the pointer points to a region of size less than
// MinSize then this function will report an error.
//
// Inputs
//   Pool     - The pool handle for the buffer
//   Buf      - The buffer
//   Complete - A boolean describing if the pointer is complete
//   MinSize  - The minimum expected size of the region pointed to by Buf
//   SRC_INFO - Source file and line number information for debugging purposes
//
static inline void
minSizeCheck (DebugPoolTy * Pool,
            void * Buf,
            bool Complete,
            size_t MinSize,
            SRC_INFO) {
  bool Found;
  void * BufStart = 0, * BufEnd = 0;

  //
  // Retrive the buffer's bound from the pool. If we cannot find the object and
  // we know everything about what the buffer should be pointing to, then
  // report an error.
  //
  if (!(Found = pool_find (Pool, Buf, BufStart, BufEnd)) && Complete) {
    LOAD_STORE_VIOLATION (Buf, Pool, SRC_INFO_ARGS);
  }

  if (Found) {
    //
    // Make sure that the region between the location pointed to by Buf and the
    // end of the same memory object is of size at least MinSize.
    //
    size_t BufSize = byte_range (Buf, BufEnd);

    if (BufSize < MinSize) {
      C_LIBRARY_VIOLATION (Buf, Pool, "", SRC_INFO_ARGS);
    }
  }

  return;
}

}

#endif // _CSTDLIB_H
