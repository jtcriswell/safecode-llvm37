//===- SpecializeCMSCalls.cpp - Specialize common memory safety calls -----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass converts the common memory safety checks and registration calls
// into SAFECode-specific calls.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "specialize-cms-calls"

#include "safecode/SpecializeCMSCalls.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

using namespace llvm;

STATISTIC(MemoryChecksConverted, "Load/store checks converted");

namespace {
  class SpecializeCMSCalls : public ModulePass {
    Type *VoidTy;
    PointerType *VoidPtrTy;
    IntegerType *Int32Ty;
    Constant *VoidNullPtr;

    void specialize(Module &M, StringRef Before, StringRef After,
                    ArrayRef <int> NewOrder, Statistic &Stats);

    void specializeLoadStoreChecks(Module &M);

  public:
    static char ID;
    SpecializeCMSCalls(): ModulePass(ID) { }
    virtual bool runOnModule(Module &M);

    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.setPreservesCFG();
    }

    virtual const char *getPassName() const {
      return "SpecializeCMSCalls";
    }
  };
} // end anon namespace

char SpecializeCMSCalls::ID = 0;

INITIALIZE_PASS(SpecializeCMSCalls, "specialize-cms-calls",
                "Specialize common memory safety checks", false, false)

ModulePass *llvm::createSpecializeCMSCallsPass() {
  return new SpecializeCMSCalls();
}

bool SpecializeCMSCalls::runOnModule(Module &M) {
  VoidTy = Type::getVoidTy(M.getContext());
  VoidPtrTy = Type::getInt8PtrTy(M.getContext());
  Int32Ty = IntegerType::getInt32Ty(M.getContext());
  VoidNullPtr = ConstantPointerNull::get(VoidPtrTy);

  specializeLoadStoreChecks(M);
  return true; // assume that something was changed
}

void SpecializeCMSCalls::specializeLoadStoreChecks(Module &M) {
  M.getOrInsertFunction("poolcheckui", VoidTy, VoidPtrTy, VoidPtrTy, Int32Ty,
                        NULL);
  Function *PoolCheckUI = M.getFunction("poolcheckui");
  PoolCheckUI->addFnAttr(Attribute::ReadOnly);

  const int NewOrder[] = {1, 2};
  specialize(M, "__loadcheck", "poolcheckui", NewOrder, MemoryChecksConverted);
  specialize(M, "__storecheck", "poolcheckui", NewOrder, MemoryChecksConverted);
}

void SpecializeCMSCalls::specialize(Module &M, StringRef Before,
                                    StringRef After, ArrayRef <int> NewOrder,
                                    Statistic &Stats) {
  Function *From = M.getFunction(Before);
  if (!From)
    return; // no uses of the initial function

  Function *To = M.getFunction(After);
  assert(To && "target function missing");

  // Get a list of the new function's arguments
  SmallVector <Argument*, 4> ToArgs;
  for (Function::arg_iterator A = To->arg_begin(), E = To->arg_end();
       A != E;
       ++A) {
    ToArgs.push_back(A);
  }

  // Make all calls of the old function use the new function instead.
  SmallVector <CallInst*, 64> ToRemove;
  for (Value::use_iterator UI = From->use_begin(), E = From->use_end();
        UI != E;
        ++UI) {
    // Only call instructions are supposed to exist.
    CallInst *CI = cast<CallInst>(*UI);

    IRBuilder<> Builder(CI);
    SmallVector <Value*, 4> Args(To->arg_size());

    // Set the arguments according to the given order.
    for (size_t i = 0, N = NewOrder.size(); i < N; ++i) {
      assert(Args[NewOrder[i]] == NULL);
      Value *Arg = CI->getArgOperand(i);
      const Argument *ToArg = ToArgs[NewOrder[i]];

      if (Arg->getType()->isIntegerTy()) {
        // This is a size argument that may need to be casted.
        assert(ToArg->getType()->isIntegerTy());
        Args[NewOrder[i]] = Builder.CreateIntCast(Arg, ToArg->getType(),
                                                  /*isSigned=*/false);
      } else {
        // Anything else must have the exact right type.
        assert(Arg->getType() == ToArg->getType());
        Args[NewOrder[i]] = Arg;
      }
    }

    // Fill missing arguments such as pool handles with null
    for (size_t i = 0, N = To->arg_size(); i < N; ++i) {
      if (Args[i])
        continue; // already filled

      const Argument *ToArg = ToArgs[NewOrder[i]];
      assert(ToArg->getType() == VoidPtrTy && "expected void pointer");
      Args[i] = VoidNullPtr;
    }

    CallInst *NewCall = Builder.CreateCall(To, Args);

    // Copy debug information if it is present.
    if (MDNode *MD = CI->getMetadata("dbg"))
      NewCall->setMetadata("dbg", MD);

    // Replace all uses if it has a non-void return value.
    if (From->getReturnType() != VoidTy) {
      assert(To->getReturnType() != VoidTy);
      CI->replaceAllUsesWith(NewCall);
    }

    ToRemove.push_back(CI);
    ++Stats;
  }

  // Remove the old calls
  for (size_t i = 0, N = ToRemove.size(); i < N; ++i)
    ToRemove[i]->eraseFromParent();
}
