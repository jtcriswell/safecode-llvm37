//===- InsertChecks.h - Insert run-time checks for SAFECode ------------------//
// 
//                          The SAFECode Compiler 
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file implements several passes that insert run-time checks to enforce
// SAFECode's memory safety guarantees as well as several other passes that
// help to optimize the instrumentation.
//
//===----------------------------------------------------------------------===//

#ifndef _SAFECODE_INSERT_CHECKS_H_
#define _SAFECODE_INSERT_CHECKS_H_

#include "safecode/SAFECode.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/Dominators.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/InstVisitor.h"
#include "safecode/PoolHandles.h"
#include "ArrayBoundsCheck.h"
#include "ConvertUnsafeAllocas.h"
#include "safecode/Intrinsic.h"

#include "SafeDynMemAlloc.h"
#include "poolalloc/PoolAllocate.h"

extern bool isSVAEnabled();

NAMESPACE_SC_BEGIN

//
// Pass: InsertGEPChecks
//
// Description:
//  This pass inserts checks on GEP instructions.
//
struct InsertGEPChecks : public FunctionPass, InstVisitor<InsertGEPChecks> {
  public:
    static char ID;
    InsertGEPChecks () : FunctionPass ((intptr_t) &ID) { }
    const char *getPassName() const { return "Insert GEP Checks"; }
    virtual bool runOnFunction(Function &F);
    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      // Required passes
      AU.addRequired<DataLayout>();

      // Preserved passes
      AU.addPreserved<InsertSCIntrinsic>();
      AU.addPreserved<EQTDDataStructures>();
      AU.addRequired<ArrayBoundsCheckGroup>();
      AU.setPreservesCFG();
    };

    // Visitor methods
    void visitGetElementPtrInst (GetElementPtrInst & GEP);

  protected:
    // Pointers to required passes
    DataLayout * TD;
    ArrayBoundsCheckGroup * abcPass;

    // Pointer to GEP run-time check function
    Function * PoolCheckArrayUI;
};

//
// Pass: AlignmentChecks
//
// Description:
//  This pass inserts alignment checks.  It is only needed when load/store
//  checks on type-consistent memory objects are elided.
//
struct AlignmentChecks : public FunctionPass, InstVisitor<AlignmentChecks> {
  public:
    static char ID;
    AlignmentChecks () : FunctionPass ((intptr_t) &ID) { }
    const char *getPassName() const { return "Insert Alignment Checks"; }
    virtual bool runOnFunction(Function &F);
    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      // Required passes
      AU.addRequired<DataLayout>();
      AU.addRequired<EQTDDataStructures>();

      // Preserved passes
      AU.addPreserved<InsertSCIntrinsic>();
      AU.setPreservesCFG();
    };

    // Visitor methods
    void visitLoadInst  (LoadInst  & LI);

  protected:
    // Pointers to required passes
    DataLayout * TD;
    EQTDDataStructures * dsaPass;

    // Pointers to load/store run-time check functions
    Function * PoolCheckAlign;
    Function * PoolCheckAlignUI;

    // Methods for abstracting away the DSA interface
    DSNodeHandle getDSNodeHandle (const Value * V, const Function * F);
    bool isTypeKnown (const Value * V, const Function * F);
};

struct InsertPoolChecks : public FunctionPass {
  public :
    static char ID;
    InsertPoolChecks () : FunctionPass ((intptr_t) &ID) { }
    const char *getPassName() const { return "Inserting Pool checks Pass"; }
    virtual bool runOnFunction(Function &F);
    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.addRequired<ArrayBoundsCheckGroup>();
      AU.addRequired<DataLayout>();
      AU.addRequired<InsertSCIntrinsic>();
      AU.addRequired<EQTDDataStructures>();

      AU.addPreserved<InsertSCIntrinsic>();
      AU.addPreserved<EQTDDataStructures>();
      AU.setPreservesCFG();
    };

  private:
    InsertSCIntrinsic * intrinsic;
    ArrayBoundsCheckGroup * abcPass;
    DataLayout * TD;
    EQTDDataStructures * dsaPass;

    Function *PoolCheck;
    Function *PoolCheckUI;
    Function *PoolCheckArray;
    Function *PoolCheckArrayUI;
    Function *FunctionCheck;
    void addCheckProto(Module &M);
    void addPoolChecks(Function &F);
    void addGetElementPtrChecks(GetElementPtrInst * GEP);
    void addLoadStoreChecks(Function &F);
    void addLSChecks(Value *Vnew, const Value *V, Instruction *I, Function *F);

    // Methods for abstracting the interface to DSA
    DSNodeHandle getDSNodeHandle (const Value * V, const Function * F);
    DSNode * getDSNode (const Value * V, const Function * F);
    bool isTypeKnown (const Value * V, const Function * F);
    unsigned getDSFlags (const Value * V, const Function * F);
    unsigned getOffset (const Value * V, const Function * F);
};

extern ModulePass * createClearCheckAttributesPass();

NAMESPACE_SC_END
#endif
