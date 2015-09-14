//===- DetectDanglingPointers.cpp - Dangling Pointer Detection Pass ------- --//
// 
//                          The SAFECode Compiler 
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This pass performs necessary transformations for catching dangling pointer
// errors.
//
//===----------------------------------------------------------------------===//

#ifndef DETECTDANGLINGPOINTERS_H
#define DETECTDANGLINGPOINTERS_H

#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

#include "safecode/SAFECode.h"
#include "safecode/Intrinsic.h"
#include "safecode/PoolHandles.h"
#include "safecode/Support/AllocatorInfo.h"

#include <set>

using namespace llvm;

NAMESPACE_SC_BEGIN

//
// Pass: DetectDanglingPointers
//
// Description:
//  This pass modifies a program so that it can detect dangling pointers at
//  run-time.  Most dangling pointer errors are caught by other SAFECode
//  passes; this pass is primarily for marking memory pages inaccessible
//  when an object is freed.
//
struct DetectDanglingPointers : public ModulePass {
  private:
    // Private variables
    InsertSCIntrinsic * intrinPass;
    Constant * ProtectObj;
    Constant * ShadowObj;

  protected:
    void createFunctionProtos (Module & M);
    void processFrees (Module & M, std::set<Function *> & FreeFuncs);

  public:
    static char ID;
    DetectDanglingPointers() : ModulePass((intptr_t)(&ID)) {}
    virtual bool runOnModule (Module & M);
    const char *getPassName() const {
      return "Dangling Pointer Detection Pass";
    }
    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      // This pass gives us information on the various run-time checks
      AU.addRequired<InsertSCIntrinsic>();
      AU.addRequired<AllocatorInfoPass>();
    }
};

NAMESPACE_SC_END

#endif
