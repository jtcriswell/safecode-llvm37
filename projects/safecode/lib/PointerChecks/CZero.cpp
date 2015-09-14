//===- CZero.cpp: - CZero Security Checks ---------------------------------===//
//
//                          The SAFECode Compiler
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This transformation ensures that the code emitted (if there are no warnings)
// poses no security threat to the target system.
//
//===----------------------------------------------------------------------===//


#include "llvm/Transforms/IPO.h"
#include "CZeroInfo.h"
#include "llvm/Module.h"
#include "llvm/Argument.h"
#include "llvm/Constants.h"
#include "llvm/DerivedTypes.h"
#include "llvm/ADT/DepthFirstIterator.h"
#include "llvm/Function.h"
#include "llvm/Support/CFG.h"
#include <iostream>
using namespace llvm;


//===----------------------------------------------------------------------===//
//  CZeroInfo Implementation
//===----------------------------------------------------------------------===//

string CZeroInfo::WarningString(enum WarningType WT) {
  switch (WT) {
  case NoWarning: return "";
  case IllegalMemoryLoc: return "Accessing an illegal memory location\n";
  case UninitPointer: return "Potential use of location pointed to by uninitialized pointer variable\n";
  default: return "";
  }
}

string& CZeroInfo::getWarnings() {

  // Lazy evaluation. 
  // TODO: Introduce an analyzed bool flag so that we
  // don't end up redo-ing the evaluation when there are no security warnings
  if (WarningsList != "")
    return WarningsList;

  // Uninitialized pointers
  depthFirstGatherer();

  findSpuriousInsts();
  
  return WarningsList;
    
}

void CZeroInfo::depthFirstGatherer() {
  // Adding the pointer values among the arguments to the alias graph
  // We treat them as pointers to global targets.

  for (Function::const_arg_iterator I = TheFunction.arg_begin(), 
	 E = TheFunction.arg_end(); I != E; ++I) {
    if (I->getType()->getTypeID() == Type::PointerTyID) 
      PointerAliasGraph.addEdge(I, PointsToTarget::GlobalTarget);
  }

  df_iterator<const Function*> It = df_begin(&TheFunction), End = df_end(&TheFunction);
  for ( ; It != End; It++) {
    const BasicBlock *BB = *It;
    
    // Look for store instructions sequentially in the basic block
    // updating pointer alias graphs for the other instructions
    BasicBlock::const_iterator iterBB;
    for (iterBB = BB->begin(); iterBB != BB->end(); ++iterBB) {
      const Instruction& I = *iterBB;

      // NOTE!!! Removed the if (I) clause here
      // 
      if (I.hasName() && 
	  I.getType()->getTypeID() == Type::PointerTyID) {
	// Each of these cases needs to modify the alias graph appropriately
	if (isa<AllocaInst>(I)) {
	  PointerAliasGraph.addEdge(&I, &I);
	}
	else if (isa<MallocInst>(I)) {
	  // TODO: We'll be making this illegal and only allowing
	  // calls to rmalloc and rfree.
	  PointerAliasGraph.addEdge(&I, &I);
	}
	else if (isa<LoadInst>(I)) {
	  PointerAliasGraph.addEdge(&I, PointsToTarget::DummyTarget);
	}
	else if (isa<GetElementPtrInst>(I)) {
	  // Check if the operand is a global value, in which case we 
	  // generate an alias to a generic global value.
	  if (!isa<ConstantPointerNull>(I.getOperand(0)))
	    if (isa<GlobalValue>(I.getOperand(0)) || 
		isa<Constant>(I.getOperand(0)))
		PointerAliasGraph.addEdge(&I, PointsToTarget::GlobalTarget);
	    else 
	      PointerAliasGraph.addAlias(&I, I.getOperand(0));
	  else
	    PointerAliasGraph.addEdge(&I, PointsToTarget::DummyTarget);
	}
	else if (isa<PHINode>(I)) {
	  PointerAliasGraph.addEdge(&I, &I);
	}
	else if (isa<CallInst>(I)) {
	  PointerAliasGraph.addEdge(&I, PointsToTarget::GlobalTarget);
	}
	else if (isa<CastInst>(I)) {
	  PointerAliasGraph.addEdge(&I, PointsToTarget::DummyTarget);
	}
      }
      else if (!I.hasName()) {
	if (isa<StoreInst>(I)) {
	  // We only consider stores of scalar pointers.
	  if (I.getNumOperands() <= 2 ||
	      (I.getNumOperands() == 3 &&
	       I.getOperand(2) != getGlobalContext().getConstantInt(Type::Int32Ty, 0))) {
	    if (!isa<ConstantPointerNull>(I.getOperand(1))) {
	      BBPointerLiveInfo[BB][I.getOperand(1)] = true;
	      df_iterator<const Function*> localIt = df_begin(&TheFunction), 
		localEnd = df_end(&TheFunction);
	      for ( ; localIt != localEnd; ++localIt) {
		if (DomTree->dominates((BasicBlock *) BB, 
				      (BasicBlock *) *localIt))
		  BBPointerLiveInfo[*localIt][I.getOperand(1)] = true;
	      }
	    } else {
	      WarningsList += "Stores to null pointers disallowed in CZero\n";
	    }
	  }
	}
      }
    }
  }
}

