//===- CZeroInfo.h: CZero Info ---------------------------------*- C++ -*--===//
//
//                          The SAFECode Compiler
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file checks the LLVM code for any potential security holes. We allow
// a restricted number of usages in order to preserve memory safety etc.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ANALYSIS_CZEROINFO_H
#define LLVM_ANALYSIS_CZEROINFO_H

#include "llvm/Type.h"
#include "llvm/Value.h"
#include "llvm/Instructions.h"
#include "llvm/BasicBlock.h"
#include "llvm/Analysis/Dominators.h"
#include "llvm/Pass.h"
#include "llvm/Support/CFG.h"
#include <algorithm>

using std::string;
using std::map;
using std::vector;
using std::set;

namespace llvm {

// A dummy node is used when the value pointed to is unknown as yet.
struct PointsToTarget {
private:  
  const Value *Val;
  bool Dummy;
  bool Global;

  // If the node is a dummy, the following fields don't matter
  bool Array;
  bool Struct;
  bool Heap;
  int TargetID;

  static int NextTargetID;

public:
   
  static const int DummyTarget = 1;
  static const int GlobalTarget = 2;
  
  bool operator<(const PointsToTarget PTT) const {
    return (TargetID < PTT.TargetID);
  }
  
  PointsToTarget(const Value *V) : Val(V) {
    TargetID = NextTargetID++;
    Dummy = false;
    Global = false;
    Heap = false;
    Array = false;
    Struct = false;
    if (isa<AllocaInst>(V)) {
      const AllocaInst *AIV = dyn_cast<AllocaInst>(V);
      Array = AIV->isArrayAllocation();
      if (!AIV->isArrayAllocation()) {
	if (AIV->getAllocatedType()->getTypeID() == Type::StructTyID)
	  Struct = true;
      }
    }
    else if (isa<MallocInst>(V)) {
      const MallocInst *MIV = dyn_cast<MallocInst>(V);
      Array = MIV->isArrayAllocation();
      if (!MIV->isArrayAllocation()) {
	if (MIV->getAllocatedType()->getTypeID() == Type::StructTyID)
	  Struct = true;
      }
      Heap = true;
    }
  }
  
  // Default constructor creates a dummy node.
  PointsToTarget(int TargetType) {
    TargetID = NextTargetID++;
    Val = 0;
    Dummy = false;
    Global = false;
    if (TargetType == DummyTarget) {
      Dummy = true;
    }
    else if (TargetType == GlobalTarget) {
      Global = true;
    }
    Struct = false;
    Array = false;
    Heap = false;
  }
  
  PointsToTarget() {
    Val = 0;
    Dummy = false;
    Global = false;
    Struct = false;
    Array = false;
    Heap = false;
  }
  
  bool isDummy() { return Dummy; }
  
  bool isGlobal() { return Global; }

  bool isArray() { return Array; }

  bool isStruct() { return Struct; }

  bool isHeap() { return Heap; }

  bool isPHINode() { 
    if (Val) 
      return isa<PHINode>(Val); 
    else
      return false;
  }
  
  const Value* val() { return Val; }
  
};

int PointsToTarget::NextTargetID = 0;

// The graph is to be recreated for each function. This is done by each
// CZeroInfo object being associated with a CZeroAliasGraph
class CZeroAliasGraph {
  // We ensure that memory locations on stack alone are initialized before
  // use. A memory location on the stack is identified by the alloca
  // instruction that created it.  
  
protected:
  // There are edges from a SSA pointer variable to memory locations
  // and from SSA pointer variables to phi nodes.
  // Phi nodes are treated specially in the alias graph.
  std::map<const Value *, PointsToTarget> pointsTo;
  
  // Given a memory location, all the SSA pointer vars that point to it.
  std::map<PointsToTarget, set<const Value *> > pointedBy;
  
  // NOTE: every update to the graph should update both of these maps
  
public:
  
