//===- GEPChecks.cpp - Insert GEP run-time checks ------------------------- --//
// 
//                     The LLVM Compiler Infrastructure
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This pass instruments GEPs with run-time checks to ensure safe array and
// structure indexing.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "safecode"

#include "llvm/ADT/Statistic.h"
#include "llvm/Support/CommandLine.h"
#include "safecode/GEPChecks.h"
#include "safecode/Utility.h"

namespace llvm {

char InsertGEPChecks::ID = 0;

static RegisterPass<InsertGEPChecks>
X ("gepchecks", "Insert GEP run-time checks");

//
// Command Line Options
//

// Disable checks on pure structure indexing
cl::opt<bool> DisableStructChecks ("disable-structgepchecks", cl::Hidden,
                                   cl::init(false),
                                   cl::desc("Disable Struct GEP Checks"));

// Pass Statistics
namespace {
  STATISTIC (GEPChecks, "Bounds Checks Added");
  STATISTIC (SafeGEP,   "GEPs proven safe by SAFECode");
}

//
// Method: visitGetElementPtrInst()
//
// Description:
//  This method checks to see if the specified GEP is safe.  If it cannot prove
//  it safe, it then adds a run-time check for it.
//
void
InsertGEPChecks::visitGetElementPtrInst (GetElementPtrInst & GEP) {
  //
  // Don't insert a check if GEP only indexes into a structure and the
  // user doesn't want to do structure index checking.
  //
  if (DisableStructChecks && indexesStructsOnly (&GEP)) {
    return;
  }

  //
  // Get the function in which the GEP instruction lives.
  //
  Value * PH = ConstantPointerNull::get (getVoidPtrType(GEP.getContext()));
  BasicBlock::iterator InsertPt = &GEP;
  ++InsertPt;
  Instruction * ResultPtr = castTo (&GEP,
                                    getVoidPtrType(GEP.getContext()),
                                    GEP.getName() + ".cast",
                                    InsertPt);

  //
  // Make this an actual cast instruction; it will make it easier to update
  // DSA.
  //
  Value * SrcPtr = castTo (GEP.getPointerOperand(),
                           getVoidPtrType(GEP.getContext()),
                           GEP.getName()+".cast",
                           InsertPt);

  //
  // Create the call to the run-time check.
  //
  std::vector<Value *> args(1, PH);
  args.push_back (SrcPtr);
  args.push_back (ResultPtr);
  CallInst * CI = CallInst::Create (PoolCheckArrayUI, args, "", InsertPt);

  //
  // Add debugging info metadata to the run-time check.
  //
  if (MDNode * MD = GEP.getMetadata ("dbg"))
    CI->setMetadata ("dbg", MD);

  //
  // Update the statistics.
  //
  ++GEPChecks;
  return;
}

//
// Method: doInitialization()
//
// Description:
//  Perform module-level initialization before the pass is run.  For this
//  pass, we need to create a function prototype for the GEP check function.
//
// Inputs:
//  M - A reference to the LLVM module to modify.
//
// Return value:
//  true - This LLVM module has been modified.
//
bool
InsertGEPChecks::doInitialization (Module & M) {
  //
  // Create a function prototype for the function that performs incomplete
  // pointer arithmetic (GEP) checks.
  //
  Type * VoidPtrTy = getVoidPtrType (M.getContext());
  Constant * F = M.getOrInsertFunction ("boundscheckui",
                                        VoidPtrTy,
                                        VoidPtrTy,
                                        VoidPtrTy,
                                        VoidPtrTy,
                                        NULL);

  //
  // Mark the function as readonly; that will enable it to be hoisted out of
  // loops by the standard loop optimization passes.
  //
  (cast<Function>(F))->addFnAttr (Attribute::ReadOnly);
  return true;
}

bool
InsertGEPChecks::runOnFunction (Function & F) {
  //
  // Get pointers to required analysis passes.
  //
  TD      = &F.getParent()->getDataLayout();
  abcPass = &getAnalysis<ArrayBoundsCheckLocal>();

  //
  // Get a pointer to the run-time check function.
  //
  PoolCheckArrayUI = F.getParent()->getFunction ("boundscheckui");

  //
  // Visit all of the instructions in the function.
  //
  visit (F);
  return true;
}

}

