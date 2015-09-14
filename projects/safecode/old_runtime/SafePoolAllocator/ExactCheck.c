/*===- ExactCheck.c - Implementation of exactcheck functions --------------===*/
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

#include "ConfigData.h"
#include "ExactCheck.h"
#include "Report.h"

#include "safecode/Config/config.h"

#ifdef LLVA_KERNEL
#include <stdarg.h>
#else
#include <stdio.h>
#endif
#define DEBUG(x) 


/* Decleare this structure type */
struct PoolTy;

/* Function to rewriting pointers to Out Of Bounds (OOB) Pointers */
extern void * rewrite_ptr (struct PoolTy * P,
                           void * p,
                           void * S,
                           void * E,
                           void * SF,
                           unsigned l);

/* Toggle whether we'll log debug data */
static int logregs = 0;

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
exactcheck_check (void * ObjStart,
                  void * ObjEnd,
                  void * Dest,
                  void * SourceFile,
                  unsigned int lineno) {
  /*
   * First, we know that the pointer is out of bounds.  If we indexed off the
   * beginning or end of a valid object, determine if we can rewrite the
   * pointer into an OOB pointer.  Whether we can or not depends upon the
   * SAFECode configuration.
   */
  if ((!(ConfigData.StrictIndexing)) ||
      (((char *) Dest) == (((char *)ObjEnd)+1))) {
    void * ptr = rewrite_ptr (0, Dest, ObjStart, ObjEnd, SourceFile, lineno);
    if (logregs)
      fprintf (ReportLog, "exactcheck: rewrite(1): %p %p %p at pc=%p to %p: %s %d\n",
               ObjStart, ObjEnd, Dest, (void*)__builtin_return_address(1), ptr, (unsigned char *)SourceFile, lineno);
      fflush (ReportLog);
    return ptr;
  } else {
    //
    // Determine if this is a rewrite pointer that is being indexed.
    //
    if ((logregs) && (((unsigned)Dest > (unsigned)0xc0000000))) {
      fprintf (stderr, "Was a rewrite: %p\n", Dest);
      fflush (stderr);
    }
    ReportExactCheck ((unsigned)0xbeefdeed,
                      (uintptr_t)Dest,
                      (uintptr_t)__builtin_return_address(0),
                      (uintptr_t)ObjStart,
                      (unsigned)ObjEnd - (unsigned)ObjStart,
                      (unsigned char *)(SourceFile),
                      lineno);
  }

  return Dest;
}

/*
 * Function: exactcheck()
 *
 * Description:
 *  Determine whether the index is within the specified bounds.
 *
 * Inputs:
 *  a      - The index given as an integer.
 *  b      - The index of one past the end of the array.
 *  result - The pointer that is being checked.
 *
 * Return value:
 *  If there is no bounds check violation, the result pointer is returned.
 *  This forces the call to exactcheck() to be considered live (previous
 *  optimizations dead-code eliminated it).
 */
void *
exactcheck (int a, int b, void * result) {
  if ((0 > a) || (a >= b)) {
    ReportExactCheck ((unsigned)0xbeefdeed,
                      (uintptr_t)result,
                      (uintptr_t)__builtin_return_address(0),
                      (unsigned)a,
                      (unsigned)0,
                      "<Unknown>",
                      0);
#if 0
    poolcheckfail ("exact check failed", (a), (void*)__builtin_return_address(0));
    poolcheckfail ("exact check failed", (b), (void*)__builtin_return_address(0));
#endif
  }
  return result;
}

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
exactcheck2 (signed char *base, signed char *result, unsigned size) {
  /*
   * If the pointer is within the object, the check passes.  Return the checked
   * pointer.
   */
  if ((result >= base) && (result < (base + size))) {
    return result;
  }

  return exactcheck_check (base, base + size-1, result, "<Unknown>", 0);
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
exactcheck2_debug (signed char *base,
                   signed char *result,
                   unsigned size,
                   void * SourceFile,
                   unsigned lineno) {
  /*
   * If the pointer is within the object, the check passes.  Return the checked
   * pointer.
   */
  if ((result >= base) && (result < (base + size))) {
    return result;
  }

  return exactcheck_check (base, base + size - 1, result, SourceFile, lineno);
}

void *
exactcheck2a (signed char *base, signed char *result, unsigned size) {
  if (result >= base + size ) {
    ReportExactCheck ((unsigned)0xbeefdeed,
                      (uintptr_t)result,
                      (uintptr_t)__builtin_return_address(0),
                      (uintptr_t)base,
                      (unsigned)size,
                      "<Unknown>",
                      0);
  }
  return result;
}

void *
exactcheck3(signed char *base, signed char *result, signed char * end) {
  if ((result < base) || (result > end )) {
    ReportExactCheck ((unsigned)0xbeefbeef,
                      (uintptr_t)result,
                      (uintptr_t)__builtin_return_address(0),
                      (uintptr_t)base,
                      (unsigned)(end-base),
                      "<Unknown>",
                      0);
  }
  return result;
}

