//===- NoMSCInfo.cpp - Minimal Memory Safety Check Info implementation ----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the default implementation of the MSCInfo interface that
// provides no information about any memory safety checks by itself. Other
// memory safety check info passes provide the necessary information to this
// pass to make it easy to find the corresponding check for a function or
// iterate over all known memory safety checks types.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "no-msc-info"

#include "CommonMemorySafetyPasses.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Analysis/MSCInfo.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/Pass.h"

using namespace llvm;

namespace {
  /// NoMSCInfo - This class implements the -no-msc-info pass, which makes it
  /// appear as if there were no memory safety checks. NoMSCInfo is unlike other
  /// memory safety check info implementations, in that it does not chain to a
  /// previous analysis. As such it doesn't follow many of the rules that other
  /// memory safety check info analyses must.
  ///
  class NoMSCInfo : public ImmutablePass, public MSCInfo {
    StringMap <CheckInfoType*> CheckData;

  protected:
    virtual void addCheckInfo(const CheckInfo *CI) {
      CheckData[CI->Name] = CI;
    }

  public:
    static char ID;
    NoMSCInfo() : ImmutablePass(ID), MSCInfo() {
      initializeNoMSCInfoPass(*PassRegistry::getPassRegistry());
    }

    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    }

    virtual void initializePass() {
      // Note: NoMSCInfo does not call InitializeMSCInfo because it's
      // special and does not support chaining.
    }

    virtual CheckInfoListType getCheckInfoList() const;
    virtual CheckInfoType* getCheckInfo(Function *F) const;

    /// getAdjustedAnalysisPointer - This method is used when a pass implements
    /// an analysis interface through multiple inheritance.  If needed, it
    /// should override this to adjust the this pointer as needed for the
    /// specified pass info.
    virtual void *getAdjustedAnalysisPointer(const void *ID) {
      if (ID == &MSCInfo::ID)
        return (MSCInfo*)this;
      return this;
    }

    virtual ~NoMSCInfo();
  };
} // End of anonymous namespace

// Register this pass
char NoMSCInfo::ID = 0;
INITIALIZE_AG_PASS(NoMSCInfo, MSCInfo, "no-msc-info",
                   "No Memory Safety Check Info", true, true, true)

ImmutablePass* llvm::createNoMSCInfoPass() {
  return new NoMSCInfo();
}

CheckInfoListType NoMSCInfo::getCheckInfoList() const {
  CheckInfoListType List;

  for (StringMap <CheckInfoType*>::const_iterator I = CheckData.begin(),
         E = CheckData.end();
       I != E;
       ++I) {
    List.push_back(I->second);
  }

  return List;
}

CheckInfoType* NoMSCInfo::getCheckInfo(Function *F) const {
  if (!F || !F->hasName())
    return NULL;

  StringMap <CheckInfoType*>::const_iterator It = CheckData.find(F->getName());
  if (It != CheckData.end())
    return It->second;
  return NULL;
}

NoMSCInfo::~NoMSCInfo() {
  for (StringMap <CheckInfoType*>::const_iterator I = CheckData.begin(),
         E = CheckData.end();
       I != E;
       ++I) {
    delete I->second;
  }
  CheckData.clear();
}
