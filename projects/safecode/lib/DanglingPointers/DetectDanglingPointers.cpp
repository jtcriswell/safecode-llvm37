//===- DetectDanglingPointers.cpp - Insert calls to mark objects read-only  --//
// 
//                          The SAFECode Compiler 
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This pass instruments a program so that it marks the shadow pages of
// heap objects read-only; this is used for the dangling pointer detection as
// described in the DSN 2006 paper "Efficiently Detecting All Dangling Pointer
// Uses in Production Servers."
//
// Notes:
//  o) This pass must be run before the pass that adds poolunregister() calls.
//     This is because the run-time must change the memory protections before
//     unregistering the object.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "dpchecks"

#include "llvm/ADT/Statistic.h"

#include "safecode/SAFECode.h"
#include "safecode/SAFECodeConfig.h"
#include "safecode/Support/AllocatorInfo.h"
#include "safecode/DetectDanglingPointers.h"
#include "SCUtils.h"

#include "dsa/DSSupport.h"

#include <iostream>
#include <utility>
#include <vector>

char NAMESPACE_SC::DetectDanglingPointers::ID = 0;

NAMESPACE_SC_BEGIN

// Statistics
STATISTIC (Changes,    "Number of Shadowing Calls Inserted");

//
// Method: createFunctionProtos()
//
// Description:
//  Create the function prototypes for shadowing and unshadowing objects.
//
void
DetectDanglingPointers::createFunctionProtos (Module & M) {
  //
  // Get basic integer and pointer types.
  //
  const Type * Int8Type  = IntegerType::getInt8Ty(getGlobalContext());
  const Type * Int32Type  = IntegerType::getInt32Ty(getGlobalContext());
  Type * VoidPtrTy = PointerType::getUnqual(Int8Type);

  //
  // Get the function that unshadows heap objects.
  //
  std::vector<const Type *> Arg(1, VoidPtrTy);
  FunctionType * Ty = FunctionType::get(VoidPtrTy, Arg, false);
  ProtectObj = M.getOrInsertFunction("pool_unshadow", Ty);

  //
  // Get the function that shadows heap objects.
  //
  Arg.push_back (Int32Type);
  Ty = FunctionType::get(VoidPtrTy, Arg, false);
  ShadowObj  = M.getOrInsertFunction("pool_shadow", Ty);

  return;
}

void
DetectDanglingPointers::processFrees (Module & M,
                                      std::set<Function *> & FreeFuncs) {
  //
  // Scan through all uses of all heap deallocation functions.  For each one,
  // insert a call to the run-time library that will change the page
  // protections so that reads and writes to the object will cause a hardware
  // fault.
  //
  AllocatorInfoPass & AIP = getAnalysis<AllocatorInfoPass>();
  std::vector<std::pair<CallInst *, Value *> > Worklist;
  AllocatorInfoPass::AllocatorInfoListTy::iterator i;
  for (i = AIP.alloc_begin(); i != AIP.alloc_end(); ++i) {
    // Get the allocator information structure
    AllocatorInfo * info = *i;

    //
    // Clear the work list.
    //
    Worklist.clear();

    // Reference to the deallocation function
    Function * freeFunc = M.getFunction(info->getFreeCallName());
    if (freeFunc) {
      //
      // Record the deallocation function in the set so that we can quickly
      // look it up later.
      //
      FreeFuncs.insert (freeFunc);

      //
      // Iterate over all uses of the free function and add instrumentation.
      //
      Value::use_iterator  it, end;
      for (it = freeFunc->use_begin(), end = freeFunc->use_end();
           it != end;
           ++it) {
        if (CallInst * CI = dyn_cast<CallInst>(*it)) {
          //
          // Backup one instruction since the preceding instruction should be
          // a call to poolunregister().
          //
          BasicBlock::iterator InsertPt = CI;
          assert (InsertPt != CI->getParent()->begin());
          --InsertPt;

          //
          // Create the call.
          //
          Value * Pointer = info->getFreedPointer (CI);
          CallInst * OrigPtr = CallInst::Create (ProtectObj,
                                                 Pointer,
                                                 "",
                                                 InsertPt);

          //
          // Add to the worklist the call instruction that we will need to
          // change and the new pointer value that should be freed.
          //
          Worklist.push_back (std::make_pair(CI, OrigPtr));
        }
      }
    }

    //
    // Update the statistics if the worklist has any elements.  This avoids
    // printing a statistic of zero in the results.
    //
    if (Worklist.size()) Changes += Worklist.size();

    //
    // Go through the work list and change all of the deallocation calls to
    // use the original pointer returned from the pool_unshadow() call.
    //
    while (Worklist.size()) {
      CallInst * FreeCall = Worklist.back().first;
      Value *    OrigPtr  = Worklist.back().second;
      Worklist.pop_back();

      FreeCall->setOperand (1, OrigPtr);
    }
  }

  return;
}

