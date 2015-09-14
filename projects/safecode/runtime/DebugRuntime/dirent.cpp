//===------- dirent.cpp - CStdLib Runtime functions for <dirent.h> -------===//
// 
//                          The SAFECode Compiler
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file provides CStdLib runtime wrapper versions of functions found in
// <dirent.h>.
//
//===----------------------------------------------------------------------===//

#include <dirent.h>

#include "CStdLib.h"

//
// Function: pool_readdir()
//
// Inputs
//   EntryPool    - The pool handle for Entry
//   ResultPool   - The pool handle for Result
//   Entry        - The second argument of readdir_r
//   Result       - The third argument of readdir_r
//   DPtr         - The first argument of readdir_r
//   Complete     - Completeness bit vector
//   TAG          - Tag information for debugging purposes
//   SRC_INFO     - Source and line information for debugging purposes
//
// Returns
//   0 on success, otherwise an error code
//
int
pool_readdir_r_debug (DebugPoolTy *EntryPool,
                      DebugPoolTy *ResultPool,
                      void *Entry,
                      void *Result,
                      void *DPtr,
                      const uint8_t Complete,
                      TAG,
                      SRC_INFO) {
  const bool EntryComplete = ARG1_COMPLETE(Complete);
  const size_t DirentSize = sizeof(struct dirent);

  const bool ResultComplete = ARG2_COMPLETE(Complete);
  const size_t PtrSize = sizeof(struct dirent *);

  minSizeCheck(EntryPool, Entry, EntryComplete, DirentSize, SRC_INFO_ARGS);
  minSizeCheck(ResultPool, Result, ResultComplete, PtrSize, SRC_INFO_ARGS);

  return readdir_r(
    (DIR *) DPtr, (struct dirent *) Entry, (struct dirent **) Result
  );
}

int
pool_readdir_r (DebugPoolTy *EntryPool,
                DebugPoolTy *ResultPool,
                void *Entry,
                void *Result,
                void *DPtr,
                const uint8_t Complete) {
  return pool_readdir_r_debug (
    EntryPool, ResultPool, Entry, Result, DPtr, Complete, DEFAULTS
  );
}
