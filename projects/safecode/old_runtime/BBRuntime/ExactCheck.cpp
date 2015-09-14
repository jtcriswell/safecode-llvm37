/*===- ExactCheck.cpp - Implementation of exactcheck functions ------------===*/
/*                                                                            */
/*                          The SAFECode Compiler                             */
/*                                                                            */
/* This file was developed by the LLVM research group and is distributed      */
/* under the University of Illinois Open Source License. See LICENSE.TXT for  */
/* details.                                                                   */
/*                                                                            */
/*===----------------------------------------------------------------------===*/
/*                                                                            */
/* This file implements the exactcheck family of functions.                   */
/*                                                                            */
/*===----------------------------------------------------------------------===*/

#include "DebugReport.h"
#include "ConfigData.h"

#include "safecode/Config/config.h"
#include "safecode/Runtime/BBRuntime.h"

#include <stdint.h>

#include <cstdio>

extern FILE * ReportLog;

using namespace NAMESPACE_SC;

static void *
exactcheck_check (const void * ObjStart, const void * ObjEnd,
                  const void * Dest, const char * SourceFile,
                  unsigned int lineno) __attribute__((noinline));
/*
 * Function: exactcheck2()
 *
 * Description:
 *  Determine whether a pointer is within the specified bounds of an object.
 *
 * Inputs:
 *  base   - The address of the first byte of a memory object.
 *  result - The pointer that is being checked.
 *  size   - The size of the object in bytes.
 *
 * Return value:
 *  If there is no bounds check violation, the result pointer is returned.
 *  Otherwise, depending upon the configuration of the run-time, either an
 *  error is returned or a rewritten Out-of-Bounds (OOB) pointer is returned.
 */
void *
bb_exactcheck2 (const char *base, const char *result, unsigned size) {
  /*
   * If the pointer is within the object, the check passes.  Return the checked
   * pointer.
   */
  if ((result >= base) && (result < (base + size))) {
    return (void*)result;
  }

  return exactcheck_check (base, base + size-1, result, NULL, 0);
}

/*
 * Function: exactcheck2_debug()
 *
 * Description:
 *  This function is identical to exactcheck2(), but the caller provides more
 *  source level information about the run-time check for error reporting if
 *  the check fails.
 *
 * Inputs:
 *  base   - The address of the first byte of a memory object.
 *  result - The pointer that is being checked.
 *  size   - The size of the object in bytes.
 *
 * Return value:
 *  If there is no bounds check violation, the result pointer is returned.
 *  This forces the call to exactcheck() to be considered live (previous
 *  optimizations dead-code eliminated it).
 */
void *
bb_exactcheck2_debug (const char *base,
                   const char *result,
                   unsigned size,
                   unsigned tag,
                   const char * SourceFile,
                   unsigned lineno) {
  /*
   * If the pointer is within the object, the check passes.  Return the checked
   * pointer.
   */
  if ((result >= base) && (result < (base + size))) {
    return (void*) result;
  }

  return exactcheck_check (base, base + size - 1, result, SourceFile, lineno);
}

/*
 * Function: exactcheck_check()
 *
 * Description:
 *  This is the slow path for an exactcheck.  It handles pointer rewriting
 *  and error reporting when an exactcheck fails.
 *
 * Inputs:
 *  ObjStart - The address of the first valid byte of the object.
 *  ObjEnd   - The address of the last valid byte of the object.
 *  Dest     - The result pointer of the indexing operation (the GEP).
 *  SourceFile - The name of the file in which the check occurs.
 *  lineno     - The line number within the file in which the check occurs.
 */
void *
exactcheck_check (const void * ObjStart,
                  const void * ObjEnd,
                  const void * Dest,
                  const char * SourceFile,
                  unsigned int lineno) {

  /*
   * First, we know that the pointer is out of bounds.  If we indexed off the
   * beginning or end of a valid object, determine if we can rewrite the
   * pointer into an OOB pointer.  Whether we can or not depends upon the
   * SAFECode configuration.
   */
  if ((!(ConfigData.StrictIndexing)) ||
      (((char *) Dest) == (((char *)ObjEnd)+1))) {
    void *ptr = (void *)((uintptr_t)Dest | SET_MASK);
    return ptr;
  } else {

    OutOfBoundsViolation v;
    v.type = ViolationInfo::FAULT_OUT_OF_BOUNDS,
      v.faultPC = __builtin_return_address(0),
      v.faultPtr = Dest,
      v.PoolHandle = 0,
      v.dbgMetaData = NULL,
      v.SourceFile = SourceFile,
      v.objStart = ObjStart,
      v.objLen = (unsigned)((const char*)ObjEnd - (const char*)ObjStart + 1),
      v.lineNo = lineno;
    
    ReportMemoryViolation(&v);
  }

  return const_cast<void*>(Dest);
}

