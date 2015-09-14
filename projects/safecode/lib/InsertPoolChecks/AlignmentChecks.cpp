//===- AlignmentChecks.cpp - Insert alignment run-time checks ------------- --//
// 
//                          The SAFECode Compiler 
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This pass instruments the code with alignment checks.  This is required when
// load/store checks on type-safe memory objects are optimized away; pointers
// to type-safe memory objects that are loaded from type-unsafe memory objects
// may not point to a valid memory object or may not be alignment properly
// within a valid memory object.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "safecode"

#include "safecode/SAFECode.h"
#include "safecode/InsertChecks.h"
#include "SCUtils.h"

#include "llvm/ADT/Statistic.h"

NAMESPACE_SC_BEGIN

char AlignmentChecks::ID = 0;

static RegisterPass<AlignmentChecks>
X ("alignchecks", "Insert Alignment Checks");

// Pass Statistics
namespace {
  STATISTIC (AlignChecks, "Alignment Checks Added");
}

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
AlignmentChecks::getDSNodeHandle (const Value * V, const Function * F) {
  //
  // Ensure that the function has a DSGraph
  //
  assert (dsaPass->hasDSGraph(*F) && "No DSGraph for function!\n");

  //
  // Lookup the DSNode for the value in the function's DSGraph.
  //
  DSGraph * TDG = dsaPass->getDSGraph(*F);
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
// Method: isTypeKnown()
//
// Description:
//  Determines whether the value is always used in a type-consistent fashion
//  within the program.
//
// Inputs:
//  V - The value to check for type-consistency.  This value *must* have a
//      DSNode.
//
// Return value:
//  true  - This value is always used in a type-consistent fashion.
//  false - This value is not guaranteed to be used in a type-consistent
//          fashion.
//
bool
AlignmentChecks::isTypeKnown (const Value * V, const Function * F) {
  //
  // First, get the DSNode for the value.
  //
  DSNode * DSN = getDSNodeHandle (V, F).getNode();
  assert (DSN && "isTypeKnown(): No DSNode for the specified value!\n");

  //
  // Now determine if it is type-consistent.
  //
  return (!(DSN->isNodeCompletelyFolded()));
}

//
// Method: visitLoadInst()
//
// Description:
//  Place a run-time check on a load instruction.
//
void
AlignmentChecks::visitLoadInst (LoadInst & LI) {
  //
  // Don't do alignment checks on non-pointer values.
  //
  if (!(isa<PointerType>(LI.getType())))
    return;

  //
  // Get the function in which the load instruction lives.
  //
  Function * F = LI.getParent()->getParent();

  //
  // Get the DSNode for the result of the load instruction.  If it is type
  // unknown, then no alignment check is needed.
  //
  if (!isTypeKnown (&LI, F))
    return;

  //
  // Get the pool handle for the node.
  //
  Value *PH = ConstantPointerNull::get (getVoidPtrType(LI.getContext()));

  //
  // If the node is incomplete or unknown, then only perform the check if
  // checks to incomplete or unknown are allowed.
  //
  Constant * CheckAlignment = PoolCheckAlign;
  DSNode * DSNode = getDSNodeHandle (&LI, F).getNode();
  if (DSNode->getNodeFlags() & (DSNode::IncompleteNode | DSNode::UnknownNode)) {
    CheckAlignment = PoolCheckAlignUI;
    return;
  }

  //
  // A check is needed.  Fetch the alignment of the loaded pointer and insert
  // an alignment check.
  //
  const Type * Int32Type = Type::getInt32Ty(F->getParent()->getContext());
  unsigned offset = getDSNodeHandle (&LI, F).getOffset();
  Value * Alignment = ConstantInt::get(Int32Type, offset);

  // Insertion point for this check is *after* the load.
  BasicBlock::iterator InsertPt = LI;
  ++InsertPt;

  //
  // Create instructions to cast the checked pointer and the checked pool
  // into sbyte pointers.
  //
  Value *CastLI  = castTo (&LI, getVoidPtrType(LI.getContext()), InsertPt);
  Value *CastPHI = castTo (PH, getVoidPtrType(LI.getContext()), InsertPt);

  // Create the call to poolcheckalign
  std::vector<Value *> args(1, CastPHI);
  args.push_back(CastLI);
  args.push_back (Alignment);
  CallInst::Create (CheckAlignment, args.begin(), args.end(), "", InsertPt);

  // Update the statistics
  ++AlignChecks;

  return;
}

bool
AlignmentChecks::runOnFunction (Function & F) {
  //
  // Get pointers to required analysis passes.
  //
  TD      = &getAnalysis<DataLayout>();
  dsaPass = &getAnalysis<EQTDDataStructures>();

  //
  // Get a pointer to the run-time check function.
  //
  PoolCheckAlign   = F.getParent()->getFunction ("sc.lscheckalign");
  PoolCheckAlignUI = F.getParent()->getFunction ("sc.lscheckalignui");

  //
  // Visit all of the instructions in the function.
  //
  visit (F);
  return true;
}

NAMESPACE_SC_END

