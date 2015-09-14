//===- BreakConstantStrings.h - Make global string constants non-constant - --//
// 
//                          The SAFECode Compiler 
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This pass changes all GEP constant expressions into GEP instructions.  This
// permits the rest of SAFECode to put run-time checks on them if necessary.
//
//===----------------------------------------------------------------------===//

#ifndef BREAKCONSTANTSTRINGS_H
#define BREAKCONSTANTSTRINGS_H

#include "llvm/IR/Dominators.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

#include "safecode/SAFECode.h"

namespace llvm { 

  //
  // Pass: BreakConstantStrings
  //
  // Description:
  //  This pass modifies an LLVM Module so that strings are not constant.
  //
  struct BreakConstantStrings : public ModulePass {
    private:
      // Private methods

      // Private variables

    public:
      static char ID;
      BreakConstantStrings() : ModulePass(ID) {}
      const char *getPassName() const {return "Make Global Strings Non-constant";}
      virtual bool runOnModule (Module & M);
      virtual void getAnalysisUsage(AnalysisUsage &AU) const {
        // This pass does not modify the control-flow graph of the function
        AU.setPreservesCFG();
      }
  };

}

#endif
