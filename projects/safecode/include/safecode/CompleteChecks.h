//===- CompleteChecks.h - Make run-time checks Complete ----------------------//
// 
//                          The SAFECode Compiler 
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file defines a pass that modifies SAFECode run-time checks to be
// complete.  A complete check is for memory objects that are complete analyzed
// by SAFECode; if the run-time check fails, we *know* that it is an error.
//
//===----------------------------------------------------------------------===//

#ifndef _SAFECODE_COMPLETE_CHECKS_H_
#define _SAFECODE_COMPLETE_CHECKS_H_

#include "dsa/DataStructure.h"
#include "dsa/DSGraph.h"

#include "llvm/Analysis/CallGraph.h"
#include "llvm/Pass.h"

namespace llvm {

//
// Pass: CompleteChecks
//
// Description:
//  This pass searches for SAFECode run-time checks.  If the checks are on
//  complete DSNodes, then it modifies the check to use a complete version of
//  the run-time check function.
//
struct CompleteChecks : public ModulePass {
  public:
    static char ID;
    CompleteChecks () : ModulePass (ID) { }
    const char *getPassName() const { return "Complete Run-time Checks"; }
    virtual bool runOnModule (Module & M);
    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      // Required passes
      AU.addRequired<CallGraphWrapperPass>();
      AU.addRequired<EQTDDataStructures>();

      // Preserved passes
      AU.setPreservesCFG();
    };

  protected:
    // Protected methods
    DSNodeHandle getDSNodeHandle (const Value * V, const Function * F);
    void makeComplete (Module & M, const struct CheckInfo & CheckInfo);
    void makeCStdLibCallsComplete(Function *, unsigned, bool);
    void makeFSParameterCallsComplete(Module &M);
    void fixupCFIChecks (Module & M, std::string name);
    void getFunctionTargets (CallSite CS, std::vector<const Function *> & T);
};

}
#endif
