//===- ArrayBoundCheckDummy.cpp -  -*- C++ -*---------------------------------//
// 
//                          The SAFECode Compiler 
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This pass is the basic version of static array bounds checking.
// It assumes that every GEP instruction is unsafe.
//
//===----------------------------------------------------------------------===//

#include "safecode/ArrayBoundsCheck.h"

namespace llvm {

char ArrayBoundsCheckGroup::ID = 0;
char ArrayBoundsCheckDummy::ID = 0;

static RegisterPass<ArrayBoundsCheckDummy>
X ("abc-none", "Dummy Array Bounds Check pass");

static RegisterAnalysisGroup<ArrayBoundsCheckGroup, true>
ABCGroup (X);

ArrayBoundsCheckGroup::~ArrayBoundsCheckGroup() {}

}