bool
DetectDanglingPointers::runOnModule (Module & M) {
  //
  // If dangling pointer protection is disabled, do nothing.
  //
  if (!(SCConfig.dpChecks())) return false;

  //
  // Get prerequisite analysis results.
  //
  intrinPass = &getAnalysis<InsertSCIntrinsic>();

  //
  // Create the functions for shadowing and unshadowing objects.
  //
  createFunctionProtos (M);

  //
  // Process the deallocation functions first.  This allows us to collect the
  // a list of the deallocation functions while instrumenting them so that they
  // free the originally allocated object and not the shadow object.
  //
  std::set<Function *> FreeFuncs;
  processFrees (M, FreeFuncs);

  //
  // Scan through all calls to allocation functions.  For each allocation,
  // add a call after it to remap the object to a shadow object.  Then, replace
  // all uses of the original pointer with the shadow pointer.
  //
  AllocatorInfoPass & AIP = getAnalysis<AllocatorInfoPass>();
  AllocatorInfoPass::AllocatorInfoListTy::iterator i;
  for (i = AIP.alloc_begin(); i != AIP.alloc_end(); ++i) {
    // Get the allocator information structure
    AllocatorInfo * info = *i;

    // Reference to the allocation function
    Function * allocFunc = M.getFunction(info->getAllocCallName());
    if (allocFunc) {
      //
      // Iterate over all uses of the allocation function.
      //
      Value::use_iterator  it, end;
      for (it = allocFunc->use_begin(), end = allocFunc->use_end();
           it != end;
           ++it) {
        if (CallInst * CI = dyn_cast<CallInst>(*it)) {
          if (CI->getCalledFunction() == allocFunc) {
            //
            // FIXME: This should eventually use an integer that is identical
            //        in size to the address space.
            //
            const Type * Int32Type=IntegerType::getInt32Ty(getGlobalContext());
            BasicBlock::iterator InsertPt = CI;
            ++InsertPt;
            Value * allocSize = info->getOrCreateAllocSize(CI);
            allocSize = CastInst::CreateIntegerCast (allocSize,
                                                     Int32Type,
                                                     false,
                                                     allocSize->getName(),
                                                     InsertPt);

            //
            // This is an allocation site.  Add a call after it to create a
            // shadow copy of the allocated object.
            //
            std::vector<Value *> args;
            args.push_back (CI);
            args.push_back (allocSize);
            CallInst * Shadow = CallInst::Create (ShadowObj, args.begin(), args.end(), "", InsertPt);

            //
            // Replace all uses of the originally allocated pointer with the
            // shadow pointer.
            //
            CI->replaceAllUsesWith (Shadow);

            //
            // The previous statement modified the call to pool_shadow() so
            // that it takes its return value as its argument.  Change its
            // argument back to the original allocated object.
            //
            Shadow->setOperand (1, CI);

            //
            // Update the statistics.
            //
            ++Changes;
          }
        }
      }
    }
  }

  //
  // We most likely changed something; conservatively claim that we made
  // modifications.
  //
  return true;
}

NAMESPACE_SC_END

