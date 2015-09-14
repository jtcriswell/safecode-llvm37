//===- Terminate.cpp --------------------------------------------*- C++ -*----//
// 
//                     The LLVM Compiler Infrastructure
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// Pass to modify SAFECode's initialization in the program to terminate on the
// first memory safety error.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/CallSite.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

namespace llvm {
  class Terminate : public llvm::ModulePass {
    public:
      static char ID;
      Terminate () : llvm::ModulePass(ID) {
      }
      virtual bool runOnModule(llvm::Module & M);
      virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const {
        AU.setPreservesCFG();
      }
  };

  ModulePass * createSCTerminatePass (void);
}

using namespace llvm;

// ID for the Terminate pass
char Terminate::ID = 0;

//
// Method: runOnModule()
//
// Description:
//  This method is the entry point for this LLVM pass.  We look for calls to
//  the pool_init_runtime() function in the program and modify them to tell the
//  run-time to terminate the program when a memory error is detected.
//
// Return value:
//  true  - The program was modified in some way.
//  false - This pass did not modify the program.
//
bool
Terminate::runOnModule (Module & M) {
  //
  // Find the pool_init_runtime() function.  If it does not exit, then there
  // is nothing to do.
  //
  Function * F = M.getFunction ("pool_init_runtime");
  if (!F)
    return false;

  //
  // Scan through all uses of the function looking for calls to it.  If we
  // find them, modify the last argument (the terminate argument) to be true.
  //
  bool modified = false;
  Type * Int32Type = IntegerType::getInt32Ty(M.getContext());
  Function::use_iterator i, e;
  for (i = F->use_begin(), e = F->use_end(); i != e; ++i) {
    if (CallInst * CI = dyn_cast<CallInst>(*i)) {
      CallSite CS(CI);
      CS.setArgument(2, ConstantInt::get(Int32Type, 1));
      modified = true;
    }
  }

  return modified;
}

// Function to allow external code to create objects of this pass
ModulePass *
llvm::createSCTerminatePass (void) {
  return new Terminate();
}
