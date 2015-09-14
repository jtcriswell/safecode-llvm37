//===- InsertPoolChecks.h - Insert run-time checks for SAFECode --------------//
// 
//                          The SAFECode Compiler 
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file implements a pass to remove special attributes from the
// run-time checking functions.
//
//===----------------------------------------------------------------------===//

#include <vector>
#include <algorithm>

#include "llvm/Pass.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Function.h"

#include "safecode/Intrinsic.h"
#include "safecode/Config/config.h"

using namespace llvm;
NAMESPACE_SC_BEGIN

struct CleanAttribute {
  void operator()(Function * F) const {
    F->setOnlyReadsMemory(false);
  }
};

//
// Pass: ClearCheckAttributes
//
// Description:
//  Remove special attributes from the run-time checking functions.
//
struct ClearCheckAttributes : public ModulePass {
public:
  static char ID;
  ClearCheckAttributes() : ModulePass((intptr_t) &ID) {};
  virtual ~ClearCheckAttributes() {};

  virtual bool runOnModule (Module & M) {
    InsertSCIntrinsic * intrinsic = &getAnalysis<InsertSCIntrinsic>();
    Funcs.push_back (intrinsic->getIntrinsic("sc.lscheck").F);
    Funcs.push_back (intrinsic->getIntrinsic("sc.lscheckui").F);
    Funcs.push_back (intrinsic->getIntrinsic("sc.lscheckalign").F);
    Funcs.push_back (intrinsic->getIntrinsic("sc.lscheckalignui").F);

    struct CleanAttribute functor;
    std::for_each(Funcs.begin(), Funcs.end(), functor);

    return false;
  }

  virtual const char * getPassName() const {
    return "Clear attributes on run-time functions";
  }

  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<InsertSCIntrinsic>();
    AU.setPreservesAll();
    AU.setPreservesCFG();
  };

private:
  std::vector<Function *> Funcs;
};

char ClearCheckAttributes::ID = 0;
static RegisterPass<ClearCheckAttributes> X("sc-clear-attr", 
                                            "remove special attributes from the run-time checking functions.");
ModulePass * createClearCheckAttributesPass() { return new ClearCheckAttributes(); }

NAMESPACE_SC_END
