//===- OptimizeChecks.cpp - Optimize SAFECode Run-time Checks -----*- C++ -*--//
// 
//                     The LLVM Compiler Infrastructure
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This pass optimizes some of the SAFECode run-time checks.
//
//===----------------------------------------------------------------------===//

#ifndef SAFECODE_OPTIMIZECHECKS_H
#define SAFECODE_OPTIMIZECHECKS_H

#include "llvm/ADT/DenseSet.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

#include "safecode/CheckInfo.h"
#include "safecode/AllocatorInfo.h"

namespace llvm {

//
// Pass: OptimizeChecks
//
// Description:
//  This pass examines the run-time checks that SAFECode has inserted into a
//  program and attempts to remove checks that are unnecessary.
//
struct OptimizeChecks : public ModulePass {
  private:
    // Private methods
    bool processFunction (Module & M, const struct CheckInfo & Info);
    bool onlyUsedInCompares (Value * Val);

  public:
    static char ID;
    OptimizeChecks() : ModulePass(ID) {}
    virtual bool runOnModule (Module & M);

    const char *getPassName() const {
      return "Optimize SAFECode Run-time Checks";
    }

    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.setPreservesCFG();
    }
};

//
// Pass: GlobalRegisterOpt
//
// Description:
//  This pass looks for global variables that are never used in run-time checks
//  that perform lookups.  Such global variables do not need to be registered
//  with pool_register_global(), so we remove such registrations.
//
struct GlobalRegisterOpt : public ModulePass {
  private:
  public:
    static char ID;
    GlobalRegisterOpt() : ModulePass(ID) {}
    virtual bool runOnModule (Module & M);

    const char *getPassName() const {
      return "Optimize SAFECode Global Object Registration";
    }

    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.setPreservesCFG();
    }
};

}

#endif
