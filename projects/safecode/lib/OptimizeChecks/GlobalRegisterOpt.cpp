//===- GlobalRegisterOpt.cpp ---------------------------------------------- --//
// 
//                          The SAFECode Compiler 
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
//  This pass eliminates unnessary pool_register_global() calls in the code.
//
//===----------------------------------------------------------------------===//

#include <iostream>

#define DEBUG_TYPE "poolreg-global-elim"

#include "safecode/OptimizeChecks.h"
#include "safecode/Utility.h"

#include "llvm/ADT/Statistic.h"

// Pass Statistics
namespace {
  STATISTIC (RemovedRegistration,
  "Number of object registrations/deregistrations removed");
}

char GlobalRegisterOpt::ID = 0;

static RegisterPass<GlobalRegisterOpt>
X ("poolreg-global-elim", "Global Variable Register Eliminiation");

//
// Method: findSafeGlobals()
//
// Description:
//  Find global variables that do not escape into memory or external code.
//
template<typename insert_iterator>
static void
findSafeGlobals (Module & M, insert_iterator InsertPt) {
  for (Module::global_iterator GV = M.global_begin();
       GV != M.global_end();
       ++GV) {
    if (!escapesToMemory (GV))
      InsertPt = GV;
  }

  return;
}

//
// Method: isSafeToRemove()
//
// Description:
//  Determine whether the registration for the specified pointer value can be
//  safely removed.
//
// Inputs:
//  Ptr        - The pointer value that is registered.
//  SafeValues - The values that do not need registration.
//
// Return value:
//  true  - The registration of this value can be safely removed.
//  false - The registration of this value may not be safely removed.
//
bool
isSafeToRemove (Value * Ptr, std::set<Value *> & SafeValues) {
  //
  // We can remove registrations on global variables that don't escape to
  // memory.
  //
  if (GlobalVariable * GV = dyn_cast<GlobalVariable>(Ptr->stripPointerCasts()))
    if (SafeValues.count (GV))
      return true;

  return false;
}

//
// Method: removeUnusedRegistration()
//
// Description:
//  This method take a registration function and removes all
//  registrations made with that function for pointers that are never checked.
//
// Inputs:
//  F          - The registration function.
//  SafeValues - The set of values that are never checked.
//
void
removeUnusedRegistrations (Function * F, std::set<Value *> & SafeValues) {
  //
  // If the function does not exist, do nothing.
  //
  if (!F)
    return;

  //
  // Scan through all uses of each registration function and see if it can be
  // safely removed.  If so, schedule it for removal.
  //
  std::vector<Instruction *> toBeRemoved;

  //
  // Look for and record all registrations that can be deleted.
  //
  for (Value::use_iterator UI=F->use_begin(), UE=F->use_end();
       UI != UE;
       ++UI) {
    CallSite CS (cast<CallInst>(*UI));
    if (CS.getInstruction()) {
      if (isSafeToRemove (CS.getArgument (2), SafeValues)) {
        toBeRemoved.push_back(CS.getInstruction());
      }
    }
  }

  //
  // Update the statistics.
  //
  if (toBeRemoved.size())
    RemovedRegistration += toBeRemoved.size();

  std::cerr << "Removing " << toBeRemoved.size() << " Globals!" << std::endl;

  //
  // Remove the unnecesary registrations.
  //
  std::vector<Instruction *>::iterator it, end;
  for (it = toBeRemoved.begin(), end = toBeRemoved.end(); it != end; ++it) {
    (*it)->eraseFromParent();
  }

  return;
}

namespace llvm {

//
// Method: runOnModule()
//
// Description:
//  Entry point for this pass.  Find calls to pool_register_global() that are
//  unneeded and eliminate them.
//
bool
GlobalRegisterOpt::runOnModule(Module & M) {
  //
  // Get the pool registration function.  If it doesn't exist or if it has no
  // uses, do nothing.
  //
  Function * RegisterGlobal      = M.getFunction ("pool_register_global");
  Function * RegisterGlobalDebug = M.getFunction ("pool_register_global_debug");

  if (!RegisterGlobal && !RegisterGlobalDebug)
    return false;

  //
  // Find the set of globals that do not need to be registered.
  //
  std::set<Value *> SafeGlobals;
  findSafeGlobals (M, std::inserter (SafeGlobals, SafeGlobals.begin()));

  //
  // Remove all unused registrations.
  //
  removeUnusedRegistrations (RegisterGlobal, SafeGlobals);
  removeUnusedRegistrations (RegisterGlobalDebug, SafeGlobals);

  return true;
}

}
