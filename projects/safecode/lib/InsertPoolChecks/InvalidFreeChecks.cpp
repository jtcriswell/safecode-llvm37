//===- InvalidFreeChecks.cpp - Insert invalid free run-time checks -------- --//
// 
//                     The LLVM Compiler Infrastructure
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This pass instruments calls to deallocators to ensure memory safety.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "safecode"

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Constants.h"
#include "safecode/InvalidFreeChecks.h"
#include "safecode/Utility.h"

namespace llvm {

char InsertFreeChecks::ID = 0;

static RegisterPass<InsertFreeChecks>
X ("freechecks", "Insert invalid free run-time checks");

// Pass Statistics
namespace {
  STATISTIC (FreeChecks, "Invalid Free Checks Added");
}

//
// Method: visitCallSite()
//
// Description:
//  See if this is a call to a deallocator and, if so, put a check on it.
//
void
InsertFreeChecks::visitCallSite (CallSite & CS) {
  //
  // Determine if this is a call to a deallocation function.  If not, then
  // ignore it.
  //
  Value * CalledValue = CS.getCalledValue()->stripPointerCasts();
  if (Function * F = dyn_cast<Function>(CalledValue)) {
    if (F->hasName()) {
      if (F->getName() != "free") {
        return;
      }
    }
  } else {
    return;
  }

  //
  // Get a pointer to the run-time check function.
  //
  Instruction * InsertPt = CS.getInstruction();
  Module * M = InsertPt->getParent()->getParent()->getParent();
  Function * PoolFreeCheck = M->getFunction ("poolcheck_freeui");
  assert (PoolFreeCheck && "Invalid free check function has disappeared!\n");

  //
  // Create an STL container with the arguments.
  // The first argument is the pool handle (which is a NULL pointer).
  // The second argument is the pointer to check.
  //
  std::vector<Value *> args;
  LLVMContext & Context = M->getContext();
  args.push_back(ConstantPointerNull::get (getVoidPtrType(Context)));
  args.push_back(castTo (CS.getArgument(0), getVoidPtrType(Context), InsertPt));

  //
  // Create the call to the run-time check.  Place it *before* the load
  // instruction.
  //
  CallInst * CI = CallInst::Create (PoolFreeCheck, args, "", InsertPt);

  //
  // If there's debug information on the load instruction, add it to the
  // run-time check.
  //
  if (MDNode * MD = InsertPt->getMetadata ("dbg"))
    CI->setMetadata ("dbg", MD);

  //
  // Update the statistics.
  //
  ++FreeChecks;
  return;
}

//
// Method: doInitialization()
//
// Description:
//  Perform module-level initialization before the pass is run.  For this
//  pass, we need to create a function prototype for the invalid free check
//  function.
//
// Inputs:
//  M - A reference to the LLVM module to modify.
//
// Return value:
//  true - This LLVM module has been modified.
//
bool
InsertFreeChecks::doInitialization (Module & M) {
  //
  // Create a function prototype for the function that performs incomplete
  // load/store checks.
  //
  Type * VoidTy  = Type::getVoidTy (M.getContext());
  Type * VoidPtrTy = getVoidPtrType (M.getContext());
  M.getOrInsertFunction ("poolcheck_freeui",
                         VoidTy,
                         VoidPtrTy,
                         VoidPtrTy,
                         NULL);
  return true;
}

bool
InsertFreeChecks::runOnFunction (Function & F) {
  //
  // Visit all of the instructions in the function.
  //
  visit (F);
  return true;
}

}