bool CZeroInfo::checkPredecessors(const BasicBlock *BB, const Value *V,
				  set<const BasicBlock *>& visitedBlocks) {
  set<const Value *> aliases = PointerAliasGraph.getAliases(V);  
  set<const Value *>::iterator Siter;
  
  // Check the block BB itself. Necessary when checkPredecessors is called
  // for a PHINode pointer
  for (Siter = aliases.begin(); Siter != aliases.end(); ++Siter)
    if (BBPointerLiveInfo[BB][*Siter])
      return true;
  
  pred_const_iterator PI = pred_begin(BB), PEnd = pred_end(BB);
  
  if (PI != PEnd) {
    for (; PI != PEnd; ++PI) {
      if (visitedBlocks.find(*PI) == visitedBlocks.end()) {
	bool alivehere = false;
	visitedBlocks.insert(*PI);
	for (Siter = aliases.begin(); Siter != aliases.end(); ++Siter)
	  if (BBPointerLiveInfo[*PI][*Siter])
	    alivehere = true;
	if (!alivehere) {
	  if (!checkPredecessors(*PI, V, visitedBlocks))
	    return false;
	  else {
	    // Caching the value for future use.
	    for (Siter = aliases.begin(); Siter != aliases.end(); Siter++)
	      BBPointerLiveInfo[*PI][*Siter] = true;
	  }
	}
      }
    }
  }
  else
    return false;
  return true;
}

// if BigSet contains even one of the elements in the smallset, return true
// else return false
static bool setContains (set<const Value *> BigSet, set<const Value *> SmallSet) {
  set<const Value *>::iterator Siter;
  bool found = false;
  if (!SmallSet.empty())
    for (Siter = SmallSet.begin(); Siter != SmallSet.end() && !found; Siter++)
      if (BigSet.find(*Siter) != BigSet.end())
	found = true;
  return found;
}

// Called on PointerVar only if PointerVar is a scalar, non-heap variable.
// However the case that PointerVar points to a phi node needs to be handled.
enum WarningType CZeroInfo::checkIfStored(const BasicBlock *BB, 
					  const Value *PointerVar,
					  std::set<const Value *>& 
					  LocalStoresSoFar) {
  if (!isa<ConstantPointerNull>(PointerVar))
    return NoWarning;
  // TODO: Optimization if a pointer is defined in the same basic block.
  set<const Value *> aliases = PointerAliasGraph.getAliases(PointerVar);
  set<const BasicBlock *> visitedBlocks;
  visitedBlocks.insert(BB);
  if (PointerAliasGraph.getPointsToInfo(PointerVar).isPHINode()) {
    // We have a phi node pointer
    // Solve separate problems for each of the predecessors
    if (!setContains(LocalStoresSoFar, aliases)) {
      // Each of the predecessor branches.
      bool stored = true;
      const PHINode *pI = dyn_cast<const PHINode>(PointerAliasGraph.getPointsToInfo(PointerVar).val());
      // There has to be at least one predecessor
      for (unsigned int i = 0; i < pI->getNumIncomingValues(); i++) {
	if (!checkPredecessors(pI->getIncomingBlock(i), 
			       pI->getIncomingValue(i), 
			       visitedBlocks))
	  stored = false;
      }
      if (!stored)
	return UninitPointer;
      else { 
	// Cache the value
	set<const Value *>::iterator Siter;
	for (Siter = aliases.begin(); Siter != aliases.end(); Siter++)
	  BBPointerLiveInfo[BB][*Siter] = true;
      }
    }
  }
  else {
    if (!setContains(LocalStoresSoFar, aliases)) {
      if (!checkPredecessors(BB, PointerVar, visitedBlocks))
	return UninitPointer;
      else {
	// Cache the information that PointerVar and its aliases are live here
	set<const Value *>::iterator Siter;
	for (Siter = aliases.begin(); Siter != aliases.end(); Siter++)
	  BBPointerLiveInfo[BB][*Siter] = true;
      }
    }
  }
  return NoWarning;
}

