//===- SpeculativeChecking.cpp: -------------------------------------------===//
//
//                          The SAFECode Compiler
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file define a pass to lower synchronous checking calls to speculative
// checking calls.
//
//===----------------------------------------------------------------------===//

#include <iostream>
#include <set>
#include <map>
#include "llvm/Module.h"
#include "llvm/Support/CommandLine.h"
#include "safecode/Config/config.h"
#include "safecode/SpeculativeChecking.h"
#include "safecode/VectorListHelper.h"
#include "safecode/InsertChecks.h"
#include "SCUtils.h"

using namespace llvm;
NAMESPACE_SC_BEGIN

/// Static Members
namespace {
  typedef std::set<std::string> CheckFuncSetTy;
  typedef std::set<std::string> SafeFuncSetTy;
  SafeFuncSetTy sSafeFuncSet;
  CheckFuncSetTy sCheckFuncSet;
  Function * sFuncWaitForSyncToken;
  llvm::RegisterPass<ParCheckingCallAnalysis> callAnalysisPass("par-check-call-analysis", "Determine which calls are safe to not inserting sync points before them", true, true);
  llvm::RegisterPass<SpeculativeCheckingInsertSyncPoints> X("par-check-sync-points", "Insert sync points before external functions");
}

// here are the functions are considered as "safe",
// either we know the semantics of them or they are not handled
// TODO: add stuffs like strlen / strcpy / strncpy
static const char * safeFunctions[] = {
  //  "poolinit", "pool_init_runtime",
  "pool_init_runtime",
  "memset", "memcmp",
  "llvm.memcpy.i32", "llvm.memcpy.i64",
  "llvm.memset.i32", "llvm.memset.i64",
  "llvm.memmove.i32", "llvm.memmove.i64",
  "llvm.sqrt.f64",
  // HACK: these two should be handled by poolalloc
  "calloc", "realloc",
  // These functions are not marked as "readonly"
  // So we have to add them to the list explicitly
  "atoi", "atof", "srand", "fabs", "random", "srandom", "drand48",
  "pow", "sqrt", "lrand48",
  "clock",
  "strcpy", "strncpy"
};

// TODO: Should use the information from the intrinsic pass
// Functions used in checking

static const char * checkingFunctions[] = {
  "sc.lscheck", "sc.lscheckui", "sc.lscheckalign", "sc.lscheckalignui",
  "sc.boundscheck", "sc.boundscheckui", "sc.exactcheck", "sc.exactcheck2",
  "sc.lscheck.serial", "sc.lscheckui.serial", "sc.lscheckalign.serial",
  "sc.lscheckalignui.serial", "sc.boundscheck.serial",
  "sc.boundscheckui.serial", "sc.exactcheck.serial", "sc.exactcheck2.serial",
  
  "funccheck",

  // Function for pool allocations
  "poolinit", "pooldestroy", "poolargvregister", "poolregister",
  "poolunregister", "poolalloc", "poolrealloc", "poolstrdup", "poolcalloc",
  "poolfree"
};

// Helper functions
namespace {
  static bool
  isCheckingCall(const Function * F) {
    if (!F) return false;
    std::string FName = F->getName();
    CheckFuncSetTy::const_iterator it = sCheckFuncSet.find(FName);
    return it != sCheckFuncSet.end();
  }

  static bool
  isSafeDirectCall(const Function * F) {
    if (!F) return false;
    const std::string & FName = F->getName();
  
    // in the exception list?
    SafeFuncSetTy::const_iterator it = sSafeFuncSet.find(FName);
    if (it != sSafeFuncSet.end() || isCheckingCall(F)) return true;
    
    if (!F->isDeclaration()) return true;
    if (F->onlyReadsMemory()) return true;
    return false;
  }

  class InitializeFunctionList {
  public:
    InitializeFunctionList() {
      for (size_t i = 0; i < sizeof(checkingFunctions) / sizeof(const char *); ++i) {
        sCheckFuncSet.insert(checkingFunctions[i]);
      }

      for (size_t i = 0; i < sizeof(safeFunctions) / sizeof(const char *); ++i) {
        sSafeFuncSet.insert(safeFunctions[i]);
      }
    };
  }; 
  static InitializeFunctionList initializer;
}




