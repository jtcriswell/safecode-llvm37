//===- OptimizeIdenticalLSChecks.cpp - Remove identical load/store checks -===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass removes identical load/store checks by removing all but the
// first instances of repeating (base ptr, access size) pairs in segments of
// basic blocks where the segments are ended by function calls that may
// deallocate memory.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "optimize-identical-ls-checks"

#include "CommonMemorySafetyPasses.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/MSCInfo.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Instrumentation.h"

using namespace llvm;

STATISTIC(MemoryChecksRemoved, "Load/store checks removed");

namespace {
  class OptimizeIdenticalLSChecks : public FunctionPass {
    bool mayDeallocateMemory(CallInst *CI);

  public:
    static char ID;
    OptimizeIdenticalLSChecks(): FunctionPass(ID) { }
    virtual bool runOnFunction(Function &F);

    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.addRequired<MSCInfo>();
      AU.setPreservesCFG();
    }

    virtual const char *getPassName() const {
      return "OptimizeIdenticalLSChecks";
    }
  };

  typedef std::pair <Value*, Value*> ValuePair;
} // end anon namespace

char OptimizeIdenticalLSChecks::ID = 0;

INITIALIZE_PASS(OptimizeIdenticalLSChecks, "optimize-identical-ls-checks",
                "Remove identical load/store checks where possible", false,
                false)

FunctionPass *llvm::createOptimizeIdenticalLSChecksPass() {
  return new OptimizeIdenticalLSChecks();
}

bool OptimizeIdenticalLSChecks::runOnFunction(Function &F) {
  MSCInfo *MSCI = &getAnalysis<MSCInfo>();
  SmallSet <ValuePair, 32> PreviousChecks;
  SmallVector <CallInst*, 64> ToRemove;

  for (Function::iterator BB = F.begin(), BBE = F.end(); BB != BBE; ++BB) {
    for (BasicBlock::iterator I = BB->begin(), IE = BB->end(); I != IE; ++I) {
      // Clear cache on atomics intrinsics to be able to catch some concurrency
      // bugs where one thread frees the object between two accesses by another
      // thread.
      if (isa<AtomicCmpXchgInst>(I) || isa<AtomicRMWInst>(I)) {
        PreviousChecks.clear();
        continue;
      }

      // InvokeInst can be ignored because it is a terminator and all checks
      // should be call instructions.
      CallInst *CI = dyn_cast<CallInst>(I);
      if (!CI)
        continue;

      CheckInfoType *Info = MSCI->getCheckInfo(CI->getCalledFunction());
      if (Info && Info->isMemoryCheck()) {
        ValuePair Pair(CI->getArgOperand(Info->PtrArgNo)->stripPointerCasts(),
                       CI->getArgOperand(Info->SizeArgNo));
        if (PreviousChecks.count(Pair))
          ToRemove.push_back(CI);
        else
          PreviousChecks.insert(Pair);
      } else if (mayDeallocateMemory(CI)) {
        PreviousChecks.clear();
      }
    }

    PreviousChecks.clear();
  }

  for (size_t i = 0, N = ToRemove.size(); i != N; ++i) {
    ToRemove[i]->eraseFromParent();
    ++MemoryChecksRemoved;
  }

  return !ToRemove.empty();
}

bool OptimizeIdenticalLSChecks::mayDeallocateMemory(CallInst *CI) {
  // llvm.mem[set|cpy|move].*
  if (isa<MemIntrinsic>(CI))
    return false;

  return true;
}
