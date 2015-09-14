//===- OptimizeChecks.cpp - Optimize SAFECode Checks ---------------------- --//
// 
//                          The SAFECode Compiler 
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This pass performs optimizations on the SAFECode checks.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "opt-safecode"

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"

#include "safecode/OptimizeChecks.h"
#include "safecode/Utility.h"

#include <iostream>
#include <set>

char llvm::OptimizeChecks::ID = 0;

namespace {
  STATISTIC (Removed, "Number of Bounds Checks Removed");
}

namespace llvm {

static RegisterPass<OptimizeChecks>
X ("opt-checks", "Optimize run-time checks", true);

//
// Function: onlyUsedInCompares()
//
// Description:
//  Determine whether the result of the given instruction is only used in
//  comparisons.
//
// Return value:
//  true  - The instruction is only used in comparisons.
//  false - The instruction has some other use besides comparisons.
//
bool
OptimizeChecks::onlyUsedInCompares (Value * Val) {
  // The worklist
  std::vector<Value *> Worklist;

  // The set of processed values
  std::set<Value *> Processed;

  //
  // Initialize the worklist.
  //
  Worklist.push_back(Val);

  //
  // Process each item in the work list.
  //
  while (Worklist.size()) {
    Value * V = Worklist.back();
    Worklist.pop_back();

    //
    // Check whether we have already processed this value.  If not, mark it as
    // processed.
    //
    if (Processed.find (V) != Processed.end()) continue;
    Processed.insert (V);

    //
    // Scan through all the uses of this value.  Some uses may be safe.  Other
    // uses may generate uses we need to check.  Still others are known-bad
    // uses.  Handle each appropriately.
    //
    for (Value::use_iterator U = V->use_begin(); U != V->use_end(); ++U) {
      // Compares are okay
      if (isa<CmpInst>(*U)) continue;

      // Casts, Phi nodes, and GEPs require that we check the result, too.
      if (isa<CastInst>(*U) ||
          isa<PHINode>(*U)  ||
          isa<BinaryOperator>(*U)  ||
          isa<SelectInst>(*U)  ||
          isa<SwitchInst>(*U)  ||
          isa<GetElementPtrInst>(*U)) {
        Worklist.push_back(*U);
        continue;
      }

      if (ConstantExpr * CE = dyn_cast<ConstantExpr>(*U)) {
        // Casts and compares are okay
        if (CE->isCast() || CE->isCompare()) {
          Worklist.push_back(*U);
          continue;
        }

        // Selects and GEPs are also okay
        switch (CE->getOpcode()) {
          case Instruction::GetElementPtr:
          case Instruction::Select:
            Worklist.push_back(*U);
            continue;
            break;
          default:
            return false;
        }
      }

      // Calls to run-time functions are okay; others are not.
      if (CallInst * CI = dyn_cast<CallInst>(*U)) {
        Value * CV = CI->getCalledValue()->stripPointerCasts();
        if (Function * F = dyn_cast<Function>(CV)) {
          if (isRuntimeCheck (F)) {
            continue;
          }
        }
      }

      //
      // We don't know what this is; just assume it is bad.
      //
      return false;
    }
  }

  //
  // All uses are comparisons.  Return true.
  //
  return true;
}

//
// Method: processFunction()
//
// Description:
//  Look for calls of the specified function (which is a SAFECode run-time
//  check), determine whether the call can be eliminated, and eliminate it
//  if possible.
//
// Inputs:
//  M    - A reference to the LLVM module.
//  Info - Information about the SAFECode run-time check.
//
// Return value:
//  false - No modifications were made to the Module.
//  true  - One or more modifications were made to the module.
//
bool
OptimizeChecks::processFunction (Module & M, const CheckInfo & Info) {
  //
  // Get the runtime function in the code.  If no calls to the run-time
  // function were added to the code, do nothing.
  //
  Function * F = M.getFunction (Info.name);
  if (!F)
    return false;

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
      // If the call instruction has any uses, we cannot remove it.
      //
      if (CI->use_begin() != CI->use_end()) continue;

      //
      // Get the operand that needs to be replaced as well as the operand
      // with all of the casts peeled away.  Increment the operand index by
      // one because a call instruction's first operand is the function to
      // call.
      //
      Value * Operand = Info.getCheckedPointer (CI)->stripPointerCasts();

      //
      // If the operand is only used in comparisons, mark the run-time check
      // for removal.
      //
      if (onlyUsedInCompares (Operand)) {
        CallsToDelete.push_back (CI);
        modified = true;
      }
    }
  }

  //
  // Update the statistics.
  //
  if (CallsToDelete.size())
    Removed += CallsToDelete.size();

  //
  // Remove all of the instructions that we found to be unnecessary.
  //
  for (unsigned index = 0; index < CallsToDelete.size(); ++index) {
    CallsToDelete[index]->eraseFromParent();
  }

  return modified;
}

bool
OptimizeChecks::runOnModule (Module & M) {
  //
  // Optimize all of the run-time GEP checks.
  //
  bool modified = false;
  for (unsigned index = 0; index < numChecks; ++index) {
    if (RuntimeChecks[index].checkType == gepcheck) {
      //
      // Analyze calls to this run-time check and remove them if possible.
      //
      modified |= processFunction (M, RuntimeChecks[index]);
    }
  }

  return modified;
}

}

