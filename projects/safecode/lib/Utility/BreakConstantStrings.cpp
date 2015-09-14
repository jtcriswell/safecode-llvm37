//===- BreakConstantStrings.cpp - Make global string constants non-constant - //
// 
//                          The SAFECode Compiler 
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This pass removes the constant attribute from all global strings.  This is
// done so that the native system linker does not link the strings into the
// same global string.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "break-conststrings"

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"

#include "safecode/BreakConstantStrings.h"

#include <iostream>
#include <map>
#include <utility>

namespace llvm {

// Identifier variable for the pass
char BreakConstantStrings::ID = 0;

// Statistics
STATISTIC (GVChanges,   "Number of Strings Made Non-Constant");

// Register the pass
static RegisterPass<BreakConstantStrings> P ("break-conststrings",
                                             "Make strings non-constant");

//
// Method: runOnModule()
//
// Description:
//  Entry point for this LLVM pass.
//
// Return value:
//  true  - The module was modified.
//  false - The module was not modified.
//
bool
BreakConstantStrings::runOnModule (Module & M) {
  bool modified = false;
  const Type * Int8Type  = IntegerType::getInt8Ty(getGlobalContext());

  //
  // Scan through all the global variables in the module.  Mark a variable as
  // non-constant if:
  //  o) The variable is constant
  //  o) The variable is an array of characters (Int8Ty).
  //  o) The variable is not in a special section (e.g. debug info section).
  //     This ensures that we don't mess up debug information or other special
  //     strings within the code.
  //
  Module::global_iterator i,e;
  for (i = M.global_begin(), e = M.global_end(); i != e; ++i) {
    GlobalVariable * GV = i;

    //
    // All global variables are pointer types.  Find the type of what the
    // global variable pointer is pointing at.
    //
    if (GV->isConstant() && (!GV->hasSection())) {
      const PointerType * PT = dyn_cast<PointerType>(GV->getType());
      if (const ArrayType * AT = dyn_cast<ArrayType>(PT->getElementType())) {
        if (AT->getElementType() == Int8Type) {
          modified = true;
          ++GVChanges;
          GV->setConstant (false);
        }
      }
    }
  }

  return modified;
}

}

