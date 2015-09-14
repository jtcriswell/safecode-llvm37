//===- SCUtils.h - Utility Functions for SAFECode ----------------------------//
// 
//                          The SAFECode Compiler 
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file implements several utility functions used by SAFECode.
//
//===----------------------------------------------------------------------===//

#ifndef _SCUTILS_H_
#define _SCUTILS_H_

#include "llvm/BasicBlock.h"
#include "llvm/Constants.h"
#include "llvm/Instructions.h"
#include "llvm/Function.h"
#include "llvm/Module.h"

#include <vector>
#include <set>
#include <string>

using namespace llvm;

namespace llvm {
//
// Function: isCheckingCall()
//
// Description:
//  Determine whether a function is a checking routine inserted by SafeCode.
//
// FIXME: currently the function stays in CodeDuplication.cpp, it
// should be a separate cpp file.
bool isCheckingCall(const std::string & functionName);

//
// Function: getVoidPtrType()
//
// Description:
//  Return a pointer to the LLVM type for a void pointer.
//
// Return value:
//  A pointer to an LLVM type for the void pointer.
//
// Notes:
//  Many, many passes create an LLVM void pointer type, and the code for it
//  takes up most of the 80 columns available in a line.  This function should
//  be easily inlined by the compiler and ease readability of the code (as well
//  as centralize changes when LLVM's Type API is changed).
//
static inline
PointerType * getVoidPtrType(Module & M) {
  const Type * Int8Type  = IntegerType::getInt8Ty (M.getContext());
  return PointerType::getUnqual(Int8Type);
}

static inline
PointerType * getVoidPtrType(LLVMContext & Context) {
  const Type * Int8Type  = IntegerType::getInt8Ty (Context);
  return PointerType::getUnqual(Int8Type);
}

//
// Function: castTo()
//
// Description:
//  Given an LLVM value, insert a cast instruction to make it a given type.
//
static inline Value *
castTo (Value * V, const Type * Ty, Twine Name, Instruction * InsertPt) {
  //
  // Don't bother creating a cast if it's already the correct type.
  //
  assert (V && "castTo: trying to cast a NULL Value!\n");
  if (V->getType() == Ty)
    return V;
                                                                                
  //
  // If it's a constant, just create a constant expression.
  //
  if (Constant * C = dyn_cast<Constant>(V)) {
    Constant * CE = ConstantExpr::getZExtOrBitCast (C, Ty);
    return CE;
  }
                                                                                
  //
  // Otherwise, insert a cast instruction.
  //
  return CastInst::CreateZExtOrBitCast (V, Ty, Name, InsertPt);
}

static inline Instruction *
castTo (Instruction * I, const Type * Ty, Twine Name, Instruction * InsertPt) {
  //
  // Don't bother creating a cast if it's already the correct type.
  //
  assert (I && "castTo: trying to cast a NULL Instruction!\n");
  if (I->getType() == Ty)
    return I;
                                                                                
  //
  // Otherwise, insert a cast instruction.
  //
  return CastInst::CreateZExtOrBitCast (I, Ty, Name, InsertPt);
}

static inline Value *
castTo (Value * V, const Type * Ty, Instruction * InsertPt) {
  return castTo (V, Ty, "casted", InsertPt);
}

//
// Function: indexesStructsOnly()
//
// Description:
//  Determines whether the given GEP expression only indexes into structures.
//
// Return value:
//  true - This GEP only indexes into structures.
//  false - This GEP indexes into one or more arrays.
//
static inline bool
indexesStructsOnly (GetElementPtrInst * GEP) {
  const Type * PType = GEP->getPointerOperand()->getType();
  const Type * ElementType;
  unsigned int index = 1;
  std::vector<Value *> Indices;
  unsigned int maxOperands = GEP->getNumOperands() - 1;

  //
  // Check the first index of the GEP.  If it is non-zero, then it doesn't
  // matter what type we're indexing into; we're indexing into an array.
  //
  if (ConstantInt * CI = dyn_cast<ConstantInt>(GEP->getOperand(1)))
    if (!(CI->isNullValue ()))
      return false;

  //
  // Scan through all types except for the last.  If any of them are an array
  // type, the GEP is indexing into an array.
  //
  // If the last type is an array, the GEP returns a pointer to an array.  That
  // means the GEP itself is not indexing into the array; this is why we don't
  // check the type of the last GEP operand.
  //
  for (index = 1; index < maxOperands; ++index) {
    Indices.push_back (GEP->getOperand(index));
    ElementType = GetElementPtrInst::getIndexedType (PType,
                                                     Indices.begin(),
                                                     Indices.end());
    assert (ElementType && "ElementType is NULL!");
    if (isa<ArrayType>(ElementType)) {
      return false;
    }
  }

  return true;
}

//
// Function: peelCasts()
//
// Description:
//  This method peels off casts to get to the original instruction that
//  generated the value for the given instruction.
//
// Inputs:
//  PointerOperand - The value off of which we will peel the casts.
//
// Outputs:
//  Chain - The set of values that are between the original value and the
//          specified value.
//
// Return value:
//  A pointer to the LLVM value that originates the specified LLVM value.
//
static inline Value *
peelCasts (Value * PointerOperand, std::set<Value *> & Chain) {
  Value * SourcePointer = PointerOperand;
  bool done = false;

  while (!done) {
    //
    // Trace through constant cast and GEP expressions
    //
    if (ConstantExpr * cExpr = dyn_cast<ConstantExpr>(SourcePointer)) {
      if (cExpr->isCast()) {
        if (isa<PointerType>(cExpr->getOperand(0)->getType())) {
          Chain.insert (SourcePointer);
          SourcePointer = cExpr->getOperand(0);
          continue;
        }
      }

      // We cannot handle this expression; break out of the loop
      break;
    }

    //
    // Trace back through cast instructions.
    //
    if (CastInst * CastI = dyn_cast<CastInst>(SourcePointer)) {
      if (isa<PointerType>(CastI->getOperand(0)->getType())) {
        Chain.insert (SourcePointer);
        SourcePointer = CastI->getOperand(0);
        continue;
      }
      break;
    }

    // We can't scan through any more instructions; give up
    done = true;
  }

  return SourcePointer;
}

//
// Function: destroyFunction()
//
// Description:
//  This function removes all of the existing instructions from an LLVM
//  function and changes it to be a function declaration (i.e., no body).
//
// Inputs:
//  F - A pointer to the LLVM function to destroy.
//
// Outputs:
//  F - The LLVM function is modified to have no instructions.
//
static inline void
destroyFunction (Function * F) {
  //
  // Null functions have nothing to destroy.
  //
  if (!F) return;

  // Worklist of instructions that should be removed.
  std::vector<Instruction *> toRemove;

  //
  // Schedule all of the instructions in the function for deletion.  We use a
  // worklist to avoid any potential iterator invalidation.
  //
  for (Function::iterator BB = F->begin(); BB != F->end(); ++BB) {
    for (BasicBlock::iterator I = BB->begin(); I != BB->end(); ++I) {
        toRemove.push_back (&*I);
    }
  }

  //
  // Remove all of the remaining instructions from each basic block first.
  //
  for (unsigned index = 0; index < toRemove.size(); ++index) {
    Instruction * I = toRemove[index];

    //
    // Change all the operands so that the instruction is not using anything.
    //
    for (unsigned i = 0; i < I->getNumOperands(); ++i) {
      I->setOperand (i, UndefValue::get (I->getOperand(i)->getType()));
    }

    //
    // Remove the instruction from its basic block.
    //
    I->removeFromParent();
  }

  //
  // We can now deallocate all of the old instructions.
  //
  while (toRemove.size()) {
    Instruction * I = toRemove.back();
    toRemove.pop_back();
    delete I;
  }

  //
  // Remove all dead basic blocks.  Again, we use a worklist to avoid any
  // potential iterator invalidation.
  //
  std::vector<BasicBlock *> toRemoveBB;
  for (Function::iterator BB = F->begin(); BB != F->end(); ++BB) {
    toRemoveBB.push_back (&*BB);
  }

  while (toRemoveBB.size()) {
    BasicBlock * BB = toRemoveBB.back();
    toRemoveBB.pop_back();
    BB->eraseFromParent();
  }

  return;
}

//
// Function: escapesToMemory()
//
// Description:
//  Do some simple analysis to see if the value could escape into memory.
//
// Return value:
//  true  - The value could (but won't necessairly) escape into memory.
//  false - The value cannot escape into memory.
//
static inline bool
escapesToMemory (Value * V) {
  // Worklist of values to process
  std::vector<Value *> Worklist;
  Worklist.push_back (V);

  //
  // Scan through all uses of the value and see if any of them can escape into
  // another function or into memory.
  //
  while (Worklist.size()) {
    //
    // Get the next value off of the worklist.
    //
    Value * V = Worklist.back();
    Worklist.pop_back();

    //
    // Scan through all uses of the value.
    //
    for (Value::use_iterator UI = V->use_begin(); UI != V->use_end(); ++UI) {
      //
      // We cannot handle PHI nodes because they might introduce a recurrence
      // in the def-use chain, and we're not handling such cycles at the
      // moment.
      //
      if (isa<PHINode>(UI)) {
        return true;
      }

      //
      // The pointer escapes if it's stored to memory somewhere.
      //
      StoreInst * SI;
      if ((SI = dyn_cast<StoreInst>(UI)) && (SI->getOperand(0) == V)) {
        return true;
      }

      //
      // For a select instruction, assume that the pointer escapes.  The reason
      // is that the exactcheck() optimization can't trace back through a
      // select.
      //
      if (isa<SelectInst>(UI)) {
        return true;
      }

      //
      // GEP instructions are okay but need to be added to the worklist.
      //
      if (isa<GetElementPtrInst>(UI)) {
        Worklist.push_back (*UI);
        continue;
      }

      //
      // Cast instructions are okay even if they lose bits.  Some of the bits
      // will end up in the result.
      //
      if (isa<CastInst>(UI)) {
        Worklist.push_back (*UI);
        continue;
      }

      //
      // Cast constant expressions are okay, too.
      //
      if (ConstantExpr *cExpr = dyn_cast<ConstantExpr>(UI)) {
        if (Instruction::isCast (cExpr->getOpcode())) {
          Worklist.push_back (*UI);
          continue;
        } else {
          return true;
        }
      }

      //
      // Load instructions are okay.
      //
      if (isa<LoadInst>(UI)) {
        continue;
      }

      //
      // Call instructions are okay if we understand the semantics of the
      // called function.  Otherwise, assume they call a function that allows
      // the pointer to escape into memory.
      //
      if (CallInst * CI = dyn_cast<CallInst>(UI)) {
        if (!(CI->getCalledFunction())) {
          return true;
        }

        std::string FuncName = CI->getCalledFunction()->getName();
        if ((FuncName == "sc.exactcheck2") ||
            (FuncName == "sc.boundscheck") ||
            (FuncName == "sc.boundscheckui")) {
          Worklist.push_back (*UI);
          continue;
        } else if ((FuncName == "llvm.memcpy.i32")    || 
                   (FuncName == "llvm.memcpy.i64")    ||
                   (FuncName == "llvm.memset.i32")    ||
                   (FuncName == "llvm.memset.i64")    ||
                   (FuncName == "llvm.memmove.i32")   ||
                   (FuncName == "llvm.memmove.i64")   ||
                   (FuncName == "llva_memcpy")        ||
                   (FuncName == "llva_memset")        ||
                   (FuncName == "llva_strncpy")       ||
                   (FuncName == "llva_invokememcpy")  ||
                   (FuncName == "llva_invokestrncpy") ||
                   (FuncName == "llva_invokememset")  ||
                   (FuncName == "sc.pool_register")  ||
                   (FuncName == "sc.pool_register_stack")  ||
                   (FuncName == "sc.pool_register_global")  ||
                   (FuncName == "memcmp")) {
          continue;
        } else {
          return true;
        }
      }

      //
      // We don't know what this is.  Just assume it can escape to memory.
      //
      return true;
    }
  }

  //
  // No use causes the value to escape to memory.
  //
  return false;
}

}

#endif

