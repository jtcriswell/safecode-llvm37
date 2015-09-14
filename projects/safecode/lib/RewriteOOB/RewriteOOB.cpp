//===- RewriteOOB.cpp - Rewrite Out of Bounds Pointers -------------------- --//
// 
//                          The SAFECode Compiler 
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This pass performs necessary transformations to ensure that Out of Bound
// pointer rewrites work correctly.
//
// TODO:
//  There are several optimizations which may improve performance:
//
//  1) The old code did not insert calls to getActualValue() for pointers
//     compared against a NULL pointer.  We should determine that this
//     optimization is safe and re-enable it if it is safe.
//
//  2) We insert calls to getActualValue() even if the pointer is not checked
//     by a bounds check (and hence, is never rewritten).  It's a bit tricky,
//     but we should avoid rewriting a pointer back if its bounds check was
//     removed because the resulting pointer was always used in comparisons.
//
//  3) If done properly, all loads and stores to type-unknown objects have a
//     run-time check.  Therefore, we should only need OOB pointer rewriting on
//     type-known memory objects.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "rewrite-OOB"

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "safecode/RewriteOOB.h"
#include "safecode/Utility.h"

#include <iostream>


namespace llvm {

// Identifier variable for the pass
char RewriteOOB::ID = 0;

// Statistics
STATISTIC (Changes,    "Number of Bounds Checks Modified");
STATISTIC (GetActuals, "Number of getActualValue() Calls Inserted");

// Register the pass
static RegisterPass<RewriteOOB> P ("oob-rewriter",
                                   "OOB Pointer Rewrite Transform");

//
// Method: processFunction()
//
// Description:
//  This method searches for calls to a specified run-time check.  For every
//  such call, it replaces the pointer that the call checks with the return
//  value of the call.
//
//  This allows functions like boundscheck() to return a rewrite pointer;
//  this code changes the program to use the returned rewrite pointer instead
//  of the original pointer which was passed into boundscheck().
//
// Inputs:
//  M       - The module to modify.
//  Check   - A reference to a structure describing the checking function to
//            process.
//
// Return value:
//  false - No modifications were made to the Module.
//  true  - One or more modifications were made to the module.
//
bool
RewriteOOB::processFunction (Module & M, const CheckInfo & Check) {
  //
  // Get a pointer to the checking function.  If the checking function does
  // not exist within the program, then do nothing.
  //
  Function * F = M.getFunction (Check.name);
  if (!F)
    return false;

  //
  // Ensure the function has the right number of arguments and that its
  // result is a pointer type.
  //
  assert (isa<PointerType>(F->getReturnType()));

  //
  // To avoid recalculating the dominator information each time we process a
  // use of the specified function F, we will record the function containing
  // the call instruction to F and the corresponding dominator information; we
  // will then update this information only when the next use is a call
  // instruction belonging to a different function.  We are helped by the fact
  // that iterating through uses often groups uses within the same function.
  //
  Function * CurrentFunction = 0;
  DominatorTree * domTree = 0;

  //
  // Iterate though all calls to the function and modify the use of the
  // operand to be the result of the function.
  //
  bool modified = false;
  for (Value::use_iterator FU = F->use_begin(); FU != F->use_end(); ++FU) {
    //
    // We are only concerned about call instructions; any other use is of
    // no interest to the organization.
    //
    if (CallInst * CI = dyn_cast<CallInst>(*FU)) {
      //
      // Get the operand that needs to be replaced as well as the operand
      // with all of the casts peeled away.  Increment the operand index by
      // one because a call instruction's first operand is the function to
      // call.
      //
      Value * RealOperand = Check.getCheckedPointer (CI);
      Value * PeeledOperand = RealOperand->stripPointerCasts();

      //
      // Determine if the checked pointer and the run-time check belong to
      // the same basic block.
      //
      bool inSameBlock = false;
      if (Instruction * I = dyn_cast<Instruction>(PeeledOperand)) {
        if (CI->getParent() == I->getParent()) {
          inSameBlock = true;
        }
      }

      //
      // Don't rewrite a check on a constant NULL pointer.  NULL pointers
      // never belong to a valid memory object, and trying to replace them
      // in other parts of the code simply creates problems.
      //
      if (isa<ConstantPointerNull>(PeeledOperand))
        continue;

      //
      // We're going to make a change.  Mark that we will have done so.
      //
      modified = true;

      //
      // Cast the result of the call instruction to match that of the original
      // value.
      //
      BasicBlock::iterator i(CI);
      Instruction * CastCI = castTo (CI,
                                     PeeledOperand->getType(),
                                     PeeledOperand->getName(),
                                     ++i);

      //
      // Get dominator information for the function.
      //
      if ((CI->getParent()->getParent()) != CurrentFunction) {
        CurrentFunction = CI->getParent()->getParent();
        domTree = &getAnalysis<DominatorTreeWrapperPass>(*CurrentFunction).getDomTree();
      }

      //
      // For every use that the call instruction dominates, change the use to
      // use the result of the call instruction.  We first collect the uses
      // that need to be modified before doing the modifications to avoid any
      // iterator invalidation errors.
      //
      std::vector<User *> Uses;
      Value::use_iterator UI = PeeledOperand->use_begin();
      for (; UI != PeeledOperand->use_end(); ++UI) {
        if (Instruction * Use = dyn_cast<Instruction>(*UI)) {
          if (Use->getParent()->getParent() == CurrentFunction) {
            if (isa<PHINode>(Use)) {
              if (inSameBlock) {
                Uses.push_back (UI->getUser());
                ++Changes;
              }
              continue;
            }
            if ((CI != Use) && (domTree->dominates (CI, Use))) {
              Uses.push_back (UI->getUser());
              ++Changes;
            }
          }
        }
      }

      while (Uses.size()) {
        User * Use = Uses.back();
        Uses.pop_back();

        Use->replaceUsesOfWith (PeeledOperand, CastCI);
      }
    }
  }

  return modified;
}

//
// Method: addGetActualValues()
//
// Description:
//  Search for comparison or pointer to integer cast instructions which will
//  need to turn an OOB pointer back into the original pointer value.  Insert
//  calls to getActualValue() to do the conversion.
//
// Inputs:
//  M - The module in which to add calls to getActualValue().
//
// Return value:
//  true  - The module was modified.
//  false - The module was not modified.
//
bool
RewriteOOB::addGetActualValues (Module & M) {
  // Assume that we don't modify anything
  bool modified = false;

  // Worklist of instructions to modify
  std::vector<Instruction *> Worklist;
  for (Module::iterator F = M.begin(); F != M.end(); ++F) {
    //
    // Clear the worklist.
    //
    Worklist.clear();

    //
    // Scan through all the instructions in the given function for those that
    // need to be modified.  Add them to the worklist.
    //
    for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
      //
      // Integer comparisons need to be processed.
      //
      if (ICmpInst *CmpI = dyn_cast<ICmpInst>(&*I)) {
        CmpInst::Predicate Pred = CmpI->getUnsignedPredicate();
        if ((Pred >= CmpInst::FIRST_ICMP_PREDICATE) &&
            (Pred <= CmpInst::LAST_ICMP_PREDICATE)) {
          Worklist.push_back (CmpI);
        }
      }

      //
      // Casts from pointers to integers must also be processed.
      //
      if (PtrToIntInst * CastInst = dyn_cast<PtrToIntInst>(&*I)) {
        Worklist.push_back (CastInst);
      }
    }

    //
    // Now scan through the worklist and process each instruction.  Note that,
    // since we're using a worklist, we won't pick up casts introduced by
    // addGetActualValue().
    //
    for (unsigned index = 0; index < Worklist.size(); ++index) {
      //
      // Get the proper element from the worklist.
      //
      Instruction * I = Worklist[index];

      if (ICmpInst *CmpI = dyn_cast<ICmpInst>(I)) {
        //
        // Replace all pointer operands with a call to getActualValue().
        // This will convert an OOB pointer back into the real pointer value.
        //
        if (isa<PointerType>(CmpI->getOperand(0)->getType())) {
          // Rewrite both operands and flag that we modified the code
          addGetActualValue(CmpI, 0);
          modified = true;
        }

        if (isa<PointerType>(CmpI->getOperand(1)->getType())) {
          // Rewrite both operands and flag that we modified the code
          addGetActualValue(CmpI, 1);
          modified = true;
        }
      }

      if (PtrToIntInst * CastInst = dyn_cast<PtrToIntInst>(&*I)) {
        //
        // Replace all pointer operands with a call to getActualValue().
        // This will convert an OOB pointer back into the real pointer value.
        //
        if (isa<PointerType>(CastInst->getOperand(0)->getType())) {
          // Rewrite both operands and flag that we modified the code
          addGetActualValue(CastInst, 0);
          modified = true;
        }
      }
    }
  }

