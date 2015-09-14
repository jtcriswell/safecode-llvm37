//===- IndirectCallChecks.h - Insert Indirect Function Call Checks -----------//
// 
//                          The SAFECode Compiler 
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//

#ifndef _INDIRECT_CALL_CHECKS_H_
#define _INDIRECT_CALL_CHECKS_H_

#include "safecode/Config/config.h"
#include "llvm/Pass.h"

namespace llvm {
    ModulePass *createIndirectCallChecksPass();
}

#endif
