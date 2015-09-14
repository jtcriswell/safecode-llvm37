//===- LowerSafecodeIntrinsic.cpp: ----------------------------------------===//
//
//                          The SAFECode Compiler
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass lowers all intrinsics added by SAFECode to appropriate calls to
// run-time functions in the run-time implementation.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/Module.h"

#include "safecode/LowerSafecodeIntrinsic.h"

using namespace llvm;

char LowerSafecodeIntrinsic::ID = 0;

static RegisterPass<LowerSafecodeIntrinsic> passReplaceFunction 
("lower-sc-intrinsic", "Replace all uses of a function to another");

namespace llvm {

  ////////////////////////////////////////////////////////////////////////////
  // LowerSafecodeIntrinsic Methods
  ////////////////////////////////////////////////////////////////////////////

  bool
  LowerSafecodeIntrinsic::runOnModule(Module & M) {
    std::vector<IntrinsicMappingEntry>::const_iterator it=mReplaceList.begin();
    std::vector<IntrinsicMappingEntry>::const_iterator end=mReplaceList.end();
    for (; it != end; ++it) {
      //
      // Get a reference to the original function (if it exists).
      //
      Function * origF = M.getFunction(it->intrinsicName);

      //
      // If the new function has a name different from the old function, create
      // a function prototype of the new function and replace uses of the old
      // function with it.
      //
      if (origF) {
        Constant * newF = M.getOrInsertFunction (it->functionName,
                                                 origF->getFunctionType());
        origF->replaceAllUsesWith(newF);
        origF->eraseFromParent();
      }
    }   
    return true;
  }
}
