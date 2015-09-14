//===- FaultInjector.h - Insert faults into programs -------------------------//
// 
//                          The SAFECode Compiler 
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This pass transforms injects faults into a program.
//
//===----------------------------------------------------------------------===//

#ifndef FAULT_INJECTOR_H
#define FAULT_INJECTOR_H

#include "safecode/Config/config.h"
#include "dsa/DataStructure.h"
#include "dsa/DSGraph.h"
#include "llvm/Pass.h"

namespace llvm {

  struct FaultInjector : public ModulePass {
    public :
      static char ID;
      FaultInjector () : ModulePass ((intptr_t) &ID) {
        return;
      }

      const char *getPassName() const { return "Fault Injector Pass"; }
      virtual bool runOnModule(Module &M);
      virtual void getAnalysisUsage(AnalysisUsage &AU) const {
        AU.addRequired<DataLayout>();
        AU.addRequired<TDDataStructures>();
      };

    private :
      // Private variables
      DataLayout        * TD;
      TDDataStructures  * TDPass;
      Function          * Free;

      // Private methods
      bool insertEasyDanglingPointers (Function & F);
      bool insertHardDanglingPointers (Function & F);
      bool insertRealDanglingPointers (Function & F);
      bool insertBadAllocationSizes   (Function & F);
      bool insertBadIndexing          (Function & F);
      bool insertUninitializedUse     (Function & F);
  };
}
#endif
