//===- PoolRegisterElimination.cpp ---------------------------------------- --//
// 
//                          The SAFECode Compiler 
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
//  This pass eliminates unnessary poolregister() / poolunregister() in the
//  code. Redundant poolregister() happens when there are no boundscheck() /
//  poolcheck() on a certain GEP, possibly all of these checks are lowered to
//  exact checks.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "poolreg-elim"
#include "safecode/OptimizeChecks.h"
#include "safecode/Support/AllocatorInfo.h"
#include "SCUtils.h"

#include "dsa/DSSupport.h"

#include "llvm/ADT/Statistic.h"


NAMESPACE_SC_BEGIN

static RegisterPass<PoolRegisterElimination>
X ("poolreg-elim", "Pool Register Eliminiation");

// Pass Statistics
namespace {
  STATISTIC (RemovedRegistration,
  "Number of object registrations/deregistrations removed");

  STATISTIC (TypeSafeRegistrations,
  "Number of type safe object registrations/deregistrations removed");

  STATISTIC (SingletonRegistrations,
  "Number of singleton object registrations/deregistrations removed");
}

//
// Method: findSafeGlobals()
//
// Description:
//  Find global variables that do not escape into memory or external code.
//
template<typename insert_iterator>
void
PoolRegisterElimination::findSafeGlobals (Module & M, insert_iterator InsertPt) {
  for (Module::global_iterator GV = M.global_begin();
       GV != M.global_end();
       ++GV) {
    if (!escapesToMemory (GV))
      InsertPt = GV;
  }

  return;
}

bool
PoolRegisterElimination::runOnModule(Module & M) {
  //
  // Get access to prequisite analysis passes.
  //
  intrinsic = &getAnalysis<InsertSCIntrinsic>();
  dsaPass   = &getAnalysis<EQTDDataStructures>();
  TS = &getAnalysis<dsa::TypeSafety<EQTDDataStructures> >();

  //
  // Get the set of safe globals.
  //
  findSafeGlobals (M, std::inserter (SafeGlobals, SafeGlobals.begin()));

  //
  // List of registration intrinsics.
  //
  const char * registerIntrinsics[] = {
    "pool_register_global",
    "pool_register_stack",
    "pool_unregister_stack"
  };

  //
  // Remove all unused registrations.
  //
  unsigned numberOfIntrinsics=sizeof(registerIntrinsics) / sizeof (const char*);
  for (size_t i = 0; i < numberOfIntrinsics; ++i) {
    removeUnusedRegistrations (registerIntrinsics[i]);
  }

  //
  // Remove registrations for type-safe singleton objects.
  //
  removeTypeSafeRegistrations ("pool_register");

  //
  // Remove registrations for singleton objects.  Note that we only do this for
  // heap objects.
  //
  removeSingletonRegistrations ("pool_register");

  //
  // Deallocate memory and return;
  //
  SafeGlobals.clear();
  return true;
}

bool
DebugPoolRegisterElimination::runOnModule(Module & M) {
  //
  // Get access to prequisite analysis passes.
  //
  intrinsic = &getAnalysis<InsertSCIntrinsic>();

  //
  // List of registration intrinsics.
  //
  const char * registerIntrinsics[] = {
    "pool_register_global",
    "pool_register_stack",
    "pool_unregister_stack",
  };

  //
  // Remove all unused registrations.
  //
  unsigned numberOfIntrinsics=sizeof(registerIntrinsics) / sizeof (const char*);
  for (size_t i = 0; i < numberOfIntrinsics; ++i) {
    removeUnusedRegistrations (registerIntrinsics[i]);
  }

  //
  // Deallocate memory and return;
  //
  return true;
}

//
// Method: isSafeToRemove()
//
// Description:
//  Determine whether the registration for the specified pointer value can be
//  safely removed.
//
// Inputs:
//  Ptr - The pointer value that is registered.
//
// Return value:
//  true  - The registration of this value can be safely removed.
//  false - The registration of this value may not be safely removed.
//
bool
PoolRegisterElimination::isSafeToRemove (Value * Ptr) {
  //
  // We can remove registrations on global variables that don't escape to
  // memory.
  //
  if (GlobalVariable * GV = dyn_cast<GlobalVariable>(Ptr)) {
    if (SafeGlobals.count (GV))
      return true;
  }

  return false;
}

