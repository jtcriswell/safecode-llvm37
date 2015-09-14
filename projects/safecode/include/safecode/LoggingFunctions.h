//===---- LoggingFunctions.h - Register va_lists for securing vprintf() ---===//
// 
//                          The SAFECode Compiler 
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// These passes identify where va_lists are created, and add calls around these
// sites so that the SAFECode vprintf() / vscanf() wrappers can identify the
// contents of these lists.
//
//===----------------------------------------------------------------------===//

#ifndef LOGGING_FUNCTIONS_H
#define LOGGING_FUNCTIONS_H

#include "llvm/IR/CallSite.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

#include "safecode/SAFECode.h"

#include <string>
#include <vector>
#include <map>

using std::string;
using std::vector;
using std::map;

namespace llvm
{
  class RegisterVarargCallSites :
    public ModulePass, public InstVisitor<RegisterVarargCallSites> {
    private:
      static const char *ExternalVarargFunctions[];
      map<Function *, bool> shouldRegister;
      vector<CallSite> toRegister;
      Value *registrationFunc, *unregistrationFunc;
      void makeRegistrationFunctions(Module &M);
      static bool isExternalVarargFunction(const string &name);
      void registerCallSite(Module &M, CallSite &CS);
    public:
      static char ID;
      RegisterVarargCallSites() : ModulePass(ID) {}
      bool runOnModule(Module &M);
      void visitCallInst(CallInst &I);
  };

  class LoggingFunctions : public ModulePass {
    private:
      Value *targetCheckFunc, *vaRegisterFunc, *vaCopyRegisterFunc;
      map<Function *, Value *> targetCheckCalls;
      void registerVaStartCallSite(CallSite &CS);
      void registerVaCopyCallSite(CallSite &CS);
    public:
      static char ID;
      LoggingFunctions() : ModulePass(ID) {}
      bool runOnModule(Module &M);
  };
}

#endif
