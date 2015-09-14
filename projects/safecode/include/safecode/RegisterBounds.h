//===- InsertChecks/RegisterBounds.h ----------------------------*- C++ -*----//
// 
//                     The LLVM Compiler Infrastructure
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// Various passes to register the bound information of variables into the pools
//
//===----------------------------------------------------------------------===//

#ifndef _REGISTER_BOUNDS_H_
#define _REGISTER_BOUNDS_H_

#include "llvm/Analysis/DominanceFrontier.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "safecode/AllocatorInfo.h"

namespace llvm {

/// Base class of all passes which register variables into pools.
class RegisterVariables : public llvm::ModulePass {
public:
  RegisterVariables(char & id) : llvm::ModulePass(id) {}
  virtual ~RegisterVariables();
  virtual bool runOnModule(llvm::Module & M) = 0;
protected:
  //
  // Method: init()
  //
  // Description:
  //  This method performs some initialization that is common to all subclasses
  //  of this pass.
  //
  void init(Module & M, std::string registerName);

  /// Helper function to register the bound information of a variable into a
  /// particular pool.
  void RegisterVariableIntoPool(llvm::Value * PH, llvm::Value * val, llvm::Value * AllocSize, llvm::Instruction * InsertBefore);

  /// Helper function to create the body of sc.register_globals /
  /// sc.register_main. It inserts an empty basicblock and a ret void
  /// instruction into the function.
  ///
  /// Return the last instruction of the function body.
  llvm::Instruction * CreateRegistrationFunction(llvm::Function * F);

  Function * PoolRegisterFunc;

};

/// Register the bound information of global variables.
/// All registeration are placed at sc.register_globals
class RegisterGlobalVariables : public RegisterVariables {
public:
  static char ID;
  RegisterGlobalVariables() : RegisterVariables(ID) {}
  const char * getPassName() const 
  { return "Register Global Variables into Pools"; }

  virtual bool runOnModule(llvm::Module & M);

  virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const {
    AU.setPreservesCFG();
  }


private:
  // Other passes which we query
  const DataLayout * TD;

  // Private methods
  void registerGV(GlobalVariable * GV, Instruction * InsertBefore);
};

/// Register the bound information of argv[] in main().
/// All registeration are placed at sc.register_main_args
class RegisterMainArgs : public RegisterVariables {
public:
  static char ID;
  const char * getPassName() const { return "Register argv[] into Pools";}
  RegisterMainArgs() : RegisterVariables(ID) {}
  virtual bool runOnModule(llvm::Module & M);
  virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const {
    AU.setPreservesCFG();
  }
};

/// Register the bound information of custom allocators
/// such as kmem_cache_alloc.
///
/// FIXME: Haohui
/// Ideally, the pass should be organized as a FunctionPass. It should ask other
/// analysis passes for all allocation sites, and register them.
/// Now I hard-coded the allocation inside the pass since it is only used by SVA
/// kernel, and DSA does not have the functionality to point out all allocation
/// site yet.
///
/// Now the pass scan through all uses of customized allocators and add
/// registration right after them.

class AllocatorInfo;

class RegisterCustomizedAllocation : public RegisterVariables {
public:
  static char ID;
  const char * getPassName() const 
  { return "Register customized allocations into Pools";}
  RegisterCustomizedAllocation() : RegisterVariables(ID) {}
  virtual bool runOnModule(llvm::Module & M);
  virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const {
    AU.addRequired<AllocatorInfoPass>();
    AU.setPreservesCFG();
  }

private:
  Function * PoolReregisterFunc;
  Function * PoolUnregisterFunc;
  void registerAllocationSite(llvm::CallInst * AllocSite, AllocatorInfo * info);
  void registerReallocationSite(llvm::CallInst * AllocSite, ReAllocatorInfo * info);
  void registerFreeSite(llvm::CallInst * FreeSite, AllocatorInfo * info);
  void proceedAllocator(llvm::Module * M, AllocatorInfo * info);
  void proceedReallocator(llvm::Module * M, ReAllocatorInfo * info);
};

// Pass to register byval arguments
class RegisterFunctionByvalArguments : public RegisterVariables {
public:
  static char ID;
  const char * getPassName() const { return "Register byval arguments of functions";}
  RegisterFunctionByvalArguments() : RegisterVariables(ID) {}
  virtual bool runOnModule(llvm::Module & M);
  virtual bool runOnFunction(llvm::Function & F);
  virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const {
    AU.setPreservesCFG();
  }
private:
  const DataLayout * TD;
  Function * StackFree;
};

struct RegisterStackObjPass : public FunctionPass {
  public:
    static char ID;
    RegisterStackObjPass() : FunctionPass(ID) {};
    virtual ~RegisterStackObjPass() {};
    virtual bool runOnFunction(Function &F);
    virtual const char * getPassName() const {
      return "Register stack variables into pool";
    }
    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.addRequired<LoopInfoWrapperPass>();
      AU.addRequired<DominatorTreeWrapperPass>();
      AU.addRequired<DominanceFrontier>();
    };

  private:
    // References to other LLVM passes
    const DataLayout * TD;
    LoopInfo * LI;
    DominatorTree * DT;
    DominanceFrontier * DF;

    // The pool registration function
    Constant *PoolRegister;

    CallInst * registerAllocaInst(AllocaInst *AI);
    void insertPoolFrees (const std::vector<CallInst *> & PoolRegisters,
                          const std::vector<Instruction *> & ExitPoints,
                          LLVMContext * Context);
};

}

#endif
