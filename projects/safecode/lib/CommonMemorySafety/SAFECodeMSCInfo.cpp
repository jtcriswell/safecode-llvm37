//===- SAFECodeMSCInfo.cpp - SAFECode memory safety check info provider ---===//
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

#define DEBUG_TYPE "safecode-msc-info"

#include "CommonMemorySafetyPasses.h"
#include "safecode/SAFECodeMSCInfo.h"
#include "llvm/Analysis/MSCInfo.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/Pass.h"

using namespace llvm;

namespace {
  /// SAFECodeMSCInfo - This class implements the -safecode-msc-info pass,
  /// which provides information about the memory safety checks in SAFECode.
  ///
  class SAFECodeMSCInfo : public ImmutablePass, public MSCInfo {
  public:
    static char ID;
    SAFECodeMSCInfo() : ImmutablePass(ID) {
      initializeSAFECodeMSCInfoPass(*PassRegistry::getPassRegistry());
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
char SAFECodeMSCInfo::ID = 0;
INITIALIZE_AG_PASS(SAFECodeMSCInfo, MSCInfo, "safecode-msc-info",
                   "SAFECode Memory Safety Check Info", false, true, false)

ImmutablePass *llvm::createSAFECodeMSCInfoPass() {
  return new SAFECodeMSCInfo();
}

void SAFECodeMSCInfo::initializePass() {
  InitializeMSCInfo(this);

  // Add load/store checks.
  CheckInfoType *FastLSCheck = new CheckInfoType("fastlscheck", NULL,
                                                 CheckInfo::MemoryCheck,
                                                 1, 3, 0, 2, -1, false, true,
                                                 "");
  addCheckInfo(FastLSCheck);
  addCheckInfo(new CheckInfoType("poolcheck", FastLSCheck,
                                 CheckInfo::MemoryCheck,
                                 1, 2, -1, -1, -1, false, false, ""));
  addCheckInfo(new CheckInfoType("poolcheckui", FastLSCheck,
                                 CheckInfo::MemoryCheck,
                                 1, 2, -1, -1, -1, false, false, ""));
  addCheckInfo(new CheckInfoType("poolcheck_debug", FastLSCheck,
                                 CheckInfo::MemoryCheck,
                                 1, 2, -1, -1, -1, false, false, ""));
  addCheckInfo(new CheckInfoType("poolcheckui_debug", FastLSCheck,
                                 CheckInfo::MemoryCheck,
                                 1, 2, -1, -1, -1, false, false, ""));

  // Add gep checks.
  CheckInfoType *ExactCheck2 = new CheckInfoType("exactcheck2", NULL,
                                                 CheckInfo::GEPCheck,
                                                 0, -1, 1, 3, 2, false, true,
                                                 "");
  addCheckInfo(ExactCheck2);
  addCheckInfo(new CheckInfoType("boundscheck", ExactCheck2,
                                 CheckInfo::GEPCheck,
                                 1, -1, -1, -1, 2, false, false, ""));
  addCheckInfo(new CheckInfoType("boundscheckui", ExactCheck2,
                                 CheckInfo::GEPCheck,
                                 1, -1, -1, -1, 2, false, false, ""));
  addCheckInfo(new CheckInfoType("boundscheck_debug", ExactCheck2,
                                 CheckInfo::GEPCheck,
                                 1, -1, -1, -1, 2, false, false, ""));
  addCheckInfo(new CheckInfoType("boundscheckui_debug", ExactCheck2,
                                 CheckInfo::GEPCheck,
                                 1, -1, -1, -1, 2, false, false, ""));

  // Add global variable registration
  addCheckInfo(new CheckInfoType("pool_register_global", NULL,
                                 CheckInfo::GlobalRegistration,
                                 -1, -1, 1, 2, -1, false, false, ""));
  addCheckInfo(new CheckInfoType("pool_register_global_debug", NULL,
                                 CheckInfo::GlobalRegistration,
                                 -1, -1, 1, 2, -1, false, false, ""));

  // Add stack variable registration and unregistration
  addCheckInfo(new CheckInfoType("pool_register_stack", NULL,
                                 CheckInfo::StackRegistration,
                                 -1, -1, 1, 2, -1, false, false, ""));
  addCheckInfo(new CheckInfoType("pool_register_stack_debug", NULL,
                                 CheckInfo::StackRegistration,
                                 -1, -1, 1, 2, -1, false, false, ""));
  addCheckInfo(new CheckInfoType("pool_unregister_stack", NULL,
                                 CheckInfo::StackUnregistration,
                                 -1, -1, 1, -1, -1, false, false, ""));
  addCheckInfo(new CheckInfoType("pool_unregister_stack_debug", NULL,
                                 CheckInfo::StackUnregistration,
                                 -1, -1, 1, -1, -1, false, false, ""));

  // Add heap variable registration and unregistration
  addCheckInfo(new CheckInfoType("pool_register", NULL,
                                 CheckInfo::HeapRegistration,
                                 -1, -1, 1, 2, -1, false, false, ""));
  addCheckInfo(new CheckInfoType("pool_register_debug", NULL,
                                 CheckInfo::HeapRegistration,
                                 -1, -1, 1, 2, -1, false, false, ""));
  addCheckInfo(new CheckInfoType("pool_unregister", NULL,
                                 CheckInfo::HeapUnregistration,
                                 -1, -1, 1, -1, -1, false, false, ""));
  addCheckInfo(new CheckInfoType("pool_unregister_debug", NULL,
                                 CheckInfo::HeapUnregistration,
                                 -1, -1, 1, -1, -1, false, false, ""));
}
