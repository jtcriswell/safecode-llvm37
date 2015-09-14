//===- BottomUpCallGraph.cpp - -----------------------------------------------//
// 
//                          The SAFECode Compiler 
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// I believe this pass does two things:
//  o) It attempts to improve upon the call graph calculated by DSA for those
//     call sites in which a callee was not found.
//  o) It finds functions that are part of Strongly Connected Components (SCCs)
//     in the call graph and marks them being a part of an SCC.
//
// FIXME:
//  I believe the fixup of the call graph is no longer necessary; DSA asserts
//  if it can't find a callee for a call instruction.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "bucg"

#include "dsa/DSGraph.h"
#include "dsa/DSNode.h"
#include "BottomUpCallGraph.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/CallSite.h"
#include <iostream>

using namespace llvm;

static RegisterPass<BottomUpCallGraph> bucg("bucg","Call Graph from CBUDS");

namespace llvm {

char BottomUpCallGraph::ID = 0;

//
// This is needed because some call sites get merged away during DSA if they
// have the same inputs for instance.
// But for array bounds checking we need to get constraints from all the call
// sites
// So we have to get them some how.
//
bool
BottomUpCallGraph::runOnModule(Module &M) {
  EQTDDataStructures &CBU = getAnalysis<EQTDDataStructures>();

  const DSCallGraph & callGraph = CBU.getCallGraph();
  for (Module::iterator MI = M.begin(), ME = M.end(); MI != ME; ++MI) {
    for (inst_iterator I = inst_begin(MI), E = inst_end(MI); I != E; ++I) {
      if (CallInst *CI = dyn_cast<CallInst>(&*I)) {
        DSCallGraph::callee_iterator cI = callGraph.callee_begin(CI),
                                     cE = callGraph.callee_end(CI);
        if (cI == cE) {
          // This call site is not included in the cbuds
          // so we need to extra stuff.
          CallSite CS = CallSite::get(CI);
          if (Function *FCI = dyn_cast<Function>(CI->getOperand(0))) {
            //if it is a direct call, we can just add it!
            FuncCallSiteMap[FCI].push_back(CS);
          } else {
            // Here comes the ugly part
            Function *parenFunc = CI->getParent()->getParent();
            DSNode *calleeNode = CBU.getDSGraph(*parenFunc)->getNodeForValue(CS.getCalledValue()).getNode();
            CalleeNodeCallSiteMap.insert(std::make_pair(calleeNode, CS));
          }
        }
      }
    }
  }

#if 0
  //
  // Get the set of instructions for which the points-to graph has callee
  // information.
  //
  std::vector<const Instruction *> Instructions;
  CBU.callee_get_keys (Instructions);
#endif

  //
  // Process each callee of each indirect call site.  We scan through each
  // indirect call site known by the EQTD DSA pass and process its callees.
  //
  DSCallGraph::callee_key_iterator I = callGraph.key_begin();
  DSCallGraph::callee_key_iterator E = callGraph.key_end();
  for (; I != E; ++I) {
    CallSite CS = *I;
    DSCallGraph::callee_iterator i = callGraph.callee_begin(CS);
    DSCallGraph::callee_iterator e = callGraph.callee_end(CS);
    for (; i != e; ++i) {
      const Function * Target = *i;
#if 0
      DOUT << "CALLEE: " << Target->getName()
           << " from : " << *(CI) << std::endl;
#endif
      FuncCallSiteMap[Target].push_back(CS);

      // FIXME:
      //  This is a very expensive way of doing it, 
      //
      // Determine if this is equivalent to any other callsites of this
      // function.
      Function *parenFunc = CS.getInstruction()->getParent()->getParent();
      DSNode *calleeNode = CBU.getDSGraph(*parenFunc)
                             ->getNodeForValue(CS.getCalledValue()).getNode();
      CalleeNodeCallSiteMapTy::const_iterator cI, cE;
      tie(cI, cE) = CalleeNodeCallSiteMap.equal_range(calleeNode);
      for (; cI != cE; ++cI) {
        //
        // All the call sites Target should also be added to the
        // funccallsitemap
        //
        FuncCallSiteMap[Target].push_back(cI->second);
      }
    }
  }
  figureOutSCCs(M);
  return false;
}


void
BottomUpCallGraph::visit (Function *f) {
  if (Visited.find(f) == Visited.end()) {
    // Record that we have now visited this function
    Visited.insert(f);

    //
    // If we have not visited this function before, that implies that the
    // function won't be on the stack; therefore, push it on stack
    //
    Stack.push_back(f);

    //
    // Visit all the functions that can call this function.
    //
    if (FuncCallSiteMap.count(f)) {
      std::vector<CallSite> & callsitelist = FuncCallSiteMap[f];
      for (unsigned idx = 0, sz = callsitelist.size(); idx != sz; ++idx) {
        Function *parent = callsitelist[idx].getInstruction()->getParent()
                                                             ->getParent();
        visit(parent);
      }
    }
    Stack.pop_back();
  } else {
    // We have already visited this function; check if it forms an SCC
    std::vector<Function*>::iterator res = std::find (Stack.begin(),
                                                      Stack.end(),
                                                      f);
    if (res != Stack.end()) {
      // Cycle detected.
      for (; res != Stack.end() ; ++res) {
        SccList.insert(*res);
      }
    }
  }
}

void
BottomUpCallGraph::figureOutSCCs (Module &M) {
  for (Module::iterator I = M.begin(), E= M.end(); I != E ; ++I) {
    visit(I);
  }
}
}

