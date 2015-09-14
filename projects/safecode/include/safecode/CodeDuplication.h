/// This header file defines the analysis and transformation parts of
/// code duplication stuffs.

#ifndef _CODE_DUPLICATION_H_
#define _CODE_DUPLICATION_H_

#include <map>
#include "llvm/Pass.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/LoopPass.h"

namespace llvm {

  /// This module analyzes the side effects of codes to see:
  ///
  /// 1. Whether we can duplicate the codes.
  /// 2. What parameters are needed to duplicate the codes.
  ///
  struct CodeDuplicationAnalysis : public ModulePass {
  public:
    static char ID;
  CodeDuplicationAnalysis() : ModulePass((intptr_t) &ID) {};
    void getAnalysisUsage(AnalysisUsage & AU) const {
      AU.setPreservesAll();
      AU.setPreservesCFG();
    };
    virtual bool doInitialization(Module & M);
    virtual bool doFinalization(Module & M);
    virtual const char * getPassName() const { return "Code Duplication Analysis"; };
    virtual ~CodeDuplicationAnalysis() {};
    virtual bool runOnModule(Module & m);
    /// Arguments required to turn a basic block to "pure" basic block
    typedef SmallVector<Instruction *, 8> InputArgumentsTy;
    /// FIXME: Make an iterator interfaces
    typedef std::map<BasicBlock*, InputArgumentsTy> BlockInfoTy;
    const BlockInfoTy & getBlockInfo() const { return mBlockInfo; }
  private:
    BlockInfoTy mBlockInfo;
    void calculateBBArgument(BasicBlock * BB, InputArgumentsTy & args);
  };

  /// Remove all self-loop edges from every basic blocks
  struct RemoveSelfLoopEdge : public FunctionPass {
  public:
    static char ID;
  RemoveSelfLoopEdge() : FunctionPass((intptr_t) & ID) {};
    const char * getPassName() const { return "Remove all self-loop edges from every basic block"; };
    virtual bool runOnFunction(Function & F);
    virtual ~RemoveSelfLoopEdge() {};
  };

  struct DuplicateCodeTransform : public ModulePass {
  public:
  DuplicateCodeTransform(): ModulePass((intptr_t) & ID) {};
    static char ID;
    virtual ~DuplicateCodeTransform() {};
    const char * getPassName() const { return "Duplicate codes for SAFECode checking"; };
    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.addRequired<CodeDuplicationAnalysis>();
    }
    virtual bool runOnModule(Module &M);

  private:
    void wrapCheckingRegionAsFunction(Module & M, const BasicBlock * bb, 
                                      const CodeDuplicationAnalysis::InputArgumentsTy & args);
  };


  /**
   *
   * Analyze all loops to find all the loops that are eligible for code duplication. 
   * It also clones eligible loop
   *
   * HACK: the transformation pass is a module pass but it requires
   * the information from the analysis pass. Currently I have to make
   * things static in order to perverse the information. Should be refactored.
   *
   **/
  struct DuplicateLoopAnalysis : public FunctionPass {
  DuplicateLoopAnalysis() : FunctionPass((intptr_t) & ID) {}
    virtual ~DuplicateLoopAnalysis() {}
    virtual const char * getPassName() const { return "Find loops eligible for code duplication"; }
    virtual bool doInitialization(Module & M); 
    virtual bool runOnFunction(Function &F);

    virtual void getAnalysisUsage(AnalysisUsage & AU) const {
      AU.addRequired<LoopInfo>();
      AU.setPreservesAll();
      AU.setPreservesCFG();
    }

    typedef std::vector<Value *> InputArgumentsTy;

    InputArgumentsTy dupLoopArgument;
    DenseMap<const Value *, Value*> cloneValueMap;
    Loop * clonedLoop;

    static char ID;

  private:
    LoopInfo * LI;
    std::set<Function *> cloneFunction;
  
    /**
     * Try to duplicate loops in a prefix order
     **/
    void duplicateLoop(Loop * L, Module * M);
    /**
     * Calculate arguments of a particular loop
     **/
    void calculateArgument(const Loop * L);

    /**
     * Check whether a loop is eligible for duplication
     *
     * Here are the sufficient conditions:
     * 
     *  1. All stores in the loop are type-safe.
     *  2. It only calls readonly functions
     *
     **/
    bool isEligibleforDuplication(const Loop * L) const;

    /**
     *
     * Transform a loop into a duplicated one with all the checks
     * It will do the following:
     *  1. Clone the loop and wrap it into a function
     *  2. Replace all uses in the new functions with proper arguments
     *  3. Clean up the original loop and add calls for parallel checkings
     **/

    /**
     * Clone the loop and wrap it into a function
     **/
    Function * wrapLoopIntoFunction(Loop  * L, Module * M);

    void insertCheckingCallInLoop(Loop* L, Function * checkingFunction, StructType * checkArgumentType, Module * M);
    void replaceIntrinsic(Loop * L, Module * M);
  };
}

#endif
