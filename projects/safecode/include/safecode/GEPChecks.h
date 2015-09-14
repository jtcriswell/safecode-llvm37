//===- GEPChecks.h - Insert run-time checks for GEPs -------------------------//
// 
//                     The LLVM Compiler Infrastructure
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file implements several passes that insert run-time checks to enforce
// SAFECode's memory safety guarantees as well as several other passes that
// help to optimize the instrumentation.
//
//===----------------------------------------------------------------------===//

#ifndef _SAFECODE_GEP_CHECKS_H_
#define _SAFECODE_GEP_CHECKS_H_

#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/Pass.h"

#include "safecode/ArrayBoundsCheck.h"

namespace llvm {

//
// Pass: InsertGEPChecks
//
// Description:
//  This pass inserts checks on GEP instructions.
//
struct InsertGEPChecks : public FunctionPass, InstVisitor<InsertGEPChecks> {
  public:
    static char ID;
    InsertGEPChecks () : FunctionPass (ID) { }
    const char *getPassName() const { return "Insert GEP Checks"; }
    virtual bool  doInitialization (Module & M);
    virtual bool runOnFunction(Function &F);
    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      // Required passes
      AU.addRequired<ArrayBoundsCheckLocal>();

      // Preserved passes
      AU.setPreservesCFG();
    };

    // Visitor methods
    void visitGetElementPtrInst (GetElementPtrInst & GEP);

  protected:
    // Pointers to required passes
    const DataLayout * TD;
    ArrayBoundsCheckLocal * abcPass;

    // Pointer to GEP run-time check function
    Function * PoolCheckArrayUI;
};

}
#endif
