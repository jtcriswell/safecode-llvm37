//===- CommonMSCInfo.cpp - Common MSC info implementation -----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the default implementation of the MSCInfo interface that
// provides info about the memory safety checks used in LLVM.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "common-msc-info"

#include "CommonMemorySafetyPasses.h"
#include "llvm/Analysis/MSCInfo.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/Pass.h"

using namespace llvm;

namespace {
  /// CommonMSCInfo - This class implements the -common-msc-info pass,
  /// which provides information about the common memory safety checks in LLVM.
  ///
  class CommonMSCInfo : public ImmutablePass, public MSCInfo {
  public:
    static char ID;
    CommonMSCInfo() : ImmutablePass(ID) {
      initializeCommonMSCInfoPass(*PassRegistry::getPassRegistry());
    }

    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.addRequired<MSCInfo>();
    }

    virtual void initializePass();

    /// getAdjustedAnalysisPointer - This method is used when a pass implements
    /// an analysis interface through multiple inheritance.  If needed, it
    /// should override this to adjust the this pointer as needed for the
    /// specified pass info.
    virtual void *getAdjustedAnalysisPointer(const void *ID) {
      if (ID == &MSCInfo::ID)
        return (MSCInfo*)this;
      return this;
    }
  };
} // End of anonymous namespace

// Register this pass
char CommonMSCInfo::ID = 0;
INITIALIZE_AG_PASS(CommonMSCInfo, MSCInfo, "common-msc-info",
                   "Common Memory Safety Check Info", false, true, false)

ImmutablePass *llvm::createCommonMSCInfoPass() {
  return new CommonMSCInfo();
}

void CommonMSCInfo::initializePass() {
  InitializeMSCInfo(this);

  // Add load checks.
  CheckInfoType *FastLoadCheck = new CheckInfoType("__fastloadcheck", NULL,
                                                   CheckInfo::MemoryCheck,
                                                   0, 1, 2, 3, -1, false, true,
                                                   "__fail_fastloadcheck");
  addCheckInfo(FastLoadCheck);
  addCheckInfo(new CheckInfoType("__loadcheck", FastLoadCheck,
                                 CheckInfo::MemoryCheck,
                                 0, 1, -1, -1, -1, false, false, ""));

  // Add store checks.
  CheckInfoType *FastStoreCheck = new CheckInfoType("__faststorecheck", NULL,
                                                    CheckInfo::MemoryCheck,
                                                    0, 1, 2, 3, -1, true, true,
                                                    "__fail_faststorecheck");
  addCheckInfo(FastStoreCheck);
  addCheckInfo(new CheckInfoType("__storecheck", FastStoreCheck,
                                 CheckInfo::MemoryCheck,
                                 0, 1, -1, -1, -1, true, false, ""));

  // Add gep checks.
  CheckInfoType *FastGEPCheck = new CheckInfoType("__fastgepcheck", NULL,
                                                  CheckInfo::GEPCheck,
                                                  0, -1, 2, 3, 1, false, true,
                                                  "");
  addCheckInfo(new CheckInfoType("__gepcheck", FastGEPCheck,
                                 CheckInfo::GEPCheck,
                                 0, -1, -1, -1, 1, false, false, ""));

  // Add global variable registration
  addCheckInfo(new CheckInfoType("__pool_register_global", NULL,
                                 CheckInfo::GlobalRegistration,
                                 -1, -1, 0, 1, -1, false, false, ""));

  // Add stack variable registration and unregistration
  addCheckInfo(new CheckInfoType("__pool_register_stack", NULL,
                                 CheckInfo::StackRegistration,
                                 -1, -1, 0, 1, -1, false, false, ""));
  addCheckInfo(new CheckInfoType("__pool_unregister_stack", NULL,
                                 CheckInfo::StackUnregistration,
                                 -1, -1, 0, -1, -1, false, false, ""));
}
