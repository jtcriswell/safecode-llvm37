//===------------ CStdLib.h - Secure C standard library calls -------------===//
// 
//                          The SAFECode Compiler 
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This pass finds all calls to functions in the C standard library and
// transforms them to a more secure form.
//
//===----------------------------------------------------------------------===//

#ifndef CSTDLIB_H
#define CSTDLIB_H

#include "llvm/IR/CallSite.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/Pass.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/Support/Debug.h"

#include "safecode/SAFECode.h"

#include <vector>

using std::vector;

namespace llvm
{
  /**
   * Pass that secures C standard library string calls via transforms
   */
  class StringTransform : public ModulePass
  {
  private:

    typedef struct
    {
      const char *name;
      Type *return_type;
      unsigned argc;
    } SourceFunction;

    typedef struct
    {
      const char *name;
      unsigned source_argc;
      unsigned pool_argc;
    } DestFunction;

    bool transform(Module &M,
                   const StringRef FunctionName,
                   const unsigned argc,
                   const unsigned pool_argc,
                   Type *ReturnTy,
                   Statistic &statistic);

    bool vtransform(Module &M,
                    const SourceFunction &from,
                    const DestFunction &to,
                    Statistic &stat,
                    ...);

    bool gtransform(Module &M,
                    const SourceFunction &from,
                    const DestFunction &to,
                    Statistic &stat,
                    const vector<unsigned> &append_order);

    const DataLayout *tdata;

  public:
    static char ID;
    StringTransform() : ModulePass(ID) {}
    virtual bool runOnModule(Module &M);

    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    }
  };

}

#endif
