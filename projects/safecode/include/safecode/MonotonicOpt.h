//===- MonotonicOpt.h - Optimize SAFECode checks in loops --------------------//
// 
//                          The SAFECode Compiler 
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file defines a pass that hoists SAFECode run-time checks out of loops.
//
//===----------------------------------------------------------------------===//

#ifndef _SAFECODE_MONOTONICOPT_H_
#define _SAFECODE_MONOTONICOPT_H_

#include "llvm/Pass.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Instructions.h"

using namespace llvm;

namespace sc {

struct MonotonicLoopOpt : public LoopPass {
  public:
    static char ID;
    virtual const char *getPassName() const {
      return "Optimize SAFECode checkings in monotonic loops";
    }
    MonotonicLoopOpt() : LoopPass(ID) {}
    virtual bool doInitialization(Loop *L, LPPassManager &LPM); 
    virtual bool doFinalization(); 
    virtual bool runOnLoop(Loop *L, LPPassManager &LPM);
    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.addRequired<DataLayout>();
      AU.addRequired<LoopInfo>();
      AU.addRequired<ScalarEvolution>();
      AU.setPreservesCFG();
    }
  private:
    // Pointers to required analysis passes
    LoopInfo * LI;
    ScalarEvolution * scevPass;
    DataLayout * TD;

    // Set of loops already optimized
    std::set<Loop*> optimizedLoops;

    bool isMonotonicLoop(Loop * L, Value * loopVar);
    bool isHoistableGEP(GetElementPtrInst * GEP, Loop * L);
    void insertEdgeBoundsCheck(int checkFunctionId, Loop * L, const CallInst * callInst, GetElementPtrInst * origGEP, Instruction *
    ptIns, int type);
    bool optimizeCheck(Loop *L);
    bool isEligibleForOptimization(const Loop * L);
};

}
#endif
