//===------- unistd.cpp - CStdLib Runtime functions for <unistd.h> -------===//
// 
//                          The SAFECode Compiler
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file provides CStdLib runtime wrapper versions of functions found in
// <unistd.h>.
//
//===----------------------------------------------------------------------===//

#include <unistd.h>

#include "CStdLib.h"

//
// Function: pool_getcwd()
//
// Inputs
//   BufPool      - The pool handle for Buf
//   Buf          - Buffer to fill with current directory path
//   Size         - Size of the array referenced by Buf
//   Complete     - Completeness bit vector
//   TAG          - Tag information for debugging purposes
//   SRC_INFO     - Source and line information for debugging purposes
//
// Returns
//   Returns a pointer to a buffer containing the absolute path to the current
//   working directory if successful, and returns NULL on error.
//
char *
pool_getcwd_debug (DebugPoolTy * BufPool,
                   char * Buf,
                   size_t Size,
                   const uint8_t Complete,
                   TAG,
                   SRC_INFO) {
  //
  // Buf can be NULL; perform checks only when it is non-NULL.
  //
  if (Buf) {
    const bool BufComplete = ARG1_COMPLETE (Complete);

    //
    // Ensure that the size of Buf is at least the size specified by Size.
    //
    minSizeCheck (BufPool, Buf, BufComplete, Size, SRC_INFO_ARGS);
  }

  return getcwd (Buf, Size);
}

char *
pool_getcwd (DebugPoolTy * BufPool,
             char * Buf,
             size_t Size,
             const uint8_t Complete) {
  return pool_getcwd_debug (BufPool, Buf, Size, Complete, DEFAULTS);
}
