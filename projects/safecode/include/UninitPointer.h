//===- UninitPointer.h  - ---------------------------------*- C++ -*---------=//
// 
//                          The SAFECode Compiler 
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file defines a set of utilities for CZero checks on pointers and
// dynamic memory.
// 
//===----------------------------------------------------------------------===//

#ifndef LLVM_CZERO_H
#define LLVM_CZERO_H

using namespace llvm;

#include "llvm/Pass.h"


FunctionPass* createCZeroUninitPtrPass();
// FunctionPass* createCZeroLivePtrs();

#endif