// Called for load and getelementptr instructions
enum WarningType CZeroInfo::checkInstruction(const BasicBlock *BB, 
					     const Instruction *I,
					     std::set<const Value *>& 
					     LocalStoresSoFar) {
  const Value *PointerVar = I->getOperand(0);
  if (isa<ConstantPointerNull>(I->getOperand(0)))
    return IllegalMemoryLoc;

  // Check for pointer arithmetic
  if (!PointerAliasGraph.getPointsToInfo(PointerVar).isArray()) {
    if (I->getNumOperands() > 1) {
      // Check that every operand is 0 except for struct accesses.
      const Type *elemType = I->getType();
      for(unsigned int i = 1; i < I->getNumOperands(); i++) {
	if (elemType->getTypeID() == Type::PointerTyID) {
	  if (I->getOperand(i) != getGlobalContext().getConstantInt(Type::Int32Ty, 0))
	    return IllegalMemoryLoc;
	  elemType = cast<const PointerType>(elemType)->getElementType();
	}
	else if (elemType->getTypeID() == Type::ArrayTyID) {
	  elemType = cast<const ArrayType>(elemType)->getElementType();
	}
	else if (elemType->getTypeID() == Type::StructTyID) {
	  elemType = cast<const StructType>(elemType)->getTypeAtIndex(I->getOperand(i));
	}
      }
    }
    if (!isa<GlobalValue>(PointerVar) && 
      !(PointerAliasGraph.getPointsToInfo(PointerVar).isGlobal()) &&
      !(PointerAliasGraph.getPointsToInfo(PointerVar).isHeap()) &&
      !(PointerAliasGraph.getPointsToInfo(PointerVar).isStruct()) &&
      !(PointerAliasGraph.getPointsToInfo(PointerVar).isDummy())) {
      return checkIfStored(BB, PointerVar, LocalStoresSoFar);
    }
  }

  return NoWarning;
}
 
