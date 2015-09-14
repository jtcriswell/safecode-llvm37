//===- InitAllocas.cpp - Initialize allocas with pointers -------//
// 
//                          The SAFECode Compiler 
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file implements a pass that ensures that uninitialized memory created
// by alloca instructions is not used to violate memory safety.  It can do this
// in one of two ways:
//
//   o) Promote the allocations from stack to heap.
//   o) Insert code to initialize the newly allocated memory.
//
// The current implementation implements the latter, but code for the former is
// available but disabled.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "init-allocas"

#include "safecode/InitAllocas.h"
#include "safecode/Utility.h"

#include "llvm/ADT/Statistic.h"
#include "llvm/Pass.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"

#include <vector>

using namespace llvm;

char llvm::InitAllocas::ID = 0;

static RegisterPass<InitAllocas>
Z ("initallocas", "Initialize stack allocations containing pointers");

namespace {
  STATISTIC (InitedAllocas, "Allocas Initialized");
}

//
// Function: getInsertionPoint()
//
// Description:
//  Given an alloca instruction, skip past all subsequent alloca instructions
//  to find an ideal insertion point for instrumenting the alloca.
//
static inline Instruction *
getInsertionPoint (AllocaInst & AI) {
  //
  // Start with the instruction immediently after the alloca.
  //
  BasicBlock::iterator InsertPt = &AI;
  ++InsertPt;

  //
  // Keep skipping over instructions while they are allocas.
  //
  while (isa<AllocaInst>(InsertPt))
    ++InsertPt;
  return InsertPt;
}

namespace llvm {

bool
InitAllocas::doInitialization (Module & M) {
  //
  // Create needed LLVM types.
  //
  Type * VoidType  = Type::getVoidTy(M.getContext());
  Type * Int1Type  = IntegerType::getInt1Ty(M.getContext());
  Type * Int8Type  = IntegerType::getInt8Ty(M.getContext());
  Type * Int32Type = IntegerType::getInt32Ty(M.getContext());
  Type * VoidPtrType = PointerType::getUnqual(Int8Type);

  //
  // Add the memset function to the program.
  //
  M.getOrInsertFunction ("llvm.memset.p0i8.i32",
                         VoidType,
                         VoidPtrType,
                         Int8Type,
                         Int32Type,
                         Int32Type,
                         Int1Type,
                         NULL);

  return true;
}

//
// Method: visitAllocaInst()
//
// Description:
//  This method instruments an alloca instruction so that it is zero'ed out
//  before any data is loaded from it.
//
void
InitAllocas::visitAllocaInst (AllocaInst & AI) {
  //
  // Scan for a place to insert the instruction to initialize the
  // allocated memory.
  //
  Instruction * InsertPt = getInsertionPoint (AI);

  //
  // Zero the alloca with a memset.  If this is done more efficiently with stores
  // SelectionDAG will lower it appropriately based on target information.
  //
  const DataLayout & TD = AI.getModule()->getDataLayout();

  //
  // Get various types that we'll need.
  //
  Type * Int1Type    = IntegerType::getInt1Ty(AI.getContext());
  Type * Int8Type    = IntegerType::getInt8Ty(AI.getContext());
  Type * Int32Type   = IntegerType::getInt32Ty(AI.getContext());
  Type * VoidPtrType = getVoidPtrType (AI.getContext());
  Type * AllocType = AI.getAllocatedType();

  //
  // Create a call to memset.
  //
  Module * M = AI.getParent()->getParent()->getParent();
  Function * Memset = cast<Function>(M->getFunction ("llvm.memset.p0i8.i32"));
  std::vector<Value *> args;
  args.push_back (castTo (&AI, VoidPtrType, AI.getName().str(), InsertPt));
  args.push_back (ConstantInt::get(Int8Type, 0));
  args.push_back (ConstantInt::get(Int32Type,TD.getTypeAllocSize(AllocType)));
  args.push_back (ConstantInt::get(Int32Type,
                                   TD.getABITypeAlignment(AllocType)));
  args.push_back (ConstantInt::get(Int1Type, 0));
  CallInst::Create (Memset, args, "", InsertPt);

  //
  // Update statistics.
  //
  ++InitedAllocas;
  return;
}

bool
InitAllocas::runOnFunction (Function &F) {
  // Don't bother processing external functions
  if (F.isDeclaration())
    return false;

  visit (F);
  return true;
}

}

