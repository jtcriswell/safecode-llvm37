//===- RegisterVarargCallSites.cpp - Register vararg call sites -----------===//
// 
//                            The SAFECode Compiler
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file adds registration/unregistration information at each call site of
// a variable argument function in the program, so that SAFECode can match
// a va_list with its arguments.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/Constants.h"

#include "safecode/LoggingFunctions.h"
#include "safecode/VectorListHelper.h"

#include <set>

using std::set;

namespace llvm
{

static RegisterPass<RegisterVarargCallSites>
R("registervarargcallsites", "Add registrations around vararg call sites");

char RegisterVarargCallSites::ID = 0;

// A list of all vararg functions we consider external, consequently no
// registration needs to be added for calls to these functions.
const char *RegisterVarargCallSites::ExternalVarargFunctions[] = {
  // printf() family and SAFECode versions
  "printf", "fprintf", "sprintf", "snprintf", "asprintf", "dprintf", "wprintf",
  "fwprintf", "swprintf", "pool_printf", "pool_fprintf", "pool_sprintf",
  "pool_snprintf",
  // scanf() family and SAFECode versions
  "scanf", "fscanf", "sscanf", "wscanf", "fwscanf", "swscanf", "pool_scanf",
  "pool_fscanf", "pool_sscanf",
  // syslog() and SAFECode version
  "syslog", "pool_syslog",
  // error() family
  "error", "error_at_line",
  // err() family and SAFECode versions
  "err", "errx", "warn", "warnx", "pool_err", "pool_errx", "pool_warn",
  "pool_warnx",
  // Vararg SAFECode intrinsics
  "__sc_fscallinfo", "__sc_fscallinfo_debug", "__sc_vacallregister",
  // Other functions
  "strfmon", "strfmon_l", "ulimit", 
  // System calls
  "ioctl", "execl", "execlp", "execle", "mq_open", "sem_open", "open",
  "semctl", NULL
};

bool RegisterVarargCallSites::runOnModule(Module &M) {
  bool modified = false;
  registrationFunc = unregistrationFunc = 0;
  // Find all call sites that need registration.
  visit(M);
  // Go over the discovered call sites.
  vector<CallSite>::iterator site = toRegister.begin();
  vector<CallSite>::iterator end  = toRegister.end();
  for (; site != end; ++site) {
    registerCallSite(M, *site);
    modified = true;
  }
  return modified;
}

// Add declaration for the vararg call site registration / unregistration
// functions.
void RegisterVarargCallSites::makeRegistrationFunctions(Module &M) {
  Type *Int32Ty   = Type::getInt32Ty(M.getContext());
  Type *VoidPtrTy = Type::getInt8PtrTy(M.getContext());
  Type *VoidTy    = Type::getVoidTy(M.getContext());
  vector<Type *> arglist = args<Type *>::list(VoidPtrTy, Int32Ty);
  FunctionType *vaCallRegisterType = FunctionType::get(VoidTy, arglist, true);
  FunctionType *vaCallUnregisterType = FunctionType::get(VoidTy, false);
  //
  // TODO: There needs to be a means of a) passing pool parameters for the
  // arguments as well as b) passing completeness information for the
  // arguments.
  //
#ifndef NDEBUG
  Function *registration = M.getFunction("__sc_vacallregister");
  Function *unregistration = M.getFunction("__sc_vacallunregister");
  assert((registration == 0 ||
    registration->getFunctionType() == vaCallRegisterType ||
    registration->hasLocalLinkage()) &&
    "Intrinsic declared with wrong type!");
  assert((unregistration == 0 ||
    unregistration->getFunctionType() == vaCallUnregisterType ||
    unregistration->hasLocalLinkage()) &&
    "Intrinsic declared with wrong type!");
#endif
  registrationFunc = M.getOrInsertFunction(
    "__sc_vacallregister", vaCallRegisterType
  );
  unregistrationFunc = M.getOrInsertFunction(
    "__sc_vacallunregister", vaCallUnregisterType
  );
}

// Check if the given function is a known external vararg function.
bool RegisterVarargCallSites::isExternalVarargFunction(const string &f) {
  for (unsigned i = 0; ExternalVarargFunctions[i] != NULL; ++i) {
    if (f == ExternalVarargFunctions[i])
      return true;
  }
  return false;
}

// Add calls to the registration functions around this call site.
void RegisterVarargCallSites::registerCallSite(Module &M, CallSite &CS) {
  // Insert the registration intrinsics.
  if (registrationFunc == 0 || unregistrationFunc == 0)
    makeRegistrationFunctions(M);
  Instruction *inst = CS.getInstruction();
  LLVMContext &C = M.getContext();
  Type *VoidPtrTy = Type::getInt8PtrTy(C);
  Type *Int32Ty   = Type::getInt32Ty(C);
  // Build the argument vector to vacallregister.
  vector<Value *> vaCallRegisterArgs(2);
  Value *dest = CS.getCalledValue();
  Value *destPtr;
  // Get the function pointer casted to i8*.
  if (isa<Constant>(dest))
    destPtr = ConstantExpr::getPointerCast(cast<Constant>(dest), VoidPtrTy);
  else 
    destPtr = new BitCastInst(dest, VoidPtrTy, "", inst);
  vaCallRegisterArgs[0] = destPtr;
  vaCallRegisterArgs[1] = ConstantInt::get(Int32Ty, CS.arg_size());
  // Register all the pointer arguments to this function call as well.
  set<Value *> pointerArguments;
  CallSite::arg_iterator arg = CS.arg_begin();
  CallSite::arg_iterator end = CS.arg_end();
  for (; arg != end; ++arg) {
    Value *argval = *arg;
    if (isa<PointerType>(argval->getType())) {
      if (pointerArguments.find(argval) == pointerArguments.end()) {
        pointerArguments.insert(argval);
        vaCallRegisterArgs.push_back(argval);
      }
    }
  }
  // End the argument list with a NULL parameter.
  vaCallRegisterArgs.push_back(
    ConstantPointerNull::get(cast<PointerType>(VoidPtrTy))
  );
  // Add the registration call before the call site.
  CallInst::Create(registrationFunc, vaCallRegisterArgs, "", inst);
  // Add the unregistration call after the call site.
  Instruction *unreg = CallInst::Create(unregistrationFunc);
  unreg->insertAfter(inst);
  return;
}

// Determine if the given call instruction should be registered.
void RegisterVarargCallSites::visitCallInst(CallInst &I) {
  //
  // Do not register inline assembly instructions.
  //
  if (I.isInlineAsm())
    return;

  CallSite CS(&I);
  Function *f = CS.getCalledFunction();
  // If this is an indirect call, conservatively register it.
  if (f == 0) {
    toRegister.push_back(CS);
    return;
  }
  // Check whether we know to register this function.
  map<Function *, bool>::iterator found = shouldRegister.find(f);
  // If we've found the function, register the call site if we know that this
  // function should be registered.
  if (found != shouldRegister.end()) {
    if (shouldRegister[f])
      toRegister.push_back(CS);
  }
  // The function has not been encountered yet.
  // Determine if calls to this function should be registered.
  else {
    if (f->isVarArg() && !isExternalVarargFunction(f->getName().str())) {
      shouldRegister[f] = true;
      toRegister.push_back(CS);
    }
    else
      shouldRegister[f] = false;
  }
}

}
