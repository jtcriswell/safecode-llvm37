//===- BottomUpCallGraph.cpp - -----------------------------------------------//
// 
//                          The SAFECode Compiler 
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//

#ifndef BOTTOMUP_CALLGRAPH_H
#define BOTTOMUP_CALLGRAPH_H

#include "dsa/DataStructure.h"
#include "dsa/DSSupport.h"
#include "llvm/Function.h"
#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "safecode/ADT/HashExtras.h"

#include <set> 

namespace llvm {
  struct BottomUpCallGraph : public ModulePass {
    private:
      // OneCalledFunction - For each indirect function call, we keep track of
      // one the DSNode and the corresponding Call Instruction
      typedef hash_multimap<DSNode*, CallSite> CalleeNodeCallSiteMapTy;
      CalleeNodeCallSiteMapTy CalleeNodeCallSiteMap ;

      // Containers used in finding SCCs
      std::vector<Function *> Stack;
      std::set<Function *> Visited;

      // A set of functions involved in SCCs
      std::set<Function *> SccList;

      void figureOutSCCs(Module &M);
      void visit(Function *f);

    public:
      static char ID;
      BottomUpCallGraph () : ModulePass ((intptr_t) &ID) {}
      const char *getPassName() const { return "Bottom-Up Call Graph"; }

      // This keeps the map of a function and its call sites in all the callers
      // including the indirectly called sites
      std::map<const Function *, std::vector<CallSite> > FuncCallSiteMap;

      bool isInSCC(Function *f) {
        return (SccList.find(f) != SccList.end());
      }

      virtual void getAnalysisUsage(AnalysisUsage &AU) const {
        AU.addRequiredTransitive<EQTDDataStructures>();
        AU.setPreservesAll(); 
      }
      virtual bool runOnModule(Module&M);

      //
      // Method: releaseMemory()
      //
      // Description:
      //  Free memory that is used by this pass.  This method should be called
      //  by the PassManager before the pass's analysis results are
      //  invalidated.
      //
      virtual void releaseMemory() {
        CalleeNodeCallSiteMap.clear();
        Stack.clear();
        Visited.clear();
        SccList.clear();
      }
  };
}
#endif