  // Return whether we modified anything
  return modified;
}

//
// Method: addGetActualValue()
//
// Description:
//  Insert a call to the getactualvalue() run-time function to convert the
//  potentially Out of Bound pointer back into its original value.
//
// Inputs:
//  SCI     - The instruction that has arguments requiring conversion.
//  operand - The index of the operand to the instruction that requires
//            conversion.
//
void
RewriteOOB::addGetActualValue (Instruction *SCI, unsigned operand) {
  //
  // Get a reference to the getactualvalue() function.
  //
  Module * M = SCI->getParent()->getParent()->getParent();
  Type * VoidPtrTy = getVoidPtrType (M->getContext());
  Constant * GAVConst = M->getOrInsertFunction ("pchk_getActualValue",
                                                VoidPtrTy,
                                                VoidPtrTy,
                                                VoidPtrTy,
                                                NULL);
  Function * GetActualValue = cast<Function>(GAVConst);

  //
  // Get the operand that needs to be replaced.
  //
  Value * Operand = SCI->getOperand(operand);

  //
  //
  // Rewrite pointers are generated from calls to the SAFECode run-time
  // checks.  Therefore, constants and return values from allocation
  // functions are known to be the original value and do not need to be
  // rewritten back into their orignal values.
  //
  // FIXME:
  //  Add a case for calls to heap allocation functions.
  //
  Value * PeeledOperand = Operand->stripPointerCasts();
  if (isa<Constant>(PeeledOperand) || isa<AllocaInst>(PeeledOperand)) {
    return;
  }

  //
  // Get the pool handle associated with the pointer.
  //
  Value *PH = ConstantPointerNull::get (getVoidPtrType(Operand->getContext()));

  //
  // Create a call to getActualValue() to convert the pointer back to its
  // original value.
  //

  //
  // Update the number of calls to getActualValue() that we inserted.
  //
  ++GetActuals;

  //
  // Insert the call to getActualValue()
  //
  Type * VoidPtrType = getVoidPtrType(Operand->getContext());
  Value * OpVptr = castTo (Operand,
                           VoidPtrType,
                           Operand->getName() + ".casted",
                           SCI);

  std::vector<Value *> args;
  args.push_back (PH);
  args.push_back (OpVptr);
  CallInst *CI = CallInst::Create (GetActualValue,
                                   args,
                                   "getval",
                                   SCI);
  Instruction *CastBack = castTo (CI,
                                  Operand->getType(),
                                  Operand->getName()+".castback",
                                  SCI);
  SCI->setOperand (operand, CastBack);
  return;
}

//
// Method: runOnModule()
//
// Description:
//  Entry point for this LLVM pass.
//
// Return value:
//  true  - The module was modified.
//  false - The module was not modified.
//
bool
RewriteOOB::runOnModule (Module & M) {
  //
  // Insert calls so that comparison instructions convert Out of Bound pointers
  // back into their original values.  This should be done *before* rewriting
  // the program so that pointers are replaced with the return values of bounds
  // checks; this is because the return values of bounds checks have no DSNode
  // in the DSA results, and hence, no associated Pool Handle.
  //
  bool modified = addGetActualValues (M);

  //
  // Transform the code for each type of checking function.  Mark whether
  // we've changed anything.
  //
  for (unsigned index = 0; index < numChecks; ++index) {
    //
    // If this is not a pointer arithmetic check, skip it.
    //
    if (RuntimeChecks[index].checkType == gepcheck) {
      //
      // Transform the function so that the pointer it checks is replaced with
      // its return value.  The return value is the rewritten OOB pointer.
      //
      modified |= processFunction (M, RuntimeChecks[index]);
    }
  }
  return modified;
}

}

