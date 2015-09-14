//===- InvalidFreeChecks.h - Insert invalid free checks ----------------------//
// 
//                     The LLVM Compiler Infrastructure
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file defines a pass that inserts run-time checks that ensure that
// free() only gets valid pointers.
//
//===----------------------------------------------------------------------===//

#ifndef _SAFECODE_INVALIDFREECHECKS_H_
#define _SAFECODE_INVALIDFREECHECKS_H_

#include "llvm/IR/CallSite.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/Pass.h"

namespace llvm {

//
// Pass: InsertFreeChecks
//
// Description:
//  This pass inserts checks on calls to free().
//
struct InsertFreeChecks : public FunctionPass, InstVisitor<InsertFreeChecks> {
  public:
    static char ID;
    InsertFreeChecks () : FunctionPass (ID) { }
    const char *getPassName() const { return "Insert Invalid Free Checks"; }
    virtual bool  doInitialization (Module & M);
    virtual bool runOnFunction(Function & F);
    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      // Preserve the CFG
      AU.setPreservesCFG();
    };

    // Visitor methods
    void visitCallSite (CallSite & CS);
    void visitCallInst  (CallInst  & CI) {
      CallSite CS(&CI);
      visitCallSite (CS);
    }
    void visitInvokeInst (InvokeInst & II) {
      CallSite CS(&II);
      visitCallSite (CS);
    }
};

}
#endif
