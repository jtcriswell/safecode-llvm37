//===- MSCInfo.cpp - Memory safety check info -----------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the generic MSCInfo interface, which is used as the
// common interface for identifying memory safety checks.
//
//===----------------------------------------------------------------------===//

#include "CommonMemorySafetyPasses.h"
#include "llvm/Analysis/MSCInfo.h"

using namespace llvm;

// Register the MSCInfo interface, providing a nice name to refer to.
INITIALIZE_ANALYSIS_GROUP(MSCInfo, "Memory Safety Check Info", NoMSCInfo)
char MSCInfo::ID = 0;

//===----------------------------------------------------------------------===//
// Default chaining methods
//===----------------------------------------------------------------------===//

void MSCInfo::addCheckInfo(const CheckInfo *CI) {
  assert(MSCI && "InitializeMSCInfo was not called in the run method!");
  return MSCI->addCheckInfo(CI);
}

CheckInfoListType MSCInfo::getCheckInfoList() const {
  assert(MSCI && "InitializeMSCInfo was not called in the run method!");
  return MSCI->getCheckInfoList();
}

CheckInfoType* MSCInfo::getCheckInfo(Function *F) const {
  assert(MSCI && "InitializeMSCInfo was not called in the run method!");
  return MSCI->getCheckInfo(F);
}

// MSCInfo destructor: DO NOT move this to the header file for MSCInfo or else
// clients of the MSCInfo class may not depend on the MSCInfo.o file in the
// current .a file, causing memory safety check info analysis to not be included
// in the tool correctly!
//
MSCInfo::~MSCInfo() {}

void MSCInfo::InitializeMSCInfo(Pass *P) {
  MSCI = &P->getAnalysis<MSCInfo>();
}

// getAnalysisUsage - All memory safety check info implementations should
// invoke this directly (using MSCInfo::getAnalysisUsage(AU)).
void MSCInfo::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<MSCInfo>(); // All MSCInfo passes chain
}
