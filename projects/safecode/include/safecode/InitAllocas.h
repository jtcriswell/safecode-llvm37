//===- convert.h - Promote unsafe alloca instructions to heap allocations ----//
// 
//                          The SAFECode Compiler 
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file implements a pass that promotes unsafe stack allocations to heap
// allocations.  It also updates the pointer analysis results accordingly.
//
// This pass relies upon the abcpre, abc, and checkstack safety passes.
//
//===----------------------------------------------------------------------===//

#ifndef SAFECODE_INITALLOCAS_H
#define SAFECODE_INITALLOCAS_H

#include "llvm/IR/DataLayout.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/Pass.h"

namespace llvm {

//
// Pass: InitAllocas
//
// Description:
//  This pass ensures that uninitialized pointers within stack allocated
//  (i.e., alloca'ed) memory cannot be dereferenced to cause a memory error.
//  This can be done either by promoting the stack allocation to a heap
//  allocation (since the heap allocator must provide similar protection for
//  heap allocated memory) or be inserting special initialization code.
//
struct InitAllocas : public FunctionPass, InstVisitor<InitAllocas> {
  public:
    static char ID;
    InitAllocas() : FunctionPass(ID) {}
    const char *getPassName() const { return "Init Alloca Pass"; }
    virtual bool runOnFunction (Function &F);
    bool doInitialization (Module & M);
    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.setPreservesCFG();
    }
    void visitAllocaInst (AllocaInst & AI);
};

}
 
#endif
