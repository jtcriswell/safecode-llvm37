//===- StackSafety.h - ------------------------------------*- C++ -*---------=//
// 
//                          The SAFECode Compiler 
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file defines checks for stack safety.
// 
//===----------------------------------------------------------------------===//

#ifndef LLVM_STACKSAFETY_H
#define LLVM_STACKSAFETY_H

#include "safecode/PoolHandles.h"
#include "dsa/DataStructure.h"
#include "dsa/DSGraph.h"
#include "dsa/DSNode.h"
#include "llvm/Pass.h"

#include <set>

namespace llvm {

  ModulePass* createStackSafetyPass();
  
  namespace CSS {
    
    struct checkStackSafety : public ModulePass {
      
    public :
      static char ID;
      checkStackSafety() : ModulePass ((intptr_t) &ID) {}
      std::set<DSNode *> AllocaNodes;
      const char *getPassName() const { return "Stack Safety Check";}
      virtual bool runOnModule(Module &M);
      virtual void getAnalysisUsage(AnalysisUsage &AU) const {
        AU.addRequired<DataLayout>();
        AU.addRequired<EQTDDataStructures>();
        AU.setPreservesAll();
      }

      //
      // Method: releaseMemory()
      //
      // Description:
      //  This method frees memory used by the pass; it should be called by the
      //  PassManager before the pass's analysis results are invalidated.
      //
      virtual void releaseMemory () {
        // Clear the set of nodes
        AllocaNodes.clear();

        // Clear the set of reachable nodes; this may have items from the last
        // execution lying around.
        reachableAllocaNodes.clear();
      }

    private :
      //
      // Tracks the DSNodes that have already been analyzed by an invocation of
      // markReachableAllocas().
      //
      std::set<DSNode *> reachableAllocaNodes; 
      bool markReachableAllocas(DSNode *DSN, bool start=false);
      bool markReachableAllocasInt(DSNode *DSN, bool start=false);
    };
  }
}
#endif
