//===- SAFECodePasses.h - Functions to create SAFECode Passes ----------------//
// 
//                     The LLVM Compiler Infrastructure
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// Define functions that can create SAFECode pass objects.
//
//===----------------------------------------------------------------------===//

#include "llvm/Pass.h"

namespace llvm {
  extern ModulePass * createSCTerminatePass (void);
}

//
// Borrow this techique from LLVM.  We basically create a global object with a
// constructor that calls all of the pass creation functions.  This, in turn,
// forces all of these functions to be linked in.  The key is the call to
// getenv(); that call prevents the compiler from removing the calls.
//
namespace {
  struct ForcePassLinking {
    ForcePassLinking() {
      // We must reference the passes in such a way that compilers will not
      // delete it all as dead code, even with whole program optimization,
      // yet is effectively a NO-OP. As the compiler isn't smart enough
      // to know that getenv() never returns -1, this will do the job.
      if (std::getenv("bar") != (char*) -1)
        return;

      llvm::createSCTerminatePass();
      return;
    }
  };
}

