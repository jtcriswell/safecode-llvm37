//===- CFIChecks.h - Insert run-time checks for Control-Flow Integrity -------//
// 
//                     The LLVM Compiler Infrastructure
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file defines a pass that instruments indirect function calls to ensure
// that control-flow integrity is preserved at run-time.
//
//===----------------------------------------------------------------------===//

#ifndef _SAFECODE_CFICHECKS_H_
#define _SAFECODE_CFICHECKS_H_

#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/Pass.h"

namespace llvm {

//
// Pass: CFIChecks
//
// Description:
//  This pass inserts checks on indirect function calls.
//
struct CFIChecks : public ModulePass, InstVisitor<CFIChecks> {
  public:
    static char ID;
    CFIChecks () : ModulePass (ID) { }
    const char *getPassName() const {
      return "Insert Control-Flow Integrity Checks";
    }
    virtual bool runOnModule(Module & M);
    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      // Required passes
      AU.addRequired<CallGraphWrapperPass>();

      // Preserved passes
      AU.setPreservesCFG();
    };

    // Visitor methods
    void visitCallInst  (CallInst  & CI);

  protected:
    // Pointer to load/store run-time check function
    Function * FunctionCheckUI;

    // Create a global variable table for the targets of the call instruction
    GlobalVariable * createTargetTable (CallInst & CI, bool & isComplete);
};

}
#endif
