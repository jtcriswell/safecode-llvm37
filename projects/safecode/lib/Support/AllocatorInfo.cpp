//===- AllocatorInfo.cpp ----------------------------------------*- C++ -*----//
// 
//                     The LLVM Compiler Infrastructure
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// Define the abstraction of a pair of allocator / deallocator, including:
//
//   * The size of the object being allocated. 
//   * Whether the size may be a constant, which can be used for exactcheck
// optimization.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/CallSite.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "safecode/AllocatorInfo.h"

using namespace llvm;

namespace llvm {

AllocatorInfo::~AllocatorInfo() {}

char AllocatorInfoPass::ID = 0;

static RegisterPass<AllocatorInfoPass>
X ("allocinfo", "Allocator Information Pass");

Value *
SimpleAllocatorInfo::getAllocSize(Value * AllocSite) const {
  CallInst * CI = dyn_cast<CallInst>(AllocSite);
  if (!CI)
    return NULL;

  Function * F  = dyn_cast<Function>(CI->getCalledValue()->stripPointerCasts());
  if (!F || F->getName() != allocCallName) 
    return NULL;

  CallSite CS(CI);
  return CS.getArgument(allocSizeOperand - 1);
}

Value *
SimpleAllocatorInfo::getOrCreateAllocSize(Value * AllocSite) const {
  return getAllocSize (AllocSite);
}

Value *
ArrayAllocatorInfo::getOrCreateAllocSize(Value * AllocSite) const {
  //
  // See if this is a call to the allocator.  If not, return NULL.
  //
  CallInst * CI = dyn_cast<CallInst>(AllocSite);
  if (!CI)
    return NULL;

  Function * F  = dyn_cast<Function>(CI->getCalledValue()->stripPointerCasts());
  if (!F || F->getName() != allocCallName) 
    return NULL;

  //
  // Insert a multiplication instruction to compute the size of the array
  // allocation.
  //
  CallSite CS(CI);
  return BinaryOperator::Create (BinaryOperator::Mul,
                                 CS.getArgument(allocSizeOperand - 1),
                                 CS.getArgument(allocNumOperand - 1),
                                 "size",
                                 CI);
}

Value *
StringAllocatorInfo::getOrCreateAllocSize (Value * AllocSite) const {
  //
  // See if this is a call to the allocator.  If not, return NULL.
  //
  CallInst * CI = dyn_cast<CallInst>(AllocSite);
  if (!CI)
    return NULL;

  Function * F  = dyn_cast<Function>(CI->getCalledValue()->stripPointerCasts());
  if (!F || F->getName() != allocCallName) 
    return NULL;

  //
  // See if this call has an argument.  If not, ignore it.  We do this because
  // autoconf configure scripts will create calls to string functions with zero
  // arguments just to see if the function exists.
  //
  CallSite CS(CI);
  if (CS.arg_size() == 0)
    return NULL;

  //
  // Insert a call to strlen() to determine the length of the string that was
  // allocated.  Use a version of strlen() in the SAFECode library that can
  // handle NULL pointers.
  //
  Module * M = CI->getParent()->getParent()->getParent();
  Function * Strlen = M->getFunction ("nullstrlen");
  assert (Strlen && "No nullstrlen function in the module");
  BasicBlock::iterator InsertPt = CI;
  ++InsertPt;
  Instruction * Length =
    CallInst::Create (Strlen, CS.getInstruction(), "", InsertPt);

  //
  // The size of the allocation is the string length plus one.
  //
  IntegerType * LengthType = dyn_cast<IntegerType>(Length->getType());
  assert (LengthType && "nullstrlen doesn't return an integer?");
  Value * One = ConstantInt::get (LengthType, 1);
  Instruction * Size = BinaryOperator::Create (Instruction::Add, Length, One);
  Size->insertAfter (Length);
  return Size;
}

Value *
SimpleAllocatorInfo::getFreedPointer(Value * FreeSite) const {
  CallInst * CI = dyn_cast<CallInst>(FreeSite);

  Function * F  = dyn_cast<Function>(CI->getCalledValue()->stripPointerCasts());
  if (!F || F->getName() != freeCallName) 
    return NULL;

  CallSite CS(CI);
  return CS.getArgument(freePtrOperand-1);
}

Value *
ReAllocatorInfo::getAllocedPointer (Value * AllocSite) const {
  CallInst * CI = dyn_cast<CallInst>(AllocSite);

  Function * F  = dyn_cast<Function>(CI->getCalledValue()->stripPointerCasts());
  if (!F || F->getName() != allocCallName) 
    return NULL;

  CallSite CS(CI);
  return CS.getArgument(allocPtrOperand-1);
}

//
// Method: getObjectSize()
//
// Description:
//  Try to get an LLVM value that represents the size of the memory object
//  referenced by the specified pointer.
//
Value *
AllocatorInfoPass::getObjectSize(Value * V) {
  // Get access to the target data information
  DataLayout dummy("");
  DataLayout & TD = dummy; // dummy initialization
  if (isa<Instruction>(V)) {
    TD = dyn_cast<Instruction>(V)->getModule()->getDataLayout();
  }
  else if (isa<GlobalValue>(V)) {
    TD = dyn_cast<GlobalValue>(V)->getParent()->getDataLayout();
  }
  else if (isa<Argument>(V)) {
    TD = dyn_cast<Argument>(V)->getParent()->getParent()->getDataLayout();
  }

  //
  // Finding the size of a global variable is easy.
  //
  Type * Int32Type = IntegerType::getInt32Ty(V->getContext());
  if (GlobalVariable * GV = dyn_cast<GlobalVariable>(V)) {
    Type * allocType = GV->getType()->getElementType();
    return ConstantInt::get (Int32Type, TD.getTypeAllocSize (allocType));
  }

  //
  // Find the size of byval function arguments is also easy.
  //
  if (Argument * AI = dyn_cast<Argument>(V)) {
    if (AI->hasByValAttr()) {
      assert (isa<PointerType>(AI->getType()));
      PointerType * PT = cast<PointerType>(AI->getType());
      unsigned int type_size = TD.getTypeAllocSize (PT->getElementType());
      return ConstantInt::get (Int32Type, type_size);
    }
  }

  //
  // Alloca instructions are a little harder but not bad.
  //
  if (AllocaInst * AI = dyn_cast<AllocaInst>(V)) {
    unsigned int type_size = TD.getTypeAllocSize (AI->getAllocatedType());
    if (AI->isArrayAllocation()) {
      if (ConstantInt * CI = dyn_cast<ConstantInt>(AI->getArraySize())) {
        if (CI->getSExtValue() > 0) {
          type_size *= CI->getSExtValue();
        } else {
          return NULL;
        }
      } else {
        return NULL;
      }
    }
    return ConstantInt::get(Int32Type, type_size);
  }

  //
  // Heap (i.e., customized) allocators are the most difficult, but we can
  // manage.
  //
  if (CallInst * CI = dyn_cast<CallInst>(V)) {
    Function * F = CI->getCalledFunction();
    if (!F)
      return NULL;

    const std::string & name = F->getName();
    for (alloc_iterator it = alloc_begin(); it != alloc_end(); ++it) {
      if ((*it)->isAllocSizeMayConstant(CI) && (*it)->getAllocCallName() == name) {
        return (*it)->getAllocSize(CI);
      }
    }
  }

  return NULL;
}

}

