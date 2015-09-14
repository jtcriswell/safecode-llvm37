//===- StackSafety.cpp: - Analysis for Ensuring Stack Safety --------------===//
//
//                          The SAFECode Compiler
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Implementation of StackSafety.h
//
// FIXME:
//  Can this pass get better results by using another DSA pass?  It seems this
//  pass may be too conservative by using the Top-Down DSA results.
//
//===----------------------------------------------------------------------===//

#include "llvm/Module.h"
#include "llvm/Function.h"
#include "llvm/Instructions.h"
#include "llvm/BasicBlock.h"
#include "llvm/Type.h"
#include "llvm/Pass.h"
#include "llvm/Support/InstIterator.h"
#include "StackSafety.h"
#include <iostream>

using namespace llvm;
 
using namespace CSS;

char checkStackSafety::ID = 0;

RegisterPass<checkStackSafety> css("css1", "check stack safety", true, true);

//
// Method: markReachableAllocas()
//
// Description:
//  Find all of the DSNodes that alias with stack objects and are reachable
//  from the specified DSNode.
//
// Inputs:
//  DSN   - The DSNode from which reachability of stack objects begins.
//  start - Flags whether the initial DSNode (DSN) should be ignored in the
//          reachability analysis.
//
// Return value:
//  true  - At least one DSNode reachable from the specified DSNode aliases
//          with a stack object.
//  false - No DSNode reachable from this DSNode alises with a stack object.
//  
bool
checkStackSafety::markReachableAllocas(DSNode *DSN, bool start) {
  reachableAllocaNodes.clear();
  return   markReachableAllocasInt(DSN,start);
}

//
// Method: markReachableAllocasInt()
//
// Description:
//  Find all of the DSNodes that alias with stack objects and are reachable
//  from the specified DSNode.  This is the recursive helper function to
//  markReachableAllocas(); it does not clear the set of reachable allocas.
//
// Inputs:
//  DSN   - The DSNode from which reachability of stack objects begins.
//  start - Flags whether the initial DSNode (DSN) should be ignored in the
//          reachability analysis.
//
// Return value:
//  true  - At least one DSNode reachable from the specified DSNode aliases
//          with a stack object.
//  false - No DSNode reachable from this DSNode alises with a stack object.
//  
bool
checkStackSafety::markReachableAllocasInt(DSNode *DSN, bool start) {
  bool returnValue = false;
  reachableAllocaNodes.insert(DSN);

  //
  // If the initial node is an alloca node, then put it in the reachable set.
  //
  if (!start && DSN->isAllocaNode()) {
    returnValue =  true;
    AllocaNodes.insert (DSN);
  }

  //
  // Look at the DSNodes reachable from this DSNode.  If they alias with the
  // stack, put them in the reachable set.
  //
  DataLayout & TD = getAnalysis<DataLayout>();
  for (unsigned i = 0, e = DSN->getSize(); i < e; i += TD.getPointerSize())
    if (DSNode *DSNchild = DSN->getLink(i).getNode()) {
      if (reachableAllocaNodes.find(DSNchild) != reachableAllocaNodes.end()) {
        continue;
      } else if (markReachableAllocasInt(DSNchild)) {
        returnValue = returnValue || true;
      }
    }
  return returnValue;
}

//
// Method: runOnModule()
//
// Description:
//  This is where invocation of this analysis pass begins.
//
// Inputs:
//  M - A reference to the module to analyze.
//
// Return value:
//  true  - The module was modified.
//  false - The module was not modified.
//
bool
checkStackSafety::runOnModule(Module &M) {
  //  TDDataStructures *TDDS;
  //  TDDS = &getAnalysis<TDDataStructures>();
  EQTDDataStructures *BUDS;
  BUDS = &getAnalysis<EQTDDataStructures>();

  //
  // Get a pointer to the entry of the program.
  //
  Function *MainFunc = M.getFunction("main") ? M.getFunction("main")
                                             : M.getFunction ("MAIN__");

  //
  // Scan each function and look for stack objects which can escape from the
  // function.
  //
  for (Module::iterator MI = M.begin(), ME = M.end(); MI != ME; ++MI) {
    Function &F = *MI;
    if (&F != MainFunc) {
      if (!F.isDeclaration()) {
        DSGraph * BUG = BUDS->getDSGraph(F);

        //
        // If the function can return a pointer, see if a stack object can
        // escape via the return value.
        //
        if (isa<PointerType>(F.getReturnType())) {
          for (inst_iterator ii = inst_begin(F), ie = inst_end(&F);
                             ii != ie;
                             ++ii) {
            if (ReturnInst *RI = dyn_cast<ReturnInst>(&*ii)) {
              DSNode *DSN = BUG->getNodeForValue(RI).getNode();
              if (DSN) {
                markReachableAllocas(DSN);
              }
            }
          }
        }
    
        //
        // Conservatively assume that any stack object reachable from one of
        // the incoming arguments is a stack object that is placed there as an
        // "output" by this function (or one of its callees).
        //
        Function::arg_iterator AI = F.arg_begin(), AE = F.arg_end();
        for (; AI != AE; ++AI) {
          if (isa<PointerType>(AI->getType())) {
            DSNode *DSN = BUG->getNodeForValue(AI).getNode();
            markReachableAllocas(DSN, true);
          }
        }

        //
        // Any stack object that is reachable by a global may also escape the
        // function.  Scan both for local variables that may alias with globals
        // as well as globals that are directly accessed by the function.
        //
        DSGraph::node_iterator DSNI = BUG->node_begin(), DSNE = BUG->node_end();
        for (; DSNI != DSNE; ++DSNI) {
          if (DSNI->isGlobalNode()) {
            markReachableAllocas(DSNI);
          }
        }

        DSGraph * GlobalGraph = BUG->getGlobalsGraph();
        DSNI = GlobalGraph->node_begin(), DSNE = GlobalGraph->node_end();
        for (; DSNI != DSNE; ++DSNI) {
          if (DSNI->isGlobalNode()) {
            markReachableAllocas(DSNI);
          }
        }
      }
    }
  }

  //
  // This pass never changes the module; always return false.
  //
  return false;
}


Pass *createStackSafetyPass() { return new CSS::checkStackSafety(); }

