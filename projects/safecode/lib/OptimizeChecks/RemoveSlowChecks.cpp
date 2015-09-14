//===- RemoveSlowChecks.cpp - Remove Slow Checks -------------------------- --//
// 
//                          The SAFECode Compiler 
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This pass removes run-time checks which are too expensive because the
// referant object must be found for the checked pointer.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "slowchecks"

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

#include <vector>

namespace {
  STATISTIC (Removed, "Number of Slow Checks Removed");
}

// List of slow run-time checks
static const char * slowChecks[] = {
  "poolcheck",
  "poolcheckui",
  "boundscheck",
  "boundscheck_ui",
  0
};

namespace llvm {
  //
  // Pass: RemoveSlowChecks
  //
  // Description:
  //  This pass removes run-time checks that are too slow.
  //
  struct RemoveSlowChecks : public ModulePass {
   public:
    static char ID;
   RemoveSlowChecks() : ModulePass(ID) {}
    virtual bool runOnModule (Module & M);
    const char *getPassName() const {
      return "Remove slow checks transform";
    }
    
    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.setPreservesCFG();
    }

   private:
     bool removeCheck (Module & M, Function * F);
  };
}

//
// Method: removeCheck()
//
// Description:
//  Remove run-time checks to the specified function.
//
// Return value:
//  true  - One or more calls to the check were removed.
//  false - No calls to the check were removed.
//
bool
llvm::RemoveSlowChecks::removeCheck (Module & M, Function * F) {
  //
  // Get the runtime function in the code.  If no calls to the run-time
  // function were added to the code, do nothing.
  //
  if (!F) return false;

  //
  // Iterate though all calls to the function and search for pointers that are
  // checked but only used in comparisons.  If so, then schedule the check
  // (i.e., the call) for removal.
  //
  bool modified = false;
  std::vector<Instruction *> CallsToDelete;
  for (Value::use_iterator FU = F->use_begin(); FU != F->use_end(); ++FU) {
    //
    // We are only concerned about call instructions; any other use is of
    // no interest to the organization.
    //
    if (CallInst * CI = dyn_cast<CallInst>(*FU)) {
      //
      // If the call instruction has no uses, we can remove it.
      //
      if (CI->use_begin() == CI->use_end())
        CallsToDelete.push_back (CI);
    }
  }

  //
  // Update the statistics and determine if we will modify anything.
  //
  if (CallsToDelete.size()) {
    modified = true;
    Removed += CallsToDelete.size();
  }

  //
  // Remove all of the instructions that we found to be unnecessary.
  //
  for (unsigned index = 0; index < CallsToDelete.size(); ++index) {
    CallsToDelete[index]->eraseFromParent();
  }

  return modified;
}

bool
llvm::RemoveSlowChecks::runOnModule (Module & M) {
  //
  // Optimize all of the run-time GEP checks.
  //
  bool modified = false;
  for (unsigned index = 0; slowChecks[index]; ++index) {
    //
    // Analyze calls to this run-time check and remove them if possible.
    //
    modified |= removeCheck (M, M.getFunction (slowChecks[index]));
  }

  return modified;
}

namespace llvm {
  char RemoveSlowChecks::ID = 0;

  static RegisterPass<RemoveSlowChecks>
  X ("rm-slowchecks", "Remove slow run-time checks", true);

  ModulePass * createRemoveSlowChecksPass (void) {
    return new RemoveSlowChecks();
  }
}
