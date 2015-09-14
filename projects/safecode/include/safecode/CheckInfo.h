//===- CheckInfo.h - Information about SAFECode run-time checks --*- C++ -*---//
// 
//                     The LLVM Compiler Infrastructure
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file implements structures containing data about the various run-time
// checks that SAFECode inserts into code.
//
//===----------------------------------------------------------------------===//

#ifndef _SC_CHECKINFO_H_
#define _SC_CHECKINFO_H_

#include "llvm/IR/CallSite.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"

using namespace llvm;

namespace {

enum CheckType {memcheck, gepcheck, funccheck, strcheck};

//
// Structure: CheckInfo
//
// Description:
//  This structure describes a run-time check.
//
struct CheckInfo {
  // The name of the function implementing the run-time check
  const char * name;

  // The name of the complete version of the check
  const char * completeName;

  // The argument of the checked pointer.
  unsigned char argno;

  // A boolean indicating whether it is a memory check or a bounds check
  CheckType checkType;

  // The argument of the length (if appropriate)
  unsigned char lenArg;

  // A boolean indicating whether the check is complete
  bool isComplete;

  // The argument of the source pointer (if appropriate)
  unsigned char srcArg;

  bool isMemCheck (void) const {
    return checkType == memcheck;
  }

  bool isGEPCheck (void) const {
    return checkType == gepcheck;
  }

  Value * getCheckedPointer (CallInst * CI) const {
    CallSite CS(CI);
    return CS.getArgument (argno);
  }

  Value * getCheckedLength (CallInst * CI) const {
    if (lenArg) {
      CallSite CS(CI);
      return CS.getArgument (lenArg);
    }

    return 0;
  }

  Value * getSourcePointer (CallInst * CI) const {
    if (srcArg) {
      CallSite CS(CI);
      return CS.getArgument (srcArg);
    }

    return NULL;
  }
};

//
// Create a table describing all of the SAFECode run-time checks.
//
static const unsigned numChecks = 24;

static const struct CheckInfo RuntimeChecks[numChecks] = {
  // Regular checking functions
  {"poolcheck",        "poolcheck",      1, memcheck,  2, true,  0},
  {"poolcheckui",      "poolcheck",      1, memcheck,  2, false, 0},
  {"poolcheckalign",   "poolcheckalign", 1, memcheck,  0, true,  0},
  {"poolcheckalignui", "poolcheckalign", 1, memcheck,  0, false, 0},
  {"poolcheckstr",     "poolcheckstr",   1, strcheck,  0, true,  0},
  {"poolcheckstrui",   "poolcheckstr",   1, strcheck,  0, false, 0},
  {"boundscheck",      "boundscheck",    2, gepcheck,  0, true,  1},
  {"boundscheckui",    "boundscheck",    2, gepcheck,  0, false, 1},
  {"exactcheck2",      "exactcheck2",    2, gepcheck,  0, true,  1},
  {"fastlscheck",      "fastlscheck",    1, memcheck,  3, true,  0},
  {"funccheck",        "funccheck",      0, funccheck, 0, true,  0},
  {"funccheckui",      "funccheck",      0, funccheck, 0, false, 0},

  // Debug versions of the above
  {"poolcheck_debug",        "poolcheck_debug",      1, memcheck, 2, true,  0},
  {"poolcheckui_debug",      "poolcheck_debug",      1, memcheck, 2, false, 0},
  {"poolcheckalign_debug",   "poolcheckalign_debug", 1, memcheck, 0, true,  0},
  {"poolcheckalignui_debug", "poolcheckalign_debug", 1, memcheck, 0, false, 0},
  {"poolcheckstr_debug",     "poolcheckstr_debug",   1, strcheck, 0, true,  0},
  {"poolcheckstrui_debug",   "poolcheckstr_debug",   1, strcheck, 0, false, 0},
  {"boundscheck_debug",      "boundscheck_debug",    2, gepcheck, 0, true,  1},
  {"boundscheckui_debug",    "boundscheck_debug",    2, gepcheck, 0, false, 1},
  {"exactcheck2_debug",      "exactcheck2_debug",    2, gepcheck, 0, true,  1},
  {"fastlscheck_debug",      "fastlscheck_debug",    1, memcheck, 3, true,  0},
  {"funccheck_debug",        "funccheck_debug",     0, funccheck, 0, true,  0},
  {"funccheckui_debug",      "funccheck_debug",     0, funccheck, 0, false, 0}
};

//
// Function: isRuntimeCheck()
//
// Description:
//  Determine whether the function is one of the run-time checking functions.
//
// Return value:
//  true  - The function is a run-time check.
//  false - The function is not a run-time check.
//
static inline bool
isRuntimeCheck (const Function * F) {
  if (F->hasName()) {
    for (unsigned index = 0; index < numChecks; ++index) {
      if (F->getName() == RuntimeChecks[index].name) {
        return true;
      }
    }
  }

  return false;
}

//
// Function: findRuntimeCheck()
//
// Description:
//  Determine if this function is one of the run-time checking functions.  If
//  so, return the information about the run-time check.
//
// Inputs:
//  F - The function to check.
//
// Return value:
//  NULL - This is not a call to a run-time check.
//  Otherwise, a pointer to the proper run-time check entry is returned.
//
static inline const struct CheckInfo *
findRuntimeCheck (const Function * F) {
  if (F->hasName()) {
    for (unsigned index = 0; index < numChecks; ++index) {
      if (F->getName() == RuntimeChecks[index].name) {
        return &(RuntimeChecks[index]);
      }
    }
  }

  return 0;
}

}
#endif
