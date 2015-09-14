//===- OptimizeImpliedFastLSChecks.cpp - Remove implied fast l/s checks ---===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass removes fast load/store checks that are implied by other
// fast load/store checks. It works by traversing a dominator tree to find
// out which checks must always happen before other checks.
//
// In particular it can remove fast load/store checks where an identical one
// already dominates it. It can also remove cases where the only difference
// between the checks is the object being referred to (i.e. when the objects are
// the same size and the access offsets and sizes are equal).
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "optimize-implied-fast-ls-checks"

#include "CommonMemorySafetyPasses.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/MSCInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/Instrumentation.h"
#include "llvm/Pass.h"

using namespace llvm;

STATISTIC(FastMemoryChecksRemoved, "Fast load/store checks removed");

namespace {
  struct AccessData {
    Value *AccessSize, *ObjSize;
    const SCEV *Offset;

    AccessData(Value *AccessSize, Value *ObjSize, const SCEV *Offset):
        AccessSize(AccessSize), ObjSize(ObjSize), Offset(Offset) { }

    bool operator<(const AccessData &Other) const {
      if (AccessSize != Other.AccessSize)
        return AccessSize < Other.AccessSize;
      if (ObjSize != Other.ObjSize)
        return ObjSize < Other.ObjSize;
      return Offset < Other.Offset;
    }

    bool operator==(const AccessData &Other) const {
      return AccessSize == Other.AccessSize && ObjSize == Other.ObjSize &&
             Offset == Other.Offset;
    }
  };

  class OptimizeImpliedFastLSChecks : public FunctionPass {
    MSCInfo *MSCI;
    ScalarEvolution *SE;

    // The access data objects of all fast load/store checks that dominate the
    // basic block that is being worked on in exploreNode.
    SmallSet <AccessData, 16> PreviousChecks;

    // The checks scheduled for removal.
    SmallVector <CallInst*, 16> ToRemove;

    void exploreNode(DomTreeNode* Node);

  public:
    static char ID;
    OptimizeImpliedFastLSChecks(): FunctionPass(ID) { }

    virtual bool runOnFunction(Function &F);

    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.addRequired<DominatorTreeWrapperPass>();
      AU.addPreserved<DominatorTreeWrapperPass>();
      AU.addRequired<MSCInfo>();
      AU.addRequired<ScalarEvolution>();
      AU.setPreservesCFG();
    }

    virtual const char *getPassName() const {
      return "OptimizeImpliedFastLSChecks";
    }
  };
} // end anon namespace

char OptimizeImpliedFastLSChecks::ID = 0;

INITIALIZE_PASS(OptimizeImpliedFastLSChecks, "optimize-implied-fast-ls-checks",
                "Remove implied fast load/store checks where possible.", false,
                false)

FunctionPass *llvm::createOptimizeImpliedFastLSChecksPass() {
  return new OptimizeImpliedFastLSChecks();
}

bool OptimizeImpliedFastLSChecks::runOnFunction(Function &F) {
  DominatorTree *DT = &getAnalysis<DominatorTreeWrapperPass>().getDomTree();
  MSCI = &getAnalysis<MSCInfo>();
  SE = &getAnalysis<ScalarEvolution>();

  // Go through the function in dominance order to find the checks to remove.
  exploreNode(DT->getRootNode());
  assert(PreviousChecks.empty() && "This should be empty");

  // Erase the checks scheduled for removal.
  for (size_t i = 0, N = ToRemove.size(); i != N; ++i) {
    ToRemove[i]->eraseFromParent();
    ++FastMemoryChecksRemoved;
  }

  // Return true iff anything was changed (any checks were removed).
  bool modified = !ToRemove.empty();
  ToRemove.clear();
  return modified;
}

/// exploreNode - recursively explore the basic blocks that are dominated by
/// the current basic block (referred to by the dominator tree node).
///
/// Side effects:
/// * Previously unseen checks will be added to PreviousChecks before the
///   recursive calls. The initial state will be restored before returning.
/// * Checks scheduled for removal will be added to ToRemove.
///
void OptimizeImpliedFastLSChecks::exploreNode(DomTreeNode* Node) {
  // The list of previously seen checks in this basic block.
  SmallVector <AccessData, 4> LocalChecks;

  // Iterate over all fast load/store checks in this basic block.
  // Remove the ones that are implied by dominating checks.
  // Add the rest to the set of previous checks.
  BasicBlock *BB = Node->getBlock();
  for (BasicBlock::iterator I = BB->begin(), E = BB->end(); I != E; ++I) {
    CallInst *CI = dyn_cast<CallInst>(I);
    if (!CI)
      continue;

    CheckInfoType *Info = MSCI->getCheckInfo(CI->getCalledFunction());
    if (!Info || !Info->isFastMemoryCheck())
      continue;

    Value *AccessPtr = CI->getArgOperand(Info->PtrArgNo);
    Value *AccessSize = CI->getArgOperand(Info->SizeArgNo);
    Value *ObjPtr = CI->getArgOperand(Info->ObjArgNo);
    Value *ObjSize = CI->getArgOperand(Info->ObjSizeArgNo);

    // Create an analysis expression of the offset from the object.
    // It will hopefully get rid of the reference to the object itself.
    const SCEV *Offset = SE->getMinusSCEV(SE->getSCEV(AccessPtr),
                                          SE->getSCEV(ObjPtr));

    AccessData Access(AccessSize, ObjSize, Offset);
    if (PreviousChecks.count(Access)) {
      // An equivalent check has been seen before so this one can be removed.
      ToRemove.push_back(CI);
    } else {
      // Previously unseen kind of check - record it for future reference.
      PreviousChecks.insert(Access);
      LocalChecks.push_back(Access);
    }
  }

  // Recursively call this function on basic blocks that are directly dominated.
  const std::vector <DomTreeNode*> &Children = Node->getChildren();
  for (size_t i = 0, N = Children.size(); i != N; ++i)
    exploreNode(Children[i]);

  // Restore PreviousChecks to the state at the beginning of the call.
  for (size_t i = 0, N = LocalChecks.size(); i != N; ++i)
    PreviousChecks.erase(LocalChecks[i]);
}
