//===- SafeDynMemAlloc.h  - CZero passes  -----------------*- C++ -*---------=//
// 
//                          The SAFECode Compiler 
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file defines a set of utilities for EmbeC checks on pointers and
// dynamic memory.
// 
// FIXME:
//  This pass is currently disabled and a subset of its functinality moved to
//  the Check Insertion Pass.  However, removing it from the sc tool seems to
//  cause the other SAFECode passes grief.  Therefore, this pass is left in
//  place, but it does nothing.
//
// FIXME:
//  Note that this pass preserves all other passes.  This must be left intact;
//  otherwise, it will invalidate the pool allocation results and cause
//  SAFECode to erronously re-execute the pool allocation pass.
//
// This file defines the interface to the EmbeCFreeRemoval pass.  This pass
// appears to do two things:
//
//  o) It ensures that there are load/store checks on pointers that point to
//     type-known data but are loaded from type-unknown partitions.
//
//  o) It seems to perform some sort of sanity/correctness checking of pool
//     creation/destruction.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_EMBEC_H
#define LLVM_EMBEC_H


#include "dsa/DataStructure.h"
#include "dsa/DSGraph.h"

#include "safecode/Config/config.h"

#include "llvm/Pass.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Constants.h"
#include "llvm/Support/CFG.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Support/Debug.h"

#include "poolalloc/PoolAllocate.h"

#include <set>
#include <map>
#include <string>

using std::set;
using std::map;

using namespace llvm;
using namespace PA;
Pass* createEmbeCFreeRemovalPass();

namespace {
  static const std::string PoolI = "poolinit";
  static const std::string PoolA = "poolalloc";
  static const std::string PoolF = "poolfree";
  static const std::string PoolD = "pooldestroy";
  static const std::string PoolMUF = "poolmakeunfreeable";
  static const std::string PoolCh = "poolcheck";
  static const std::string PoolAA = "poolregister";
}

namespace llvm {

  struct EmbeCFreeRemoval : public ModulePass {
    
    // The function representing 'poolmakeunfreeable'
    Constant *PoolMakeUnfreeable;

    Constant *PoolCheck;

    bool runOnModule(Module &M);
    std::vector<Value *> Visited;

    static char ID;
    EmbeCFreeRemoval () : ModulePass ((intptr_t) &ID) {}
    const char *getPassName() const { return "Embedded C Free Removal"; }
    void checkPoolSSAVarUses(Function *F, Value *V, 
                             map<Value *, set<Instruction *> > &FuncAllocs, 
                             map<Value *, set<Instruction *> > &FuncFrees, 
                             map<Value *, set<Instruction *> > &FuncDestroy);

    void propagateCollapsedInfo(Function *F, Value *V);
    DSNode *guessDSNode(Value *v, DSGraph &G, PA::FuncInfo *PAFI);
    void guessPoolPtrAndInsertCheck(PA::FuncInfo *PAFI, Value *oldI, Instruction  *I, Value *pOpI, DSGraph &oldG);
      
    void insertNonCollapsedChecks(Function *Forig, Function *F, DSNode *DSN);

    void addRuntimeChecks(Function *F, Function *Forig);
    
    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.addRequired<PoolAllocateGroup>();
      AU.addPreserved<PoolAllocateGroup>();
      AU.addRequired<CallGraph>();
      AU.setPreservesAll();
    }

    // Maps from a function to a set of Pool pointers and DSNodes from the 
    // original function corresponding to collapsed pools.
    map <Function *, set<Value *> > CollapsedPoolPtrs;

    
  private:
    
    Module *CurModule;

    TDDataStructures *TDDS;
    PoolAllocateGroup *PoolInfo;
    bool moduleChanged;
    bool hasError;
    
    // The following maps are only for pool pointers that escape a function.
    // Associates function with set of pools that are freed or alloc'ed using 
    // pool_free or pool_alloc but not destroyed within the function.
    // These have to be pool pointer arguments to the function
    map<Function *, set<Value *> > FuncFreedPools;
    map<Function *, set<Value *> > FuncAllocedPools;
    map<Function *, set<Value *> > FuncDestroyedPools;

  };  
}

#endif
