//===- LowerSafecodeIntrinsic.h ---------------------------------*- C++ -*----//
// 
//                     The LLVM Compiler Infrastructure
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This pass lowers all intrinsics used by SAFECode to appropriate runtime
// functions.
//
//===----------------------------------------------------------------------===//

#ifndef _LOWER_SAFECODE_INTRINSICS_H_
#define _LOWER_SAFECODE_INTRINSICS_H_

#include "llvm/Pass.h"
#include "llvm/IR/Instructions.h"

#include "safecode/Config/config.h"

#include <vector>

namespace llvm {

  struct LowerSafecodeIntrinsic : public ModulePass {
  public:

    typedef struct IntrinsicMappingEntry {
      const char * intrinsicName;
      const char * functionName;
    } IntrinsicMappingEntry;

    static char ID;

    template<class Iterator>
      LowerSafecodeIntrinsic(Iterator begin, Iterator end) : ModulePass(ID) {
        for(Iterator it = begin; it != end; ++it) {
          mReplaceList.push_back(*it);
      }
     }
    
    LowerSafecodeIntrinsic() : ModulePass(ID), mReplaceList() {}

    virtual ~LowerSafecodeIntrinsic() {};
    virtual bool runOnModule(Module & M);
    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.setPreservesCFG();
    }

  private:
    std::vector<IntrinsicMappingEntry> mReplaceList;
  };

}

#endif
