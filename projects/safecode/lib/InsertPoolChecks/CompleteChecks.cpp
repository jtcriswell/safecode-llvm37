//===- CompleteChecks.cpp - Make run-time checks complete ----------------- --//
// 
//                          The SAFECode Compiler 
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This pass instruments loads and stores with run-time checks to ensure memory
// safety.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "safecode"

#include "poolalloc/RuntimeChecks.h"
#include "safecode/CheckInfo.h"
#include "safecode/CompleteChecks.h"
#include "safecode/Utility.h"

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Module.h"

#include <stdint.h>

namespace llvm {

char CompleteChecks::ID = 0;

static RegisterPass<CompleteChecks>
X ("compchecks", "Make run-time checks complete");

// Pass Statistics
namespace {
  STATISTIC (CompLSChecks, "Complete Load/Store Checks");
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
CompleteChecks::getDSNodeHandle (const Value * V, const Function * F) {
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
// Function: makeCStdLibCallsComplete()
//
// Description:
//  Fills in completeness information for all calls of a given CStdLib function
//  assumed to be of the form:
//
//   pool_X(POOL *p1, ..., POOL *pN, void *a1, ..., void *aN, ..., uint8_t c);
//
//  Specifically, this function assumes that there are as many pointer arguments
//  to check as there are initial pool arguments, and the pointer arguments
//  follow the pool arguments in corresponding order. Also, it is assumed that
//  the final argument to the function is a byte sized bit vector.
//
//  This function fills in this final byte with a constant value whose ith
//  bit is set exactly when the ith pointer argument is complete.
//
// Inputs:
//
//  F            - A pointer to the CStdLib function appearing in the module
//                 (non-null).
//  PoolArgs     - The number of initial pool arguments for which a
//                 corresponding pointer value requires a completeness check
//                 (required to be at most 8).
//  IsDebug      - Flags that this is a debug version of the function.
//
void
CompleteChecks::makeCStdLibCallsComplete (Function *F,
                                          unsigned PoolArgs,
                                          bool IsDebug) {
  assert(F != 0 && "Null function argument!");

  assert(PoolArgs <= 8 && \
    "Only up to 8 arguments are supported by CStdLib completeness checks!");

  Value::use_iterator U = F->use_begin();
  Value::use_iterator E = F->use_end();

  //
  // Hold the call instructions that need changing.
  //
  typedef std::pair<CallInst *, uint8_t> VectorReplacement;
  std::set<VectorReplacement> callsToChange;

  Type *int8ty = Type::getInt8Ty(F->getContext());
  FunctionType *F_type = F->getFunctionType();

  //
  // Verify the type of the function is as expected.
  //
  // There should be as many pointer parameters to check for completeness
  // as there are pool parameters. The last parameter should be a byte.
  //
  assert(F_type->getNumParams() >= PoolArgs * 2 && \
    "Not enough arguments to transformed CStdLib function call!");
  for (unsigned arg = PoolArgs; arg < PoolArgs * 2; ++arg)
    assert(isa<PointerType>(F_type->getParamType(arg)) && \
      "Expected pointer argument to function!");

  //
  // This is the position of the vector operand in the call.
  //
  unsigned vect_position;
  if (IsDebug)
    vect_position = F_type->getNumParams() - 4;
  else
    vect_position = F_type->getNumParams() - 1;

  assert(F_type->getParamType(vect_position) == int8ty && \
    "Unexpected parameter type where complete byte should be!");

  //
  // Iterate over all calls of the function in the module, computing the
  // vectors for each call as it is found.
  //
  for (; U != E; ++U) {
    CallInst *CI;
    if ((CI = dyn_cast<CallInst>(*U)) &&
         CI->getCalledValue()->stripPointerCasts() == F) {

      uint8_t vector = 0x0;

      //
      // Get the parent function to which this instruction belongs.
      //
      Function *P = CI->getParent()->getParent();

      //
      // Iterate over the pointer arguments that need completeness checking
      // and build the completeness vector.
      //
      CallSite CS (CI);
      for (unsigned arg = 0; arg < PoolArgs; ++arg) {
        bool complete = true;
        //
        // Go past all the pool arguments to get the pointer to check.
        //
        Value *V = CS.getArgument(PoolArgs + arg)->stripPointerCasts();

        //
        // Check for completeness of the pointer using DSA and 
        // set the bit in the vector accordingly.
        //
        DSNode *N;
        if ((N = getDSNodeHandle(V, P).getNode()) &&
              (N->isExternalNode() || N->isIncompleteNode() ||
               N->isUnknownNode()  || N->isIntToPtrNode()   ||
               N->isPtrToIntNode()) ) {
            complete = false;
        }

        if (complete)
          vector |= (1 << arg);
      }

      //
      // Add the instruction and vector to the set of instructions to change.
      //
      callsToChange.insert(VectorReplacement(CI, vector));
    }
  }

  //
  // Iterate over all call instructions that need changing, modifying the
  // final operand of the call to hold the bit vector value.
  //
  std::set<VectorReplacement>::iterator change = callsToChange.begin();
  std::set<VectorReplacement>::iterator change_end = callsToChange.end();

  while (change != change_end) {
    Constant *vect_value = ConstantInt::get(int8ty, change->second);
    CallSite CS (change->first);
    CS.setArgument(vect_position, vect_value);
    ++change;
  }

  return;

}

//
// Function: makeComplete()
//
// Description:
//  Find run-time checks on memory objects for which we have complete analysis
//  information and change them into complete functions.
//
// Inputs:
//  M         - A reference to the module to modify.
//  CheckInfo - Information about the run-time check.
//
// Outputs:
//  M - The module is modified so that incomplete checks are changed to
//      complete checks if necessary.
//
void
CompleteChecks::makeComplete (Module & M, const struct CheckInfo & CheckInfo) {
  //
  // Get the run-time checking functions.
  //
  Function * Complete   = M.getFunction (CheckInfo.completeName);
  Function * Incomplete = M.getFunction (CheckInfo.name);

  //
  // If the incomplete function does not exist within the module, then don't
  // do anything.
  //
  if (!Incomplete)
    return;

  //
  // If the complete version of the function does not exist, then create it.
  //
  if (!Complete) {
    FunctionType * FT = Incomplete->getFunctionType();
    Complete=cast<Function>(M.getOrInsertFunction (CheckInfo.completeName, FT));
  }

  //
  // Scan through all uses of the run-time check and record any checks on
  // complete pointers.
  //
  std::vector <CallInst *> toChange;
  Value::use_iterator UI = Incomplete->use_begin();
  Value::use_iterator  E = Incomplete->use_end();
  for (; UI != E; ++UI) {
    if (CallInst * CI = dyn_cast<CallInst>(*UI)) {
      if (CI->getCalledValue()->stripPointerCasts() == Incomplete) {
        //
        // Get the pointer that is checked by this run-time check.
        //
        Value * CheckPtr = CheckInfo.getCheckedPointer (CI);

        //
        // If the pointer is complete, then change the check.
        //
        Function * F = CI->getParent()->getParent();
        if (DSNode * N = getDSNodeHandle (CheckPtr, F).getNode()) {
          if (!(N->isExternalNode() ||
                N->isIncompleteNode() ||
                N->isUnknownNode() ||
                N->isIntToPtrNode() ||
                N->isPtrToIntNode())) {
            toChange.push_back (CI);
          }
        }
      }
    }
  }

  //
  // Update statistics.  Note that we only assign if the value is non-zero;
  // this prevents the statistics from being reported if the value is zero.
  //
  if (toChange.size())
    CompLSChecks += toChange.size();

  //
  // Now iterate through all of the call sites and transform them to be
  // complete.
  //
  for (unsigned index = 0; index < toChange.size(); ++index) {
    toChange[index]->setCalledFunction (Complete);
  }

  return;
}

//
// Function: makeFSParameterCallsComplete()
//
// Description:
//  Finds calls to sc.fsparameter and fills in the completeness byte which
//  is the last argument to such call. The second argument to the function
//  is the one which is analyzed for completeness.
//
// Inputs:
//  M      - Reference to the the module to analyze
//
void
CompleteChecks::makeFSParameterCallsComplete(Module &M)
{
  Function *sc_fsparameter = M.getFunction("__sc_fsparameter");

  if (sc_fsparameter == NULL)
    return;

  std::set<CallInst *> toComplete;

  //
  // Iterate over all uses of sc.fsparameter and discover which have a complete
  // pointer argument.
  //
  for (Function::use_iterator i = sc_fsparameter->use_begin();
       i != sc_fsparameter->use_end(); ++i) {
    CallInst *CI;
    CI = dyn_cast<CallInst>(*i);
    if (CI == 0 || CI->getCalledFunction() != sc_fsparameter)
      continue;

    //
    // Get the parent function to which this call belongs.
    //
    Function *P = CI->getParent()->getParent();
    Value *PtrOperand = CI->getOperand(2);
    
    DSNode *N = getDSNodeHandle(PtrOperand, P).getNode();

    if (N == 0                ||
        N->isExternalNode()   ||
        N->isIncompleteNode() ||
        N->isUnknownNode()    ||
        N->isPtrToIntNode()   ||
        N->isIntToPtrNode()) {
      continue;
    }

    toComplete.insert(CI);
  }

  //
  // Fill in a 1 for each call instruction that has a complete pointer
  // argument.
  //
  Type *int8      = Type::getInt8Ty(M.getContext());
  Constant *complete = ConstantInt::get(int8, 1);

  for (std::set<CallInst *>::iterator i = toComplete.begin();
       i != toComplete.end();
       ++i) {
    CallInst *CI = *i;
    CI->setOperand(4, complete);
  }

  return;
}

#if 0
//
// Method: getFunctionTargets()
//
// Description:
//  This method finds all of the potential targets of the specified indirect
//  function call.
//
void
CompleteChecks::getFunctionTargets (CallSite CS,
                                    std::vector<const Function *> & Targets) {
  EQTDDataStructures & dsaPass = getAnalysis<EQTDDataStructures>();
  const DSCallGraph & callgraph = dsaPass.getCallGraph();
  DSGraph* G = dsaPass.getGlobalsGraph();
  DSGraph::ScalarMapTy& SM = G->getScalarMap();

  //
  // Scan through all targets of this call site within DSA's callgraph.
  //
  DSCallGraph::callee_iterator csi = callgraph.callee_begin(CS);
  DSCallGraph::callee_iterator cse = callgraph.callee_end(CS);
  for (; csi != cse; ++csi) {
    //
    // The DSCallGraph maps call sites to targets.  However, each target can
    // represent an entire strongly connected component (SCC) in the call
    // graph.  Therefore, we need to find all of the functions represented by
    // the SCC.
    //
    const Function * SCCLeader = *csi;
    Targets.push_back (SCCLeader);
    DSCallGraph::scc_iterator sccii = callgraph.scc_begin(SCCLeader);
    DSCallGraph::scc_iterator sccee = callgraph.scc_end(SCCLeader);
    for (; sccii != sccee; ++sccii) {
      Targets.push_back (*sccii);
    }
  }

  const Function *F1 = CS.getInstruction()->getParent()->getParent();

  //
  // It's possible that the call instruction is within an SCC.  Find the leader
  // of the SCC and find which functions it can call.
  //
  F1 = callgraph.sccLeader(&*F1);
  DSCallGraph::flat_iterator sccii = callgraph.flat_callee_begin(F1),
                 sccee = callgraph.flat_callee_end(F1);
  for (; sccii != sccee; ++sccii) {
    Targets.push_back (*sccii);
  }

  return;
}
#else
//
// Method: getFunctionTargets()
//
// Description:
//  This method finds all of the potential targets of the specified indirect
//  function call.
//
void
CompleteChecks::getFunctionTargets (CallSite CS,
                                    std::vector<const Function *> & Targets) {
  //
  // Get the call graph.
  //
  CallGraph & CG = getAnalysis<CallGraphWrapperPass>().getCallGraph();

  //
  // Get the call graph node for the function containing the call.
  //
  CallGraphNode * CGN = CG[CS.getInstruction()->getParent()->getParent()];

  //
  // Iterate through all of the target call nodes and add them to the list of
  // targets to use in the global variable.
  //
  for (CallGraphNode::iterator ti = CGN->begin(); ti != CGN->end(); ++ti) {
    //
    // See if this call record corresponds to the call site in question.
    //
    const Value * V = ti->first;
    if (V != CS.getInstruction())
      continue;

    //
    // Get the node in the graph corresponding to the callee.
    //
    CallGraphNode * CalleeNode = ti->second;

    //
    // Get the target function(s).  If we have a normal callee node as the
    // target, then just fetch the function it represents out of the callee
    // node.  Otherwise, this callee node represents external code that could
    // call any address-taken function.  In that case, we'll have to do extra
    // work to get the potential targets.
    //
    if (1) {
      //
      // Get the call graph node that represents external code that calls
      // *into* the program.  Get the list of callees of this node and make
      // them targets.
      //
      CallGraphNode * ExternalNode = CG.getExternalCallingNode();
      for (CallGraphNode::iterator ei = ExternalNode->begin();
                                   ei != ExternalNode->end(); ++ei) {
        if (Function * Target = ei->second->getFunction()) {
          //
          // Do not include intrinsics or functions that do not get emitted
          // into the executable.
          //
          if ((Target->isIntrinsic()) ||
              (Target->hasAvailableExternallyLinkage())) {
            continue;
          }
          Targets.push_back (Target);
        }
      }
    } else {
      //
      // Get the function target out of the node.
      //
      if (Function * Target = CalleeNode->getFunction()) {
        if ((!Target->isIntrinsic()) ||
            (!Target->hasAvailableExternallyLinkage())) {
          Targets.push_back (Target);
        }
      }
    }
  }

  return;
}

#endif

//
// Method: fixupCFIChecks()
//
// Description:
//  Search for all complete checks on indirect function calls and update the
//  table of potential targets using DSA results.  Note that we do this here
//  because we don't have a complete call graph when analyzing individual
//  compilation units.
//
// Preconditions:
//  This method assumes that we have already converted incomplete checks to
//  complete checks.
//
void
CompleteChecks::fixupCFIChecks (Module & M, std::string name) {
  //
  // See if this run-time check is used in this program.  If not, do nothing.
  //
  Function * FuncCheck = M.getFunction (name);
  if (!FuncCheck) return;

  //
  // Scan through all uses of the funccheck() function.
  //
  PointerType * VoidPtrType = getVoidPtrType(M.getContext());
  Value::use_iterator UI = FuncCheck->use_begin();
  Value::use_iterator  E = FuncCheck->use_end();
  for (; UI != E; ++UI) {
    if (CallInst * CI = dyn_cast<CallInst>(*UI)) {
      if (CI->getCalledValue()->stripPointerCasts() == FuncCheck) {
        //
        // Get the call instruction following this call instruction.
        //
        BasicBlock::iterator I = CI;
        CallInst * ICI;
        do {
          ++I;
          assert (!isa<TerminatorInst>(I));
        } while ((ICI = dyn_cast<CallInst>(I)) == 0);

        //
        // Get the list of potential function targets.  Note that we have to
        // do some silly things to get rid of the "const"-ness of the functions
        // that we find.
        //
        //
        std::vector<const Function *> Targets;
        getFunctionTargets (ICI, Targets);
        std::vector<Constant *> GoodTargets;
        for (unsigned index = 0; index < Targets.size(); ++index) {
          Constant * C = M.getFunction (Targets[index]->getName());
          GoodTargets.push_back(ConstantExpr::getZExtOrBitCast(C, VoidPtrType));
        }
        GoodTargets.push_back (ConstantPointerNull::get (VoidPtrType));

        //
        // Create a new global variable containing the list of targets.
        //
        ArrayType * AT = ArrayType::get (VoidPtrType, GoodTargets.size());
        Constant * TargetArray = ConstantArray::get (AT, GoodTargets);
        Value * NewTable = new GlobalVariable (M,
                                               AT,
                                               true,
                                               GlobalValue::InternalLinkage,
                                               TargetArray,
                                               "TargetList");

        //
        // Install the new target list into the check.
        //
        NewTable = castTo (NewTable, VoidPtrType, ICI);
        CI->setArgOperand (1, NewTable);
      }
    }
  }

  return;
}

bool
CompleteChecks::runOnModule (Module & M) {
  //
  // For every run-time check, go and see if it can be converted into a
  // complete check.
  //
  for (unsigned index = 0; index < numChecks; ++index) {
    //
    // Skip this run-time check if it is the complete version.
    //
    if (RuntimeChecks[index].isComplete)
      continue;

    //
    // Get a pointer to the complete and incomplete versions of the run-time
    // check.
    //
    makeComplete (M, RuntimeChecks[index]);
  }

  //
  // Iterate over the CStdLib functions whose entries are known to DSA.
  // For each function call, do a completeness check on the given number of
  // pointer arguments and mark the completeness bit vector accordingly.
  //
  const unsigned NumRuntimeChecks =
    sizeof(RuntimeCheckEntries) / sizeof(RuntimeCheckEntries[0]);

  for (unsigned EntryIndex = 0; EntryIndex < NumRuntimeChecks; ++EntryIndex) {
    // Look for CStdLib function entries in the runtime check table.
    if (RuntimeCheckEntries[EntryIndex].CheckKind == CStdLibCheck) {
      unsigned PoolArgc = RuntimeCheckEntries[EntryIndex].PoolArgc;
      std::string FuncName = RuntimeCheckEntries[EntryIndex].Function;

      // Process the regular version of the function
      if (Function * F = M.getFunction(FuncName)) {
        makeCStdLibCallsComplete(F, PoolArgc, false);
      }

      // Process the debug version of the function
      if (Function * F = M.getFunction(FuncName + "_debug")) {
        makeCStdLibCallsComplete(F, PoolArgc, true);
      }
    }
  }

  //
  // For every call to sc.fsparameter, fill in the relevant completeness
  // information about its pointer argument.
  //
  makeFSParameterCallsComplete(M);

  //
  // Fixup the targets of indirect function calls.
  //
  fixupCFIChecks(M, "funccheck");
  fixupCFIChecks(M, "funccheck_debug");
  return true;
}

}