bool CZeroInfo::findSpuriousInsts() {
  bool WarningFlag = false;
  df_iterator<const Function*> It = df_begin(&TheFunction), End = df_end(&TheFunction);
  for ( ; It != End; It++) {
    const BasicBlock *BB = *It;
    std::set<const Value *> LocalStoresSoFar;
    
    // Sequentially scan instructions in the block
    BasicBlock::const_iterator iterBB;
    for (iterBB = BB->begin(); iterBB != BB->end(); iterBB++) {
      const Instruction *I = iterBB;
      enum WarningType WT;
      if (!I)
	continue;
      
      if (isa<CastInst>(I)) {
	// Disallow cast instructions involving pointers
	if (I->getType()->getTypeID() == Type::PointerTyID) {
	  WarningsList += I->getName() + ": Casts to pointers disallowed" +
	    "in CZero\n";
	  WarningFlag = true;
	}
	else if (I->getOperand(0)->getType()->getTypeID() 
		 == Type::PointerTyID) {
	  WarningsList += I->getName() + ":Casts from a pointer disallowed " +
	    "in CZero\n";
	  WarningFlag = true;
	}
      }
      // If this is a store instruction update LocalStoresSoFar
      else if (isa<StoreInst>(I)) {
	LocalStoresSoFar.insert(I->getOperand(1));
	
	// Check that there is no pointer arithmetic here
	if (!PointerAliasGraph.getPointsToInfo(I->getOperand(1)).isArray()) {
	  const Type *elemType = I->getOperand(1)->getType();
	  for(unsigned int i = 2; i < I->getNumOperands(); i++) {
	    if (elemType->getTypeID() == Type::PointerTyID) {
	      if (I->getOperand(i) != getGlobalContext().getConstantInt(Type::Int32Ty, 0)) {
		WarningsList += "Stores to pointer variables should not have pointer arithmetic\n";
		WarningFlag = true;
	      }
	      elemType = cast<const PointerType>(elemType)->getElementType();
	    }
	    else if (elemType->getTypeID() == Type::ArrayTyID) {
	      elemType = cast<const ArrayType>(elemType)->getElementType();
	    }
	    else if (elemType->getTypeID() == Type::StructTyID) {
	      elemType = cast<const StructType>(elemType)->getTypeAtIndex(I->getOperand(i));
	    }
	  }
	}

	// If a pointer is stored to another pointer, then we check that
	// the pointer being stored has been stored to. (boy thats twisted!)
	if (I->getOperand(0)->getType()->getTypeID() ==
	    Type::PointerTyID) {
	  if (!isa<GlobalValue>(I->getOperand(0)) && 
	      !(PointerAliasGraph.getPointsToInfo
		(I->getOperand(0)).isGlobal()) &&
	      !(PointerAliasGraph.getPointsToInfo
		(I->getOperand(0)).isHeap()) &&
	      !(PointerAliasGraph.getPointsToInfo
		(I->getOperand(0)).isStruct()) &&
	      !(PointerAliasGraph.getPointsToInfo
		(I->getOperand(0)).isDummy()) &&
	      !(PointerAliasGraph.getPointsToInfo
		(I->getOperand(0)).isArray())) {
	    // TODO: Array
	    WT = checkIfStored(BB, I->getOperand(0), LocalStoresSoFar);
	    if (WT == UninitPointer)
	      WarningsList += "Pointer value being stored potentially uninitialized\n";
	    else
	      WarningsList += WarningString(WT);
	    if (WT != NoWarning)
	      WarningFlag = true;
	  }
	}
	
      }
      else if (isa<LoadInst>(I)) {
	// Check for globals, heaps and structs... all of which we ignore.
	WT = checkInstruction(BB, I, LocalStoresSoFar);
	if (WT == IllegalMemoryLoc) {
	  WarningsList += "Load from illegal memory location\n";
	  WarningFlag = true;
	}
	else {
	  WarningsList += WarningString(WT);
	  if (WT != NoWarning)
	    WarningFlag = true;
	}
      }
      else if (isa<GetElementPtrInst>(I)) {
	WT = checkInstruction(BB, I, LocalStoresSoFar);
	// don't check for pointer arithmetic any more
	if (WT == IllegalMemoryLoc) {
	  //	  WarningsList += "Pointer Arithmetic disallowed\n";
	  //	  WarningFlag = true;
	}
	else {
	  WarningsList += WarningString(WT);
	  if (WT != NoWarning)
	    WarningFlag = true;
	}
      }
      else if (isa<CallInst>(I)) {
	if (I->getNumOperands() > 1)
	  for (unsigned int i = 1; i < I->getNumOperands(); i++) {
	    if (I->getOperand(i)->getType()->getTypeID() ==
		Type::PointerTyID) {
	      if (!isa<GlobalValue>(I->getOperand(i)) && 
		  !(PointerAliasGraph.getPointsToInfo
		    (I->getOperand(i)).isGlobal()) &&
		  !(PointerAliasGraph.getPointsToInfo
		    (I->getOperand(i)).isHeap()) &&
		  !(PointerAliasGraph.getPointsToInfo
		    (I->getOperand(i)).isStruct()) &&
		  !(PointerAliasGraph.getPointsToInfo
		    (I->getOperand(i)).isDummy()) &&
		  !(PointerAliasGraph.getPointsToInfo
		    (I->getOperand(i)).isArray())) {
		WT = checkIfStored(BB, I->getOperand(i), LocalStoresSoFar);
		if (WT == UninitPointer)
		  WarningsList += "Pointer value argument to function call potentially uninitialized \n";
		else
		  WarningsList += WarningString(WT);
		if (WT != NoWarning)
		  WarningFlag = true;
	      }
	    }
	  }
      }
      else if (isa<ReturnInst>(I)) {
	if (I->getNumOperands() > 0)
	  if (I->getOperand(0)->getType()->getTypeID() ==
	      Type::PointerTyID) {
	    if (!isa<GlobalValue>(I->getOperand(0)) && 
		!(PointerAliasGraph.getPointsToInfo
		  (I->getOperand(0)).isGlobal())) {
	      WarningsList += "Pointer value being returned by function does not point to a global value (only intra-procedural region analysis done)\n";
	      WarningFlag = true;
	    }
	  }
      }
    }
  }
  return WarningFlag;
}

namespace {
  
  // The Pass class we implement
  struct CZeroPtrChecks : public FunctionPass {
    static char ID;
    CZeroPtrChecks () : FunctionPass ((intptr_t)(&ID)) {}
    const char *getPassName() const { return "CZero security pass"; }
    
    virtual bool runOnFunction (Function &F) {
      bool Error = false;
      DominatorTree *DomTree = &getAnalysis<DominatorTree>();  
      CZeroInfo *CZI = new CZeroInfo(F, DomTree);
      std::cerr << "\nIn function " << F.getName() << "\n";
      if (CZI->getWarnings() != "") {
	std::cerr << "Security Warning/s: \n";
	std::cerr << CZI->getWarnings();
	Error = true;
      }

      delete CZI;
      
      return false;
      
    }
    
    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      // TODO: Check when we generate code.
      AU.setPreservesAll();
      AU.addRequired<DominatorTree>();
    }
    
  };

  char CZeroPtrChecks::ID = 0;
  
  RegisterPass<CZeroPtrChecks> X("czeroptrchecks", "CZero Pointer Checks");
  
}

//Externally visible

Pass *createCZeroUninitPtrPass() { 
  return new CZeroPtrChecks(); 
}
