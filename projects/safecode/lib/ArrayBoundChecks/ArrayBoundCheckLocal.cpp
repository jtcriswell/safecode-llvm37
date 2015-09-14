//===- ArrayBoundCheck.cpp - Static Array Bounds Checking --------------------//
// 
//                          The SAFECode Compiler 
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// ArrayBoundsCheckLocal - It tries to prove a GEP is safe only based on local
// information, that is, the size of global variables and the size of objects
// being allocated inside a function.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "abc-local"

#include "safecode/ArrayBoundsCheck.h"

#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"

#include <set>
#include <queue>

using namespace llvm;

namespace {
  STATISTIC (allGEPs ,    "Total Number of GEPs Queried");
  STATISTIC (safeGEPs ,   "Number of GEPs Proven Safe Statically");
  STATISTIC (unsafeGEPs , "Number of GEPs Proven Unsafe Statically");
}

RegisterPass<ArrayBoundsCheckLocal>
X ("abc-local", "Local Array Bounds Check pass");

static RegisterAnalysisGroup<ArrayBoundsCheckGroup>
ABCGroup (X);

char ArrayBoundsCheckLocal::ID = 0;

//
// Function: findObject()
//
// Description:
//  Find the singular memory object to which this pointer points (if such a
//  singular object exists and is easy to find).
//
static Value *
findObject (Value * obj) {
  std::set<Value *> exploredObjects;
  std::set<Value *> objects;
  std::queue<Value *> queue;

  queue.push(obj);
  while (!queue.empty()) {
    Value * o = queue.front();
    queue.pop();
    if (exploredObjects.count(o)) continue;

    exploredObjects.insert(o);

    if (isa<CastInst>(o)) {
      queue.push(cast<CastInst>(o)->getOperand(0));
    } else if (isa<GetElementPtrInst>(o)) {
      queue.push(cast<GetElementPtrInst>(o)->getPointerOperand());
    } else if (isa<PHINode>(o)) {
      PHINode * p = cast<PHINode>(o);
      for(unsigned int i = 0; i < p->getNumIncomingValues(); ++i) {
        queue.push(p->getIncomingValue(i));
      }
    } else {
      objects.insert(o);
    }
  }
  return objects.size() == 1 ? *(objects.begin()) : NULL;
}

//
// Method: visitGetElementPtrInst()
//
// Description:
//  This visitor method determines whether the specified GEP always stays
//  within the bounds of an allocated object.
//
void
ArrayBoundsCheckLocal::visitGetElementPtrInst (GetElementPtrInst & GEP) {
  //
  // Get information about allocators.
  //
  AllocatorInfoPass & AIP = getAnalysis<AllocatorInfoPass>();

  //
  // Update the count of GEPs queried.
  //
  ++allGEPs;

  //
  // Get the checked pointer and try to find the memory object from which it
  // originates.  If we can't find the memory object, let some other static
  // array bounds checking pass have a crack at it.
  //
  Value * PointerOperand = GEP.getPointerOperand();
  Value * memObject = findObject (PointerOperand);
  if (!memObject)
    return;
  Value * objSize = AIP.getObjectSize(memObject);
  if (!objSize)
    return;

  //
  // Calculate the:
  //  offset: Distance from the start of the memory object to the calculated
  //          pointer
  //  zero  : The zero value
  //  bounds: The size of the object
  //  diff  : The difference between the bounds and the offset
  //
  // SCEVs for GEP indexing operations seems to be the size of a pointer.
  // Therefore, use an integer size equal to the pointer size.
  //
  const SCEV * base   = SE->getSCEV(memObject);
  const SCEV * offset = SE->getMinusSCEV(SE->getSCEV(&GEP), base);
  const SCEV * zero = SE->getSCEV(Constant::getNullValue(TD->getIntPtrType(GEP.getType())));

  //
  // Create an SCEV describing the bounds of the object.  On 64-bit platforms,
  // this may be a 32-bit integer while the offset value may be a 64-bit
  // integer.  In that case, we need to create a new SCEV that zero-extends
  // the object size from 32 to 64 bits.
  //
  const SCEV * bounds = SE->getSCEV(objSize);
  if ((TD->getTypeAllocSize (bounds->getType())) < 
      (TD->getTypeAllocSize (offset->getType()))) {
    bounds = SE->getZeroExtendExpr(bounds, offset->getType());
  }
  const SCEV * diff = SE->getMinusSCEV(bounds, offset);

  //
  // If the offset is less than zero, then we know that we are indexing
  // backwards from the beginning of the object.  We know that this is illegal;
  // declare it unsafe.
  //
  if ((SE->getSMaxExpr(offset, zero) == zero) && (offset != zero)) {
    ++unsafeGEPs;
    return;
  }

  //
  // Otherwise, we are indexing zero or more bytes forward.  Determine whether
  // we will index past the end of the object.
  //
  if ((SE->getSMaxExpr(diff, zero) == diff) && (diff != zero)) {
    ++safeGEPs;
    SafeGEPs.insert (&GEP);
    return;
  }
  
  //
  // We cannot statically prove that the GEP is safe.
  //
  return;
}

bool
ArrayBoundsCheckLocal::runOnFunction(Function & F) {
  //
  // Get required analysis passes.
  //
  TD = &F.getParent()->getDataLayout();
  SE = &getAnalysis<ScalarEvolution>();

  //
  // Look for all GEPs in the function and try to prove that they're safe.
  //
  visit (F);

  //
  // We modify nothing; return false.
  //
  return false;
}

//
// Function: isGEPSafe()
//
// Description:
//  Determine whether the GEP will always generate a pointer that lands within
//  the bounds of the object.
//
// Inputs:
//  GEP - The getelementptr instruction to check.
//
// Return value:
//  true  - The GEP never generates a pointer outside the bounds of the object.
//  false - The GEP may generate a pointer outside the bounds of the object.
//          There may also be cases where we know that the GEP *will* return an
//          out-of-bounds pointer; we let pointer rewriting take care of those
//          cases.
//
bool
ArrayBoundsCheckLocal::isGEPSafe (GetElementPtrInst * GEP) {
  return ((SafeGEPs.count(GEP)) > 0);
}

