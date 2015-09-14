//===- include/InsertChecks/RegisterRuntimeInitializer.h --------*- C++ -*----//
// 
//                          The SAFECode Compiler 
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// Pass to register runtime initialization calls into user-space programs.
//
//===----------------------------------------------------------------------===//

#ifndef _REGISTER_RUNTIME_INITIALIZER_H_
#define _REGISTER_RUNTIME_INITIALIZER_H_

#include "safecode/SAFECode.h"

#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/DataLayout.h"

NAMESPACE_SC_BEGIN

/// Base class of all passes which register variables into pools.
class RegisterRuntimeInitializer : public llvm::ModulePass {
public:
  static char ID;
  RegisterRuntimeInitializer() : llvm::ModulePass((uintptr_t) &ID) {}
  virtual bool runOnModule(llvm::Module & M);
  virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const {
    AU.setPreservesAll();
  }
private:
  /// Construct the initializer
  void constructInitializer(llvm::Module & M);
  /// Insert the initializer into llvm.global_ctors
  void insertInitializerIntoGlobalCtorList(llvm::Module & M);
};

NAMESPACE_SC_END

#endif
