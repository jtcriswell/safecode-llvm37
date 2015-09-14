//===- BaggyBoundsChecks.h - Modify code for baggy bounds checks --------------//
// 
//                          The SAFECode Compiler 
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file defines a pass that modifies global and stack allocations to
// allocate memory objects with a power-of-two size and with the alignment
// needed for baggy bounds checking.
//
//===----------------------------------------------------------------------===//

#ifndef _BAGGY_BOUNDS_CHECKS_H_
#define _BAGGY_BOUNDS_CHECKS_H_

#include "llvm/Pass.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include "llvm/Transforms/Utils/Cloning.h"


#include "safecode/SAFECode.h"

namespace llvm {

//
// Pass:InsertBaggyBoundsChecks 
//
// Description:
//  This pass aligns all globals and allocas.
//
struct InsertBaggyBoundsChecks : public ModulePass {
  public:
    static char ID;
    InsertBaggyBoundsChecks () : ModulePass (ID) { }
    const char *getPassName() const { return "Insert BaggyBounds Checks"; }
    virtual bool runOnModule(Module &M);
    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    };

  protected:
    // Pointers to required passes
    const DataLayout * TD;

    // Protected methods
    void adjustGlobalValue (GlobalValue * GV);
    void adjustAlloca (AllocaInst * AI);
    void adjustAllocasFor (Function * F);
    void adjustArgv(Function *F);
    void cloneFunctionInto(Function *NewFunc, 
                           const Function *OldFunc,
                           ValueToValueMapTy &VMap,
                           bool ModuleLevelChanges,
                           SmallVectorImpl<ReturnInst*> &Returns,
                           const char *NameSuffix = "",
                           ClonedCodeInfo *CodeInfo = 0,
                           ValueMapTypeRemapper *TypeMapper = 0);

    Function * cloneFunction(Function * F);
    void callClonedFunction(Function * F, Function * NewF);
};

}
#endif