//
// Method: removeUnusedRegistration()
//
// Description:
//  This method take the name of a registration function and removes all
//  registrations made with that function for pointers that are never checked.
//
// Inputs:
//  name - The name of the registration intrinsic.
//
void
PoolRegisterElimination::removeUnusedRegistrations (const char * name) {
  //
  // Scan through all uses of each registration function and see if it can be
  // safely removed.  If so, schedule it for removal.
  //
  std::vector<CallInst*> toBeRemoved;
  Function * F = intrinsic->getIntrinsic(name).F;

  //
  // Look for and record all registrations that can be deleted.
  //
  for (Value::use_iterator UI=F->use_begin(), UE=F->use_end();
       UI != UE;
       ++UI) {
    CallInst * CI = cast<CallInst>(*UI);
    if (isSafeToRemove (intrinsic->getValuePointer(CI))) {
      toBeRemoved.push_back(CI);
    }
  }

  //
  // Update the statistics.
  //
  if (toBeRemoved.size())
    RemovedRegistration += toBeRemoved.size();

  //
  // Remove the unnecesary registrations.
  //
  std::vector<CallInst*>::iterator it, end;
  for (it = toBeRemoved.begin(), end = toBeRemoved.end(); it != end; ++it) {
    (*it)->eraseFromParent();
  }
}

void
PoolRegisterElimination::removeTypeSafeRegistrations (const char * name) {
  //
  // Scan through all uses of the registration function and see if it can be
  // safely removed.  If so, schedule it for removal.
  //
  std::vector<CallInst*> toBeRemoved;
  Function * F = intrinsic->getIntrinsic(name).F;

  //
  // Look for and record all registrations that can be deleted.
  //
  for (Value::use_iterator UI=F->use_begin(), UE=F->use_end();
       UI != UE;
       ++UI) {
    //
    // Get the pointer to the registered object.
    //
    CallInst * CI = cast<CallInst>(*UI);
    Value * Ptr = intrinsic->getValuePointer(CI);
    // Lookup the DSNode for the value in the function's DSGraph.
    //
    DSGraph * TDG = dsaPass->getDSGraph(*(CI->getParent()->getParent()));
    DSNodeHandle DSH = TDG->getNodeForValue(Ptr);
    assert ((!(DSH.isNull())) && "No DSNode for Value!\n");

    //
    // If the DSNode is type-safe and is never used as an array, then there
    // will never be a need to look it up in a splay tree, so remove its
    // registration.
    //
    DSNode * N = DSH.getNode();
    if(!N->isArrayNode() && 
       TS->isTypeSafe(Ptr, F)){
      toBeRemoved.push_back(CI);
    }
  }

  //
  // Update the statistics.
  //
  if (toBeRemoved.size()) {
    RemovedRegistration += toBeRemoved.size();
    TypeSafeRegistrations += toBeRemoved.size();
  }

  //
  // Remove the unnecesary registrations.
  //
  std::vector<CallInst*>::iterator it, end;
  for (it = toBeRemoved.begin(), end = toBeRemoved.end(); it != end; ++it) {
    (*it)->eraseFromParent();
  }
}

void
PoolRegisterElimination::removeSingletonRegistrations (const char * name) {
  //
  // Scan through all uses of the registration function and see if it can be
  // safely removed.  If so, schedule it for removal.
  //
  std::vector<CallInst*> toBeRemoved;
  Function * F = intrinsic->getIntrinsic(name).F;

  //
  // Look for and record all registrations that can be deleted.
  //
  for (Value::use_iterator UI=F->use_begin(), UE=F->use_end();
       UI != UE;
       ++UI) {
    //
    // Get the pointer to the registered object.
    //
    CallInst * CI = cast<CallInst>(*UI);
    Value * Ptr = intrinsic->getValuePointer(CI);

    //
    // Lookup the DSNode for the value in the function's DSGraph.
    //
    DSGraph * TDG = dsaPass->getDSGraph(*(CI->getParent()->getParent()));
    DSNodeHandle DSH = TDG->getNodeForValue(Ptr);
    assert ((!(DSH.isNull())) && "No DSNode for Value!\n");

    //
    // If the object being registered is the same size as that found in the
    // DSNode, then we know it's a singleton object.  The run-time doesn't need
    // such objects registered in the splay trees, so we can remove the
    // registration function.
    //
    DSNode * N = DSH.getNode();
    Value * Size = intrinsic->getObjectSize (Ptr->stripPointerCasts());
    if (Size) {
      if (ConstantInt * C = dyn_cast<ConstantInt>(Size)) {
        unsigned long size = C->getZExtValue();
        if (size == N->getSize()) {
          toBeRemoved.push_back(CI);
          continue;
        }
      }
    }
  }

  //
  // Update the statistics.
  //
  if (toBeRemoved.size()) {
    RemovedRegistration += toBeRemoved.size();
    SingletonRegistrations += toBeRemoved.size();
  }

  //
  // Remove the unnecesary registrations.
  //
  std::vector<CallInst*>::iterator it, end;
  for (it = toBeRemoved.begin(), end = toBeRemoved.end(); it != end; ++it) {
    (*it)->eraseFromParent();
  }
}

char PoolRegisterElimination::ID      = 0;
char DebugPoolRegisterElimination::ID = 0;

NAMESPACE_SC_END
