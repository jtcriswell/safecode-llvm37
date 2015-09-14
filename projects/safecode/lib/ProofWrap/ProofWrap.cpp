//===- ProofWrap.cpp: SAFECode Type Checker -------------------------------===//
//
//                          The SAFECode Compiler
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file is part of the type-checker for SAFECode.
//
//===----------------------------------------------------------------------===//

#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Constants.h"
#include "ProofWrap/ProofWrap.h"

using namespace llvm;

namespace {
  RegisterPass<ProofStrip> X("proofstrip", "Strip proof markers");
}

bool ProofStrip::runOnModule(Module& M) {
  std::vector<const Type * > FTV;
  FTV.push_back(Type::LongTy);
  FunctionType* FT = FunctionType::get(Type::VoidTy, FTV, true);
  Function* F = M.getFunction("llvm.proof.ptr", FT);
  if (F) {
    while (!F->use_empty()) {
      User* U = *F->use_begin();
      CallInst* CI = cast<CallInst>(U);
      const Value* PV = CI->getOperand(1);
      for (unsigned x = 2; x < CI->getNumOperands(); ++x)
        setProof(CI->getOperand(x), PV);
      CI->eraseFromParent();
    }
    return true;
  }
  return false;
}

//ProofPlace p;
