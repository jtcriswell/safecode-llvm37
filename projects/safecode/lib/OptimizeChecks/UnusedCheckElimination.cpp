//===- OptimizeChecks.cpp - Optimize SAFECode Checks ---------------------- --//
// 
//                          The SAFECode Compiler 
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This pass eliminates unused checks.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "opt-safecode"

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"

#include "safecode/OptimizeChecks.h"
#include "SCUtils.h"

#include "dsa/DSSupport.h"

#include <iostream>
#include <set>

namespace {
  struct DeleteItself {
    void operator()(llvm::CallInst * CI) {
      CI->eraseFromParent();
    }
  };
}

NAMESPACE_SC_BEGIN

char UnusedCheckElimination::ID = 0;

namespace {
  // Pass Statistics
  STATISTIC (Removed, "Number of checks on unused pointers removed");
}

static RegisterPass<UnusedCheckElimination>
X ("unused-check-elim", "Unused Check elimination");

bool
UnusedCheckElimination::runOnModule (Module & M) {
  //
  // Get prerequisite analysis results.
  //
  unusedChecks.clear();
  intrinsic = &getAnalysis<InsertSCIntrinsic>();

  //
  // Scan through the use/def chains of all the run-time checks.  If the
  // pointer being checked is never used, then eliminate the check.
  //
  InsertSCIntrinsic::intrinsic_const_iterator i = intrinsic->intrinsic_begin();
  InsertSCIntrinsic::intrinsic_const_iterator e = intrinsic->intrinsic_end();
  for (; i != e; ++i) {
    if (i->flag & (InsertSCIntrinsic::SC_INTRINSIC_CHECK |
                   InsertSCIntrinsic::SC_INTRINSIC_OOB)) {
      for (Value::use_iterator I = i->F->use_begin(), E = i->F->use_end();
           I != E;
           ++I) {
        //
        // Get the pointer that the run-time check is checking.  Strip off the
        // casts because the cast may have no uses but the pointer it comes
        // from may have uses (other than the casts).
        //
        CallInst * CI = cast<CallInst>(*I);
        if (Value * CheckedPointer = intrinsic->getValuePointer(CI)) {
          CheckedPointer = CheckedPointer->stripPointerCasts();

          //
          // If the checked pointer has no uses, schedule the run-time check for
          // deletion.
          //
          if (CheckedPointer->use_empty()) unusedChecks.push_back(CI);
        }
      }
    }
  }

  //
  // Delete all unneeded run-time checks.
  //
  DeleteItself op;
  std::for_each(unusedChecks.begin(), unusedChecks.end(), op);

  //
  // Record whether we modified the program and update the statistics.  Note
  // that we update the statistics because this pass could be run multipe
  // times, and we want the total number of eliminated checks.
  //
  bool modified = unusedChecks.size() > 0;
  Removed += unusedChecks.size();

  //
  // Free memory and finish the pass.
  //
  unusedChecks.clear();
  return modified;
}

NAMESPACE_SC_END