  // Add and edge from V1 to V2.
  // Situations in which this happens is
  // V1: SSA pointer variable, V2: alloca
  // V1: SSA pointer variable, V2: phi node.
  // Called only once for each SSA pointer value.
  void addEdge (const Value *V1, const Value *V2) {
    assert (pointsTo.count(V1) == 0 && "Value should not be inserted in graph yet");
    PointsToTarget PT(V2);
    pointsTo[V1] = PT;
    pointedBy[PT].insert(V1);
  }
  
  // Add an edge from an SSA pointer variable to a dummy if we don't really
  // know what it points to.
  // Eg. If we load an int* from an int** since we currently don't do any
  // flow-sensitive pointer tracking.
  // This or addEdge(V1, V2) called only once for an SSA pointer value
  void addEdge (const Value *V, int TargetType) {
    assert (pointsTo.count(V) == 0 && "Value should not be inserted in graph yet");
    PointsToTarget PT(TargetType);
    pointsTo[V] = PT;
    pointedBy[PT].insert(V);
  }
  
  PointsToTarget getPointsToInfo(const Value *V) {
    return pointsTo[V];
  }
  
  set<const Value *> getPointedByInfo(PointsToTarget PT) {
    return pointedBy[PT];
  }
  
  // make alias an alias of orig. 
  // If orig does not exist, then both of them need to point to a dummy node.
  // NOTE: Call only when alias is the lvalue of an instruction.
  // Call only once for a particular alias
  void addAlias (const Value *alias, const Value *orig) {
    assert (pointsTo.count(alias) == 0 && "Value alias not already in graph");    
    assert (pointsTo.count(orig) != 0 && "Value orig not inserted in graph yet");
    if (pointsTo[orig].val())
      addEdge(alias, pointsTo[orig].val());
    else if (pointsTo[orig].isGlobal())
      addEdge(alias, PointsToTarget::GlobalTarget);
    else if (pointsTo[orig].isDummy())
      addEdge(alias, PointsToTarget::DummyTarget);
  }
  
  // Returns aliases of the value.
  // Return value also contains V
  set<const Value *> getAliases(const Value *V) {
    return pointedBy[pointsTo[V]];
  }
  
};


enum WarningType { 
  NoWarning,
  IllegalMemoryLoc,
  UninitPointer 
};

typedef std::map<const Value *, bool> LivePointerMap;  

// This class contains the information that CZero checks require.
// This is re-instantiated and initialized for each function.
class CZeroInfo {
  // The two phases of our algorithm
  // Phase 1: Examine all the stores by looking at basic blocks in a depth
  // first manner and update the PointerLiveInfo map.
  void depthFirstGatherer();
  // Phase 2: Iterate through basic blocks depth first and see if the loads
  // are safe i.e. there is a store of the pointer at every path to the load
  // in question.
  bool findSpuriousInsts();
  bool checkPredecessors(const BasicBlock *BB, const Value *V,
			 set<const BasicBlock *>& vistedBlocks);
  enum WarningType checkIfStored(const BasicBlock *BB, 
		     const Value *V, 
		     std::set<const Value *>& LocalStoresSoFar);

  enum WarningType checkInstruction(const BasicBlock *BB, 
			const Instruction *I, 
			std::set<const Value *>& LocalStoresSoFar);

  string WarningString(enum WarningType WT);

protected:
  const Function& TheFunction;
  
  // This is the map (BasicBlock1 * Pointer variable) -> BasicBlock2
  // where BasicBlock2 dominates BasicBlock1 and has a store to the pointer
  std::map<const BasicBlock *, LivePointerMap> BBPointerLiveInfo;
  
  // Alias graph to be used in the findSpuriousLoads phase
  // Created in phase 1.
  CZeroAliasGraph PointerAliasGraph;
  
  // Dominator set information
  DominatorTree *DomTree;
  
  string WarningsList;
  
  
public:
  
  CZeroInfo (Function& F, DominatorTree* DSet) : TheFunction(F), DomTree(DSet) {
    
  } 
  
  // Public access method to get all the warnings associated with
  // the particular function.
  string& getWarnings ();
};

}
#endif

