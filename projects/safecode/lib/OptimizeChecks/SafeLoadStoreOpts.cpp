//===- SafeLoadStoreOpts.cpp - Optimize safe load/store checks ------------ --//
// 
//                          The SAFECode Compiler 
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This pass removes load/store checks that are known to be safe statically.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "typesafe-lsopt"

#include "safecode/SafeLoadStoreOpts.h"

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Module.h"

namespace llvm {

char OptimizeSafeLoadStore::ID = 0;

static RegisterPass<OptimizeSafeLoadStore>
X ("opt-safels", "Remove safe load/store runtime checks");

// Pass Statistics
namespace {
  STATISTIC (TypeSafeChecksRemoved , "Type-safe Load/Store Checks Removed");
  STATISTIC (TrivialChecksRemoved ,  "Trivial Load/Store Checks Removed");
}

bool
OptimizeSafeLoadStore::runOnModule(Module & M) {
  //
  // Determine if there is anything to check.
  //
  Function * LSCheck = M.getFunction ("poolcheck");
  if (!LSCheck)
    return false;

  //
  // Get access to prerequisite passes.
  //
  dsa::TypeSafety<EQTDDataStructures> & TS = getAnalysis<dsa::TypeSafety<EQTDDataStructures> >();

  //
  // Scan through all uses of the complete run-time check and record any checks
  // on type-known pointers.  These can be removed.
  //
  // TODO: This code should also work on fastlscheck calls.
  //
  std::vector <CallInst *> toRemoveTypeSafe;
  std::vector <CallInst *> toRemoveObvious;
  Value::use_iterator UI = LSCheck->use_begin();
  Value::use_iterator  E = LSCheck->use_end();
  for (; UI != E; ++UI) {
    if (CallInst * CI = dyn_cast<CallInst>(*UI)) {
      if (CI->getCalledValue()->stripPointerCasts() == LSCheck) {
        //
        // Get the pointer that is checked by this run-time check.
        //
        CallSite CS(CI);
        Value * CheckPtr = CS.getArgument(1)->stripPointerCasts();
        CheckPtr->dump();

        //
        // If it is obvious that the pointer is within a valid object, then
        // remove the check.
        //
        if ((isa<AllocaInst>(CheckPtr)) || isa<GlobalVariable>(CheckPtr)) {
            toRemoveObvious.push_back (CI);
            continue;
        }

        //
        // If the pointer is complete, then remove the check if it points to
        // a type-consistent object.
        //
        Function * F = CI->getParent()->getParent();
        if (TS.isTypeSafe (CheckPtr, F)) {
          toRemoveTypeSafe.push_back (CI);
          continue;
        }
      }
    }
  }

  //
  // Update statistics.  Note that we only assign if the value is non-zero;
  // this prevents the statistics from being reported if the value is zero.
  //
  bool modified = false;
  if (toRemoveTypeSafe.size()) {
    TypeSafeChecksRemoved += toRemoveTypeSafe.size();
    modified = true;
  }

  if (toRemoveObvious.size()) {
    TrivialChecksRemoved += toRemoveObvious.size();
    modified = true;
  }

  //
  // Now iterate through all of the call sites and transform them to be
  // complete.
  //
  for (unsigned index = 0; index < toRemoveObvious.size(); ++index) {
    toRemoveObvious[index]->eraseFromParent();
  }

  for (unsigned index = 0; index < toRemoveTypeSafe.size(); ++index) {
    toRemoveTypeSafe[index]->eraseFromParent();
  }

  return modified;
}

}
