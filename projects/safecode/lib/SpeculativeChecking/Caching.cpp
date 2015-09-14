//===- Caching.cpp: -------------------------------------------------------===//
//
//                          The SAFECode Compiler
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//

#include <set>
#include <map>
#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Instructions.h"

using namespace llvm;

namespace {
  class PoolCaching : public ModulePass {
    std::map<Value*, Value*> PoolSources;
    std::set<Value*> Pools;

    void findPools(Module& M) {
      for (Module::iterator MI = M.begin(), ME = M.end(); MI != ME; ++MI)
        if (!MI->isDeclaration())
          for (Function::iterator FI = MI->begin(), FE = MI->end(); FI != FE; ++FI)
            for (BasicBlock::iterator BI = FI->begin(), BE = FI->end(); BI != BE; ++BI)
              if (CallInst* CI = dyn_cast<CallInst>(BI))
                if (Function* F = dyn_cast<Function>(CI->getOperand(0))) {
                  if (F->getName() == "__sc_par_poolcheck") {
                    Pools.insert(CI->getOperand(1));
                  } else if (F->getName() == "__sc_par_boundscheck") {
                    Pools.insert(CI->getOperand(1));
                  } else if (F->getName() == "__sc_par_poolinit") {
                    Pools.insert(CI->getOperand(1));
                  }
                }
    }

    void findPoolSources() {
      unsigned numPools;
      do {
        numPools = Pools.size();
        for (std::set<Value*>::iterator ii = Pools.begin(); ii != Pools.end(); ++ii) {
          (*ii)->dump();
        }
      } while (numPools != Pools.size());
    }

  public:
    static char ID;
    PoolCaching() : ModulePass((intptr_t)&ID) {}
    bool runOnModule(Module& M) {
      findPools(M);
      findPoolSources();
      return true;
    }
  };
}

char PoolCaching::ID = 0;

static RegisterPass<PoolCaching> X 
("sc-par-poolcache", "Use Checking Thread caches");