char SpeculativeCheckingInsertSyncPoints::ID = 0;

  ////////////////////////////////////////////////////////////////////////////
  // SpeculativeCheckingInsertSyncPoints Methods
  ////////////////////////////////////////////////////////////////////////////

  bool
  SpeculativeCheckingInsertSyncPoints::doInitialization(Module & M) {
    const Type * VoidType  = Type::getVoidTy(M.getContext());
    sFuncWaitForSyncToken = Function::Create
      (FunctionType::get
       (VoidType, std::vector<const Type*>(), false),
       GlobalValue::ExternalLinkage,
       "__sc_par_wait_for_completion", 
       &M);
    return true;
  }

  bool
  SpeculativeCheckingInsertSyncPoints::runOnBasicBlock(BasicBlock & BB) {
#ifdef PAR_CHECKING_ENABLE_INDIRECTCALL_OPT
    dsnodePass = &getAnalysis<DSNodePass>();
    callSafetyAnalysis = &getAnalysis<ParCheckingCallAnalysis>();
#endif
    bool changed = false;

    for (BasicBlock::iterator I = BB.begin(); I != BB.end(); ++I) {
      if (CallInst * CI = dyn_cast<CallInst>(I)) {
        Function * F = CI->getCalledFunction();
        if (isSafeDirectCall(F)) continue;
        changed |= insertSyncPointsBeforeExternalCall(CI);
      }
    }
    removeRedundantSyncPoints(BB);
    return changed;
  }

  bool
  SpeculativeCheckingInsertSyncPoints::insertSyncPointsBeforeExternalCall(CallInst * CI) {
    CallInst * origCI = getOriginalCallInst(CI);
    if (callSafetyAnalysis->isSafe(origCI)) {
      return false;
    } else {
      CallInst::Create(sFuncWaitForSyncToken, "", CI);
      return true;
    }
  }

  CallInst *
  SpeculativeCheckingInsertSyncPoints::getOriginalCallInst(CallInst * CI) {
    Function * F = CI->getParent()->getParent();
    PA::FuncInfo *FI = dsnodePass->paPass->getFuncInfo(*F);
    if (!FI) {
      F = dsnodePass->paPass->getOrigFunctionFromClone(F);
      if (!F) return CI;
      FI = dsnodePass->paPass->getFuncInfo(*F);
      if (!FI) return CI;      
    }
    Value * origVal = FI->MapValueToOriginal(CI);
    if (!origVal) return CI;
    CallInst * origCI = dyn_cast<CallInst>(origVal);
    return origCI ? origCI : CI;
  }

  // A simple HACK to remove redudant synchronization points in this cases:
  //
  // call external @foo
  // spam... but does not do any pointer stuffs
  // call external @bar
  // 
  // we only need to insert a sync point before foo
  void
  SpeculativeCheckingInsertSyncPoints::removeRedundantSyncPoints(BasicBlock & BB) {
    std::vector<CallInst *> toBeRemoved;
    bool haveSeenCheckingCall = true;    
    for (BasicBlock::iterator I = BB.begin(), E = BB.end(); I != E; ++I) {
      if (CallInst * CI = dyn_cast<CallInst>(I)) { 
        Function * F = CI->getCalledFunction();
        bool checkingCall = isCheckingCall(F);
        haveSeenCheckingCall |= checkingCall; 
        if (F != sFuncWaitForSyncToken) continue;
        if (!haveSeenCheckingCall) {
          toBeRemoved.push_back(CI);
        }
        // Reset the flag
        haveSeenCheckingCall = false;
      }
    }
    for (std::vector<CallInst*>::iterator it = toBeRemoved.begin(), end = toBeRemoved.end(); it != end; ++it) {
      (*it)->eraseFromParent();
    }
  }

  ///
  /// SpeculativeCheckStoreCheckPass methods
  ///
  char SpeculativeCheckStoreCheckPass::ID = 0;
  static Constant * funcStoreCheck;

  bool SpeculativeCheckStoreCheckPass::doInitialization(Module & M) {
    const Type * VoidType  = Type::getVoidTy(M.getContext());
    std::vector<const Type *> args;
    args.push_back(getVoidPtrType(M));
    FunctionType * funcStoreCheckTy = FunctionType::get(VoidType, args, false);
    funcStoreCheck = M.getOrInsertFunction("__sc_par_store_check", funcStoreCheckTy);
    return true;
  }

  // TODO: Handle volatile instructions
  bool SpeculativeCheckStoreCheckPass::runOnBasicBlock(BasicBlock & BB) {
    bool changed = false;    
    for (BasicBlock::iterator I = BB.begin(), E = BB.end(); I != E; ++I) {
      if (StoreInst * SI = dyn_cast<StoreInst>(I)) {
        Instruction * CastedPointer = CastInst::CreatePointerCast(SI->getPointerOperand(), getVoidPtrType(BB.getContext()), "", SI);
  
        CallInst::Create(funcStoreCheck, CastedPointer, "", SI);
        changed = true;
      }
    }
    return changed;
  }


  /// ParCheckingCallAnalysis Methods
  ///

  char ParCheckingCallAnalysis::ID = 0;

  bool
  ParCheckingCallAnalysis::isSafe(CallSite CS) const {
    std::set<CallSite>::const_iterator it = CallSafetySet.find(CS);
    if (it == CallSafetySet.end()) {
      return false;
    } else {
      return true;
    }
  }

  bool
  ParCheckingCallAnalysis::runOnModule(Module & M) {
    bool changed = false;
    for (Module::iterator FI = M.begin(), FE = M.end(); FI != FE; ++FI) {
      for (Function::iterator I = FI->begin(), E = FI->end(); I != E; ++I) {
        changed |= runOnBasicBlock(*I);
      }
    }
    return changed;
  }

  bool
  ParCheckingCallAnalysis::runOnBasicBlock(BasicBlock & BB) {
    CTF = &getAnalysis<CallTargetFinder<EQTDDataStructures> >();

    for (BasicBlock::iterator I = BB.begin(), E = BB.end(); I != E; ++I) {
      CallSite CS(CallSite::get(I));
      if (CS.getInstruction() && isSafeCallSite(CS)) {
        CallSafetySet.insert(CS);
      }
    }
    return false;
  }

  bool
  ParCheckingCallAnalysis::isSafeCallSite(CallSite CS) const {
    Function * F = CS.getCalledFunction();
    
    if (isSafeDirectCall(F)) return true;

    if (!F && isSafeIndirectCall(CS)) return true; 

    return false;
  }

  bool
  ParCheckingCallAnalysis::isSafeIndirectCall(CallSite CS) const {
    typedef std::vector<const Function*>::iterator iter_t;
    if (!CTF->isComplete(CS)) 
      return false;

    for (iter_t I = CTF->begin(CS), E = CTF->end(CS); I != E; ++I) {
      if (!isSafeDirectCall(*I)) {
        return false;
      }      
    }

    return true;
  }

NAMESPACE_SC_END
