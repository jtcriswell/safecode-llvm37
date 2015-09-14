//===----- LoggingFunctions.cpp - Register va_lists in the program --------===//
// 
//                            The SAFECode Compiler
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file implements a pass that adds calls to register when va_lists are
// created and copied, so that when a logging-style function is called,
// SAFECode can associate its va_list with an argument list.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/Constants.h"
#include "llvm/IR/ValueSymbolTable.h"

#include "safecode/LoggingFunctions.h"
#include "safecode/VectorListHelper.h"

namespace llvm
{

static RegisterPass<LoggingFunctions>
R("loggingfunctions", "Instrument vararg functions that work with va_lists");

char LoggingFunctions::ID = 0;

// Module initialization: add the required intrinsics if necessary.
bool LoggingFunctions::runOnModule(Module &M) {
  bool modified = false;
  Function *vaStart = M.getFunction("llvm.va_start");
  // Look for va_start() calls to register.
  if (vaStart != 0) {
    vector<CallSite> vaStartCalls;
    Value::use_iterator vaStartUse = vaStart->use_begin();
    Value::use_iterator vaStartEnd = vaStart->use_end();
    // Find all va_start() call sites within vararg functions.
    for (; vaStartUse != vaStartEnd; ++vaStartUse) {
      CallSite CS(*vaStartUse);
      // Only concern ourselves with direct calls to va_start().
      if (!CS || CS.getCalledFunction() != vaStart)
        continue;
      // Only concern ourselves with calls inside vararg functions.
      if (!CS.getInstruction()->getParent()->getParent()->isVarArg())
        continue;
      vaStartCalls.push_back(CS);
    }
    // At least one relevant use of va_start was found....
    if (!vaStartCalls.empty()) {
      // Declare the SAFECode intrinsics we will need.
      Type *VoidTy    = Type::getVoidTy(M.getContext());
      Type *VoidPtrTy = Type::getInt8PtrTy(M.getContext());
      Type *Int32Ty   = Type::getInt32Ty(M.getContext());
      vector<Type *> tcArgTypes = args<Type *>::list(VoidPtrTy);
      vector<Type *> vrArgTypes = args<Type *>::list(VoidPtrTy, Int32Ty);
      FunctionType *tcType = FunctionType::get(Int32Ty, tcArgTypes, false);
      FunctionType *vrType = FunctionType::get(VoidTy, vrArgTypes, false);
#ifndef NDEBUG
      Function *tcInModule = M.getFunction("__sc_targetcheck");
      Function *vrInModule = M.getFunction("__sc_varegister");
      assert((tcInModule == 0 ||
        tcInModule->getFunctionType() == tcType ||
        tcInModule->hasLocalLinkage()) &&
        "Intrinsic already declared with wrong type!");
      assert((vrInModule == 0 ||
        vrInModule->getFunctionType() == vrType ||
        vrInModule->hasLocalLinkage()) &&
        "Intrinsic already declared with wrong type!");
#endif
      targetCheckFunc = M.getOrInsertFunction("__sc_targetcheck", tcType);
      vaRegisterFunc = M.getOrInsertFunction("__sc_varegister", vrType);
      // Now register all found calls....
      for (unsigned i = 0, end = vaStartCalls.size(); i < end; ++i)
        registerVaStartCallSite(vaStartCalls[i]);
      modified = true;
    }
  }
  // Now check if we need to register va_copy() calls.
  Function *vaCopy = M.getFunction("llvm.va_copy");
  if (vaCopy != 0) {
    vector<CallSite> vaCopyCalls;
    Value::use_iterator vaCopyUse = vaCopy->use_begin();
    Value::use_iterator vaCopyEnd = vaCopy->use_end();
    // Find all va_start() call sites within vararg functions.
    for (; vaCopyUse != vaCopyEnd; ++vaCopyUse) {
      CallSite CS(*vaCopyUse);
      // Only concern ourselves with direct calls to va_copy().
      if (!CS || CS.getCalledFunction() != vaCopy)
        continue;
      vaCopyCalls.push_back(CS);
    }
    // At least one relevant use of va_copy() was found...
    if (!vaCopyCalls.empty()) {
      // Add a declaration for the SAFECode intrinsic we need...
      Type *VoidTy    = Type::getVoidTy(M.getContext());
      Type *VoidPtrTy = Type::getInt8PtrTy(M.getContext());
      vector<Type *> vcArgTypes = args<Type *>::list(VoidPtrTy, VoidPtrTy);
      FunctionType *vcType = FunctionType::get(VoidTy, vcArgTypes, false);
#ifndef NDEBUG
      Function *vcInModule = M.getFunction("__sc_vacopyregister");
      assert((vcInModule == 0 ||
        vcInModule->getFunctionType() == vcType ||
        vcInModule->hasLocalLinkage()) &&
        "Intrinsic already declared with wrong type!");
#endif
      vaCopyRegisterFunc = M.getOrInsertFunction("__sc_vacopyregister", vcType);
      for (unsigned i = 0, end = vaCopyCalls.size(); i < end; ++i)
        registerVaCopyCallSite(vaCopyCalls[i]);
      modified = true;
    }
  }
  return modified;
}

// Add calls that associate the va_list in a call of va_start with the
// function's list of arguments.
void LoggingFunctions::registerVaStartCallSite(CallSite &CS) {
  Function *F = CS.getInstruction()->getParent()->getParent();
  map<Function *, Value *>::iterator found = targetCheckCalls.find(F);
  // Add a check at the entry of this function to determine if it is the
  // expected callee (needed for correctness).
  if (found == targetCheckCalls.end()) {
    Type *VoidPtrTy = Type::getInt8PtrTy(F->getContext());
    BasicBlock &start = F->getEntryBlock();
    Value *castedF = ConstantExpr::getBitCast(F, VoidPtrTy);
    vector<Value *> tcParams = args<Value *>::list(castedF);
    Instruction *tcCall = CallInst::Create(targetCheckFunc, tcParams);
    tcCall->insertBefore(&start.front());
    targetCheckCalls[F] = tcCall;
  }
  // Add a call to the registration function after the call of va_start().
  Value *TC = targetCheckCalls[F];
  Instruction *inst = CS.getInstruction();
  Value *vaList = CS.getArgument(0);
  vector<Value *> params = args<Value *>::list(vaList, TC);
  Instruction *registration = CallInst::Create(vaRegisterFunc, params);
  registration->insertAfter(inst);
}

// Add a call that associates registration information from one va_list to
// another in a va_copy() operation.
void LoggingFunctions::registerVaCopyCallSite(CallSite &CS) {
  Instruction *inst = CS.getInstruction();
  vector<Value *> params(2);
  params[0] = CS.getArgument(0);
  params[1] = CS.getArgument(1);
  Instruction *registration = CallInst::Create(vaCopyRegisterFunc, params);
  registration->insertAfter(inst);
}

}
