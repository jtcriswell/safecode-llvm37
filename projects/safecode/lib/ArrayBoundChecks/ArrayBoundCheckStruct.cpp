//===- ArrayBoundCheckStruct.cpp - Static Array Bounds Checking --------------//
// 
//                          The SAFECode Compiler 
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file implements the ArrayBoundsCheckStruct pass.  This pass utilizes
// type-safety information from points-to analysis to prove whether GEPs are
// safe (they do not create a pointer outside of the memory object).  It is
// primarily designed to alleviate run-time checks on GEPs used for structure
// indexing (hence the clever name).
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "abc-struct"

#include "SCUtils.h"
#include "safecode/ArrayBoundsCheck.h"
#include "safecode/SAFECodeConfig.h"
#include "safecode/Support/AllocatorInfo.h"

#include "dsa/DSGraph.h"
#include "dsa/DSNode.h"

#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"

using namespace llvm;


namespace {
  STATISTIC (allGEPs ,  "Total Number of GEPs Queried");
  STATISTIC (safeGEPs , "Number of GEPs on Structures Proven Safe Statically");
}

NAMESPACE_SC_BEGIN

namespace {
  RegisterPass<ArrayBoundsCheckStruct>
  X ("abc-struct", "Structure Indexing Array Bounds Check pass");

  RegisterAnalysisGroup<ArrayBoundsCheckGroup> ABCGroup(X);
}

char ArrayBoundsCheckStruct::ID = 0;

//
// Method: getDSNodeHandle()
//
// Description:
//  This method looks up the DSNodeHandle for a given LLVM value.  The context
//  of the value is the specified function, although if it is a global value,
//  the DSNodeHandle may exist within the global DSGraph.
//
// Return value:
//  A DSNodeHandle for the value is returned.  This DSNodeHandle could either
//  be in the function's DSGraph or from the GlobalsGraph.  Note that the
//  DSNodeHandle may represent a NULL DSNode.
//
DSNodeHandle
ArrayBoundsCheckStruct::getDSNodeHandle (const Value * V, const Function * F) {
  //
  // Get access to the points-to results.
  //
  EQTDDataStructures & dsaPass = getAnalysis<EQTDDataStructures>();

  //
  // Ensure that the function has a DSGraph
  //
  assert (dsaPass.hasDSGraph(*F) && "No DSGraph for function!\n");

  //
  // Lookup the DSNode for the value in the function's DSGraph.
  //
  DSGraph * TDG = dsaPass.getDSGraph(*F);
  DSNodeHandle DSH = TDG->getNodeForValue(V);

  //
  // If the value wasn't found in the function's DSGraph, then maybe we can
  // find the value in the globals graph.
  //
  if ((DSH.isNull()) && (isa<GlobalValue>(V))) {
    //
    // Try looking up this DSNode value in the globals graph.  Note that
    // globals are put into equivalence classes; we may need to first find the
    // equivalence class to which our global belongs, find the global that
    // represents all globals in that equivalence class, and then look up the
    // DSNode Handle for *that* global.
    //
    DSGraph * GlobalsGraph = TDG->getGlobalsGraph ();
    DSH = GlobalsGraph->getNodeForValue(V);
    if (DSH.isNull()) {
      //
      // DSA does not currently handle global aliases.
      //
      if (!isa<GlobalAlias>(V)) {
        //
        // We have to dig into the globalEC of the DSGraph to find the DSNode.
        //
        const GlobalValue * GV = dyn_cast<GlobalValue>(V);
        const GlobalValue * Leader;
        Leader = GlobalsGraph->getGlobalECs().getLeaderValue(GV);
        DSH = GlobalsGraph->getNodeForValue(Leader);
      }
    }
  }

  return DSH;
}

//
// Method: runOnFunction()
//
// Description:
//  This is the entry point for this analysis pass.  We grab the required
//  analysis results from other passes here.  However, we don't actually
//  compute anything in this method.  Instead, we compute results when queried
//  by other passes.  This makes the bet that each GEP will only be quered
//  once, and only if some other analysis pass can't prove it safe before this
//  pass can.
//
// Inputs:
//  F - A reference to the function to analyze.
//
// Return value:
//  true  - This pass modified the function (which should never happen).
//  false - This pass did not modify the function.
//
bool
ArrayBoundsCheckStruct::runOnFunction(Function & F) {
  //
  // Get required analysis results from other passes.
  //
  abcPass = &getAnalysis<ArrayBoundsCheckGroup>();

  //
  // We don't make any changes, so return false.
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
ArrayBoundsCheckStruct::isGEPSafe (GetElementPtrInst * GEP) {
  //
  // Update the count of GEPs queried.
  //
  ++allGEPs;

  //
  // Get the source pointer of the GEP.  This is the pointer off of which the
  // indexing operation takes place.
  //
  Value * PointerOperand = GEP->getPointerOperand();

  //
  // Determine whether the pointer is for a type-known object.
  //
  Function * F = GEP->getParent()->getParent();
  DSNode * N = getDSNodeHandle (PointerOperand, F).getNode();
  if (N) {
    //
    // If DSA says that the object is type-known but not an array node, then
    // we know that this is just structure indexing.  We can therefore declare
    // it safe.
    //
    if ((!N->isNodeCompletelyFolded()) &&
        (!(N->isArrayNode())) &&
        (!(N->isIncompleteNode())) &&
        (!(N->isUnknownNode())) &&
        (!(N->isIntToPtrNode())) &&
        (!(N->isExternalNode()))) {
      if (indexesStructsOnly (GEP)) {
        ++safeGEPs;
        return true;
      }
    }
  }

  //
  // We cannot statically prove that the GEP is safe.  Ask another array bounds
  // checking pass to prove the GEP safe.
  //
  return abcPass->isGEPSafe(GEP);
}


NAMESPACE_SC_END
