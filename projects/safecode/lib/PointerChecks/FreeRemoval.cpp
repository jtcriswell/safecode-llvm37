//===-- FreeRemoval.cpp - EmbeC transformation that removes frees ------------//
// 
//                          The SAFECode Compiler
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// FIXME:
//  This pass needs to be cleaned up and better understood.  Some of the
//  functionality seems to be addressed with poolcheckalign() in the Check
//  Insertion pass; we should ensure that the functionality there is present in
//  mainline and supercedes what is implemented here.  Also, the checking of
//  pool operations should be understood and updated/corrected if needed.
//
// This pass appears to do two things:
//
//  o) It ensures that there are load/store checks on pointers that point to
//     type-known data but are loaded from type-unknown partitions.
//
//  o) It seems to perform some sort of sanity/correctness checking of pool
//     creation/destruction.
//
//===----------------------------------------------------------------------===//
//
// Original comment from initial implementation:
//
// Implementation of FreeRemoval.h : an EmbeC pass
// 
// Some assumptions:
// * Correctness of pool allocation
// * Destroys at end of functions.
// Pool pointer aliasing assumptions
// -pool pointer copies via gep's are removed
// -no phinode takes two pool pointers because then they would be the same pool
// Result: If we look at pool pointer defs and look for their uses... we check 
// that their only uses are calls to pool_allocs, pool_frees and pool_destroys.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "FreeRemoval"

#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Instructions.h"
#include "SafeDynMemAlloc.h"
#include "SCUtils.h"
#include <iostream>
using namespace llvm;


namespace llvm {
  RegisterPass<EmbeCFreeRemoval> Y("EmbeC", "EmbeC pass that removes all frees and issues warnings if behaviour has changed");
  
char EmbeCFreeRemoval::ID = 0;

// Check if SSA pool pointer variable V has uses other than alloc, free and 
// destroy
void
EmbeCFreeRemoval::checkPoolSSAVarUses(Function *F, Value *V, 
                         map<Value *, set<Instruction *> > &FuncPoolAllocs,
                         map<Value *, set<Instruction *> > &FuncPoolFrees, 
                         map<Value *, set<Instruction *> > &FuncPoolDestroys) {
  if (V->use_begin() != V->use_end()) {
    for (Value::use_iterator UI = V->use_begin(), UE = V->use_end();
         UI != UE; ++UI) {
      // Check that the use is nothing except a call to pool_alloc, pool_free
      // or pool_destroy
      if (Instruction *I = dyn_cast<Instruction>(*UI)) {
        // For global pools, we need to check that only uses within the
        // function under consideration are checked.
        if (I->getParent()->getParent() != F)
          continue;
      } else {
        continue;
      }

      if (CallInst *CI = dyn_cast<CallInst>(*UI)) {
        if (Function *calledF = dyn_cast<Function>(CI->getOperand(0))) {
          if (calledF == F) {
            int operandNo = 0;
            for (unsigned int i = 1; i < CI->getNumOperands(); i++)
              if (CI->getOperand(i) == V) {
                operandNo = i;
                break;
              }
            
            Value *formalParam = 0;
            int opi = 0;
            for (Function::arg_iterator I = calledF->arg_begin(), 
                                        E = calledF->arg_end();
                  I != E && opi < operandNo; ++I, ++opi)
              if (opi == operandNo - 1) 
                formalParam = I;

            if (formalParam == V) {
              ;
            } else {
              std::cerr << "EmbeC: " << F->getName() 
                << ": Recursion not supported for case classification\n";
              continue;
            }
          }

          if (!calledF->isDeclaration()) {
            // the pool pointer is passed to the called function
      
            // Find the formal parameter corresponding to the parameter V
            int operandNo = 0;
            unsigned int limit = CI->getNumOperands();
            for (unsigned int i = 1; i < limit; i++)
              if (CI->getOperand(i) == V) {
                operandNo = i;
                break;
              }
      
            Value *formalParam;
            int opi = 0;
            for (Function::arg_iterator I = calledF->arg_begin(), 
                                        E = calledF->arg_end();
                 I != E && opi < operandNo; ++I, ++opi)
              if (opi == operandNo - 1) 
                formalParam = I;
      
            // if the called function has undestroyed frees in pool formalParam
            if (FuncFreedPools[calledF].find(formalParam) != 
                FuncFreedPools[calledF].end() && 
                FuncDestroyedPools[calledF].find(formalParam) == 
                FuncDestroyedPools[calledF].end()) {
              FuncPoolFrees[V].insert(cast<Instruction>(*UI));
            }

            // if the called function has undestroyed allocs in formalParam
            if (FuncAllocedPools[calledF].find(formalParam) != 
                FuncAllocedPools[calledF].end()) {
              FuncPoolAllocs[V].insert(cast<Instruction>(*UI));
            }
      
            // if the called function has a destroy in formalParam
            if (FuncDestroyedPools[calledF].find(formalParam) != 
                FuncDestroyedPools[calledF].end()) {
              FuncPoolDestroys[V].insert(cast<Instruction>(*UI));
            }
          } else {
            // external function
            if (calledF->getName() == PoolI) {
              // Insert call to poolmakeunfreeable after every poolinit since 
              // we do not free memory to the system for safety in all cases.
              //CallInst::Create(PoolMakeUnfreeable, make_vector(V, 0), "", 
              //             CI->getNext()); //taken care of in runtime library
              moduleChanged = true;
            } else if (calledF->getName() == PoolA) {
              FuncPoolAllocs[V].insert(cast<Instruction>(*UI));
            } else if (calledF->getName() == PoolF) {
              FuncPoolFrees[V].insert(cast<Instruction>(*UI));
            } else if (calledF->getName() == PoolD) {
              FuncPoolDestroys[V].insert(cast<Instruction>(*UI));
            } else if (calledF->getName() == PoolMUF) {
              // Ignore
            } else if (calledF->getName() == PoolCh) {
              // Ignore
            } else if (calledF->getName() == PoolAA) {
                BasicBlock::iterator it(CI);
                FuncPoolAllocs[V].insert(cast<Instruction>(++it));
            } else {
              hasError = true;
              std::cerr << "EmbeC: " << F->getName() << ": Unrecognized pool variable use \n";
              //        abort();
            }
          }
        } else {
          // Find the formal parameter corresponding to the parameter V
          int operandNo = 0;
          for (unsigned int i = 1; i < CI->getNumOperands(); i++)
            if (CI->getOperand(i) == V)
              operandNo = i;

          EQTDDataStructures::callee_iterator CalleesI =
            PoolInfo->callee_begin(CI), CalleesE = PoolInfo->callee_end(CI);

          for (; CalleesI != CalleesE; ++CalleesI) {
            Function *calledF = (Function *)(*CalleesI);
            
            PoolInfo->getFuncInfoOrClone(*calledF);
            
            /*
              if (PAFI->PoolArgFirst == PAFI->PoolArgLast ||
              operandNo-1 < PAFI->PoolArgFirst ||
              operandNo-1 >= PAFI->PoolArgLast)
              continue;
            */

            Value *formalParam;
            int opi = 0;
            for (Function::arg_iterator I = calledF->arg_begin(), 
                                        E = calledF->arg_end();
                 I != E && opi < operandNo; ++I, ++opi)
              if (opi == operandNo-1) 
                formalParam = I;
            
            // if the called function has undestroyed frees in pool formalParam
            if (FuncFreedPools[calledF].find(formalParam) != 
                FuncFreedPools[calledF].end() && 
                FuncDestroyedPools[calledF].find(formalParam) == 
                FuncDestroyedPools[calledF].end()) {
              FuncPoolFrees[V].insert(cast<Instruction>(*UI));
            }

            // if the called function has undestroyed allocs in formalParam
            if (FuncAllocedPools[calledF].find(formalParam) != 
                FuncAllocedPools[calledF].end()) {
              FuncPoolAllocs[V].insert(cast<Instruction>(*UI));
            }
            
            // if the called function has a destroy in formalParam
            if (FuncDestroyedPools[calledF].find(formalParam) != 
                FuncDestroyedPools[calledF].end()) {
              FuncPoolDestroys[V].insert(cast<Instruction>(*UI));
            }
          }
        }
      } else {
        hasError = true;
        std::cerr << "EmbeC: " << F->getName() << ": Unrecognized pool variable use \n";
      }   
    }
  }
}

// Propagate that the pool V is a collapsed pool to each of the callees of F
// that have V as argument
void
EmbeCFreeRemoval::propagateCollapsedInfo (Function *F, Value *V) {
  for (Value::use_iterator UI = V->use_begin(), UE = V->use_end();
       UI != UE; ++UI) {
    if (CallInst *CI = dyn_cast<CallInst>(*UI)) {
      if (Function *calledF = dyn_cast<Function>(CI->getOperand(0))) {
        if (calledF == F) {
          // Quick check for the common case
          // Find the formal parameter corresponding to the parameter V
          int operandNo = 0;
          for (unsigned int i = 1; i < CI->getNumOperands(); i++)
            if (CI->getOperand(i) == V) {
              operandNo = i;
              break;
            }
          
          Value *formalParam;
          int opi = 0;
          for (Function::arg_iterator I = calledF->arg_begin(), 
                                      E = calledF->arg_end();
               I != E && opi < operandNo; ++I, ++opi)
            if (opi == operandNo - 1) 
              formalParam = I; 

          if (formalParam == V) {
            // This is the common case.
          } else {
            std::cerr << "EmbeC: " << F->getName() 
              << ": Recursion not supported\n";
            abort();
            continue;
          }
        }

        if (!calledF->isDeclaration()) {
          // the pool pointer is passed to the called function
    
          // Find the formal parameter corresponding to the parameter V
          int operandNo = 0;
          for (unsigned int i = 1; i < CI->getNumOperands(); i++)
            if (CI->getOperand(i) == V) {
              operandNo = i;
              break;
            }
          
          Value *formalParam;
          int opi = 0;
          for (Function::arg_iterator I = calledF->arg_begin(), 
           E = calledF->arg_end();
               I != E && opi < operandNo; ++I, ++opi)
            if (opi == operandNo - 1) 
              formalParam = I;
          
          CollapsedPoolPtrs[calledF].insert(formalParam);
        } 
      } else {
        // indirect function call
  
        //  std::pair<EQTDDataStructures::ActualCalleesTy::const_iterator, EQTDDataStructures::ActualCalleesTy::const_iterator> Callees = AC.equal_range(CI);
  
        // Find the formal parameter corresponding to the parameter V
        int operandNo = 0;
        for (unsigned int i = 1; i < CI->getNumOperands(); i++)
          if (CI->getOperand(i) == V)
            operandNo = i;
        
        EQTDDataStructures::callee_iterator CalleesI =
          PoolInfo->callee_begin(CI),  CalleesE = PoolInfo->callee_end(CI);
        
        for (; CalleesI != CalleesE; ++CalleesI) {
          Function *calledF = (Function *)(*CalleesI);
          
          PoolInfo->getFuncInfoOrClone(*calledF);
          
          /*
            if (PAFI->PoolArgFirst == PAFI->PoolArgLast ||
            operandNo-1 < PAFI->PoolArgFirst ||
            operandNo-1 >= PAFI->PoolArgLast)
            continue;
          */
          
          Value *formalParam;
          int opi = 0;
          for (Function::arg_iterator I = calledF->arg_begin(), 
                                      E = calledF->arg_end();
               I != E && opi < operandNo; ++I, ++opi)
            if (opi == operandNo-1) 
              formalParam = I;

          CollapsedPoolPtrs[calledF].insert(formalParam);
        }
      }
    } 
  }
}

// Returns true if BB1 follows BB2 in some path in F
static bool
followsBlock(BasicBlock *BB1, BasicBlock *BB2, Function *F,
             set<BasicBlock *> visitedBlocks) {
  for (succ_iterator BBSI = succ_begin(BB2),
                     BBSE = succ_end(BB2); BBSI != BBSE; ++BBSI) {
    if (visitedBlocks.find(*BBSI) == visitedBlocks.end()) {
      if (*BBSI == BB1) {
        return true;
      } else {
        visitedBlocks.insert(*BBSI);
        if (followsBlock(BB1, *BBSI, F, visitedBlocks))
          return true;
      }
    }
  }

  return false;
}

#if 0
// Checks if Inst1 follows Inst2 in any path in the function F.
static bool followsInst(Instruction *Inst1, Instruction *Inst2, Function *F) {
  if (Inst1->getParent() == Inst2->getParent()) {
    for (BasicBlock::iterator BBI = Inst2, BBE = Inst2->getParent()->end();
   BBI != BBE; ++BBI)
      if (Inst1 == &(*BBI))
  return true;
  }
  set<BasicBlock *> visitedBlocks;
  return followsBlock(Inst1->getParent(), Inst2->getParent(), F, 
          visitedBlocks);
}
#endif

#if 0
static void printSets(set<Value *> &FuncPoolPtrs,
          map<Value *, set<Instruction *> > &FuncPoolFrees,
          map<Value *, set<Instruction *> > &FuncPoolAllocs) {
  for (set<Value *>::iterator I = FuncPoolPtrs.begin(), E = FuncPoolPtrs.end();
       I != E; ++I) {
    std::cerr << "Pool Variable: " << *I << "\n";
    if (FuncPoolFrees[*I].size()) {
      std::cerr << "Frees :" << "\n";
      for (set<Instruction *>::iterator FreeI = 
       FuncPoolFrees[*I].begin(), FreeE = FuncPoolFrees[*I].end();
     FreeI != FreeE; ++FreeI) {
  CallInst *CInst = dyn_cast<CallInst>(*FreeI);
  Function *CIF = dyn_cast<Function>(CInst->getOperand(0));
  std::cerr << CIF->getName() << "\n";
      }
    }
    if (FuncPoolAllocs[*I].size()) {
      std::cerr << "Allocs :" << "\n";
      for (set<Instruction *>::iterator AllocI = 
       FuncPoolAllocs[*I].begin(), AllocE = FuncPoolAllocs[*I].end();
     AllocI != AllocE; ++AllocI) {  
  CallInst *CInst = dyn_cast<CallInst>(*AllocI);
  Function *CIF = dyn_cast<Function>(CInst->getOperand(0));
  std::cerr << CIF->getName() << "\n";  
      }
    }
  }
}
#endif

DSNode *
EmbeCFreeRemoval::guessDSNode (Value *v, DSGraph &G, PA::FuncInfo *PAFI) {
  if (std::find(Visited.begin(), Visited.end(), v) != Visited.end())
    return 0;
  Visited.push_back(v);
  if (isa<PointerType>(v->getType())) {
    DSNode *r = G.getNodeForValue(v).getNode();
    if (r && PAFI->PoolDescriptors.count(r))
      return r;
  }
  DSNode *retDSNode = 0;
  if (BinaryOperator *Bop = dyn_cast<BinaryOperator>(v)) {
    retDSNode = guessDSNode(Bop->getOperand(0), G, PAFI);
    if (!retDSNode) retDSNode = guessDSNode(Bop->getOperand(0), G, PAFI);
  } else if (CastInst *CI = dyn_cast<CastInst>(v)) {
    retDSNode = guessDSNode(CI->getOperand(0), G, PAFI);
  } else if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(v)) {
    retDSNode = guessDSNode(GEP->getPointerOperand(), G, PAFI);
  } else if (LoadInst *LI = dyn_cast<LoadInst>(v)) {
    //hope its collapsed node ...
    retDSNode = guessDSNode(LI->getOperand(0), G, PAFI);
  } else if (PHINode* PN =dyn_cast<PHINode>(v)) {
    for (unsigned idx = 0; idx < PN->getNumIncomingValues(); ++idx) {
      if (!retDSNode) {
        retDSNode = guessDSNode(PN->getIncomingValue(idx), G, PAFI);
      } else {
        break;
      }
    }
  } else if (CallInst* CI =dyn_cast<CallInst>(v)) {
    for (unsigned idx = 1; idx < CI->getNumOperands(); ++idx) {
      if (!retDSNode) {
        retDSNode = guessDSNode(CI->getOperand(idx), G, PAFI);
      } else {
        break;
      }
    }
  }
  return retDSNode;
}

//
// Disabled guessPoolPtrAndInsertCheck(); no one seems to use it right now.
//
#if 0
  void EmbeCFreeRemoval::guessPoolPtrAndInsertCheck(PA::FuncInfo *PAFI, Value *oldI, Instruction  *I, Value *pOpI, DSGraph *oldG) {
    Visited.clear();
    //follow up v through the ssa def0use chains
    DSNode *DSN = guessDSNode(oldI, oldG, PAFI);
    //    assert(DSN && "can not guess the pool ptr");
    //    assert(PAFI->PoolDescriptors.count(DSN) && "no pool descriptor found \n");
    CastInst *CastI = 
      new CastInst(pOpI, 
       PointerType::getUnqual(Type::Int8Ty), "casted", I);
    if (DSN) {

    CallInst::Create(PoolCheck, 
     make_vector(PAFI->PoolDescriptors[DSN], CastI, 0),
     "", I);
    } else {
      Type *VoidPtrTy = PointerType::getUnqual(Type::Int8Ty); 
      const Type *PoolDescType = 
  ArrayType::get(VoidPtrTy, 50);

      const PointerType *PoolDescPtr = PointerType::getUnqual(PoolDescType);      
      Value *PH = Constant::getNullValue(PoolDescPtr);
      CallInst::Create(PoolCheck, 
     make_vector(PH, CastI, 0),
     "", I);
      
    }
    DEBUG(std::cerr << "inserted a pool check for unknown node \n");

  }
#endif
  
void
EmbeCFreeRemoval::insertNonCollapsedChecks (Function *Forig, Function *F,
                                            DSNode *DSN) {
  assert(!DSN->isNodeCompletelyFolded() && "its collapsed! \n");
  if (DSN->isUnknownNode()) return; //we'll handle it separately
  //Assuming alignment is the beginning of a node, owise its runtime failure
  bool isClonedFunc;
  PA::FuncInfo* PAFI = PoolInfo->getFuncInfoOrClone(*F);
  if (PoolInfo->getFuncInfo(*F))
    isClonedFunc = false;
  else
    isClonedFunc = true;
  
  DSGraph* oldG = PoolInfo->getDSGraph(*Forig);
  
  // For each scalar pointer in the original function
  for (DSGraph::ScalarMapTy::iterator SMI = oldG->getScalarMap().begin(), 
   SME = oldG->getScalarMap().end(); SMI != SME; ++SMI) {
    DSNodeHandle &GH = SMI->second;

    //We need to insert checks to all the uses of this ptr
    if (DSN == GH.getNode()) {
      // We are any way checking all arrays
      if ((GH.getOffset()) && (DSN->isArray())) return;
      Value * Offset = getGlobalContext().getConstantInt(Type::Int32Ty, GH.getOffset());

      Value *NewPtr = (Value *)(SMI->first);
      if (isClonedFunc) {
        NewPtr = PAFI->ValueMap[SMI->first];
      }
      if (!NewPtr)
        continue;

      const Type *VoidPtrTy = PointerType::getUnqual(Type::Int8Ty);

      for (Value::use_iterator UI = NewPtr->use_begin(), UE = NewPtr->use_end();
           UI != UE; ++UI) {
        // If the use is the 2nd operand of store, insert a runtime check
        if (StoreInst *StI = dyn_cast<StoreInst>(*UI)) {
          if (StI->getOperand(1) == NewPtr) {
            moduleChanged = true;
            Value * CastPH = castTo(PAFI->PoolDescriptors[DSN], VoidPtrTy, "", StI);
            Value * CastI  = castTo(StI->getOperand(1), VoidPtrTy, "casted", StI);
            std::vector<Value *> args = make_vector(CastPH, CastI, Offset, 0);
            CallInst::Create(PoolCheck, args.begin(), args.end(), "", StI);
            std::cerr << " inserted poolcheck for noncollapsed pool\n";
          }
        } else if (CallInst *CallI = dyn_cast<CallInst>(*UI)) {
          // If this is a function pointer read from a collapsed node,
          // reject the code
          if (CallI->getOperand(0) == NewPtr) {
            std::cerr << 
              "EmbeC: Error - Function pointer read from collapsed node\n";
            abort();
          }
        } else if (LoadInst *LdI = dyn_cast<LoadInst>(*UI)) {
          if (LdI->getOperand(0) == NewPtr) {
            moduleChanged = true;
            Value *CastPH = castTo (PAFI->PoolDescriptors[DSN], VoidPtrTy, "", LdI);
            Value *CastI  = castTo (LdI->getOperand(0), VoidPtrTy, "casted", LdI);
            std::vector<Value *> args = make_vector(CastPH, CastI, Offset, 0);
            CallInst::Create(PoolCheck, args.begin(), args.end(), "", LdI);
            std::cerr << " inserted poolcheck for noncollpased pool\n";
          }
        }
      }
    }
  }
}

// Insert runtime checks. Called on the functions in the existing program
void
EmbeCFreeRemoval::addRuntimeChecks(Function *F, Function *Forig) {
  //The  following code is phased out, a newer version is insert.cpp 
  
#if 0
  bool isClonedFunc;
  PA::FuncInfo* PAFI = PoolInfo->getFuncInfoOrClone(*F);

  if (PoolInfo->getFuncInfo(*F))
    isClonedFunc = false;
  else
    isClonedFunc = true;
  
  if (!PAFI->PoolDescriptors.empty()) {
  // For each scalar pointer in the original function
    for (DSGraph::ScalarMapTy::iterator SMI = oldG->getScalarMap().begin(), 
     SME = oldG->getScalarMap().end(); SMI != SME; ++SMI) {
      DSNodeHandle &GH = SMI->second;
      DSNode *DSN = GH.getNode();
      if (!DSN) 
  continue;
      if (DSN->isUnknownNode()) {
  // Report an error if we see loads or stores on the pointer
  Value *NewPtr = SMI->first;
  if (isClonedFunc) {
    if (PAFI->ValueMap.count(SMI->first))
      NewPtr = PAFI->ValueMap[SMI->first];
    else continue;
  }
  if (!NewPtr)
    continue;
  for (Value::use_iterator UI = NewPtr->use_begin(), 
         UE = NewPtr->use_end(); UI != UE; ++UI) {
    if (StoreInst *StI = dyn_cast<StoreInst>(*UI)) {
      if (StI->getOperand(1) == NewPtr) {
        guessPoolPtrAndInsertCheck(PAFI, SMI->first, StI, NewPtr, oldG);
        std::cerr << 
    "EmbeC: In function " << F->getName() << ": Presence of an unknown node can invalidate pool allocation\n";
        break;
      }
    } else if (LoadInst *LI = dyn_cast<LoadInst>(*UI)) {
      //We'll try to guess a pool descriptor and insert a check
      //if it fails then ok too bad ;)
      //Add guess the pool handle
      guessPoolPtrAndInsertCheck(PAFI, SMI->first, LI, NewPtr, oldG);
      std::cerr << 
        "EmbeC: In function " << F->getName() << ": Presence of an unknown node can invalidate pool allocation\n";
      break;
    } else if (CallInst *CallI = dyn_cast<CallInst>(*UI)) {
        // If this is a function pointer read from a collapsed node,
        // reject the code
        if (CallI->getOperand(0) == NewPtr) {
    std::cerr << 
      "EmbeC: Error - Function pointer read from Unknown node\n";
    abort();
    
        }
    }
  }
      }
      if (PAFI->PoolDescriptors.count(DSN)) {
  // If the node pointed to, corresponds to a collapsed pool 
  if (CollapsedPoolPtrs[F].find(PAFI->PoolDescriptors[DSN]) !=
      CollapsedPoolPtrs[F].end()) {
    // find uses of the coresponding new pointer
    Value *NewPtr = SMI->first;
    if (isClonedFunc) {
      if (PAFI->ValueMap.count(SMI->first)) {
        NewPtr = PAFI->ValueMap[SMI->first];
        if (!PAFI->NewToOldValueMap.count(NewPtr)) {
    std::cerr << "WARNING : checks for NewPtr are not inserted\n";
    abort();
    continue;
        }
      } else {
        std::cerr << "WARNING : checks for NewPtr are not inserted\n";
        abort();
        continue;
      }
    }
    if (!NewPtr)
      continue;
    for (Value::use_iterator UI = NewPtr->use_begin(), 
     UE = NewPtr->use_end(); UI != UE; ++UI) {
      // If the use is the 2nd operand of store, insert a runtime check
      if (StoreInst *StI = dyn_cast<StoreInst>(*UI)) {
        if (StI->getOperand(1) == NewPtr) {
    if (!isa<GlobalVariable>(StI->getOperand(1))) { 
      moduleChanged = true;
      CastInst *CastI = 
        new CastInst(StI->getOperand(1), 
         PointerType::getUnqual(Type::Int8Ty), "casted", StI);
      CallInst::Create(PoolCheck, 
             make_vector(PAFI->PoolDescriptors[DSN], CastI, 0),
             "", StI);
      DEBUG(std::cerr << " inserted poolcheck for collpased pool\n";);
    } else {
      std::cerr << "WARNING DID not insert a check for collapsed global store";
    }
        }
      } else if (CallInst *CallI = dyn_cast<CallInst>(*UI)) {
        // If this is a function pointer read from a collapsed node,
        // reject the code
        if (CallI->getOperand(0) == NewPtr) {
    std::cerr << 
      "EmbeC: Error - Function pointer read from collapsed node\n";
    abort();
        }
      } else if (LoadInst *LdI = dyn_cast<LoadInst>(*UI)) {
        if (LdI->getOperand(0) == NewPtr) {
    if (!isa<GlobalVariable>(LdI->getOperand(0))) { 
      moduleChanged = true;
      CastInst *CastI = 
        new CastInst(LdI->getOperand(0), 
         PointerType::getUnqual(Type::Int8Ty), "casted", LdI);
      CallInst::Create(PoolCheck, 
           make_vector(PAFI->PoolDescriptors[DSN], CastI, 0),
             "", LdI);
      std::cerr << " inserted poolcheck for collpased pool\n";
    } else {
      std::cerr << "WARNING DID not insert a check for collapsed global load";
    }
        }
      }
    }
  }
      }
    }
  }
#endif  
}

bool
EmbeCFreeRemoval::runOnModule(Module &M) {
#if 1
  //
  // FIXME:
  //  Currently, we do not need this pass.  However, removing it from the sc
  //  tool causes other passes to fail for reasons unknown.  So, for now, leave
  //  this pass in the sc tool, but don't let it do anything.
  //
  std::cerr << "WARNING: EmbeCFreeRemoval Pass Executed, but it does NOTHING!"
            << std::endl;
  return false;
#endif
  CurModule = &M;
  moduleChanged = false;
  hasError = false;
  // Insert prototypes in the module
  // NB: This has to be in sync with the types in PoolAllocate.cpp
  const Type *VoidPtrTy = PointerType::getUnqual(Type::Int8Ty);

  const Type *PoolDescType = 
    //    StructType::get(make_vector<const Type*>(VoidPtrTy, VoidPtrTy, 
    //               Type::Int32Ty, Type::Int32Ty, 0));
    ArrayType::get(VoidPtrTy, 50);

  const PointerType *PoolDescPtr = PointerType::getUnqual(PoolDescType);
  FunctionType *PoolMakeUnfreeableTy = 
    FunctionType::get(Type::VoidTy,
          make_vector<const Type*>(PoolDescPtr, 0),
          false);

  FunctionType *PoolCheckTy = 
    FunctionType::get(Type::VoidTy,
          make_vector<const Type*>(VoidPtrTy, VoidPtrTy, Type::Int32Ty, 0),
          false);
  
  PoolMakeUnfreeable = CurModule->getOrInsertFunction("poolmakeunfreeable", 
                  PoolMakeUnfreeableTy);
  
  PoolCheck = CurModule->getOrInsertFunction("poolcheckalign", PoolCheckTy);
  
  moduleChanged = true;

  Function *mainF = M.getFunction("main");
  
  if (!mainF) {
    mainF = M.getFunction("MAIN__");
    if (!mainF) {
      hasError = true;
      std::cerr << "EmbeC: Function main required\n";
      return false;
    }
  }

  // Bottom up on the call graph
  // TODO: Take care of recursion/mutual recursion
  PoolInfo = &getAnalysis<PoolAllocateGroup>();
  assert (PoolInfo && "Must run Pool Allocation Pass first!\n");
  CallGraph &CG = getAnalysis<CallGraph>();
  //  BUDS = &getAnalysis<EQTDDataStructures>();
  //  BUDS = PoolInfo->getDataStructures();
  //  TDDS = &getAnalysis<TDDataStructures>();
  // For each function, all its pool SSA variables including its arguments
  map<Function *, set<Value *> > FuncPoolPtrs;

  for (po_iterator<CallGraph*> CGI = po_begin(&CG), 
                               CGE = po_end(&CG); CGI != CGE; ++CGI) {
    Function *F = (*CGI)->getFunction();
    
    // Ignore nodes representing external functions in the call graph
    if (!F)
      continue;
    
    // Pool SSA variables that are used in allocs, destroy and free or calls 
    // to functions that escaped allocs, destroys and frees respectively.
    map<Value *, set<Instruction *> > FuncPoolAllocs, FuncPoolFrees, 
                                      FuncPoolDestroys;
    
    // Traverse the function finding poolfrees and calls to functions that
    // have poolfrees without pooldestroys on all paths in that function.
    
    if (!F->isDeclaration()) {
      // For each pool pointer def check its uses and ensure that there are 
      // no uses other than the pool_alloc, pool_free or pool_destroy calls
      
      PA::FuncInfo* PAFI = PoolInfo->getFuncInfoOrClone(*F);
      
      // If the function has no pool pointers (args or SSA), ignore the 
      // function.
      if (!PAFI) continue;
      
      if (PAFI->Clone && PAFI->Clone != F) continue;

      if (!PAFI->PoolDescriptors.empty()) {
        for (std::map<const DSNode*, Value*>::iterator PoolDI = 
         PAFI->PoolDescriptors.begin(), PoolDE = 
         PAFI->PoolDescriptors.end(); PoolDI != PoolDE; ++PoolDI) {
          checkPoolSSAVarUses(F, PoolDI->second, FuncPoolAllocs, 
          FuncPoolFrees, FuncPoolDestroys);
          FuncPoolPtrs[F].insert(PoolDI->second);
        }
      }
      else
        continue;

      /*
      if (F->getName() == "main") {
  std::cerr << "In Function " << F->getName() << "\n";
  printSets(FuncPoolPtrs[F], FuncPoolFrees, FuncPoolAllocs);
      }
      */

      /*
      // Implementing the global analysis that checks that
      // - For each poolfree, if there is a poolallocate on another pool
      //   before a pooldestroy on any path, then this is a case 3 pool.
      //   If there is a poolallocate on the same pool before a pooldestroy
      //   on any path, then it is a case 2 pool.
     
      // Do a local analysis on these and update the pool variables that escape
      // the function (poolfrees without pooldestroys, poolallocs)
      // For each instruction in the list of free/calls to free, do
      // the following: go down to the bottom of the function looking for 
      // allocs till you see destroy. If you don't see destroy on some path, 
      // update escape list.
      // Update escape list with allocs without destroys for arguments
      // as well as arguments that are destroyed.
      // TODO: Modify implementation so you are not checking that there is no
      // alloc to other pools follows a free in the function, but to see that 
      // there is a destroy on pool between a free and an alloc to another pool
      // Current Assumption: destroys are at the end of a function.
      if (!FuncPoolFrees.empty()) {
  std::cerr << "In Function " << F->getName() << "\n";
  for (map<Value *, set<Instruction *> >::iterator ValueI = 
         FuncPoolFrees.begin(), ValueE = FuncPoolFrees.end();
       ValueI != ValueE; ++ValueI) {
    bool case3found = false, case2found = false;
    for (set<Instruction *>::iterator FreeInstI = 
     (*ValueI).second.begin(), 
     FreeInstE = (*ValueI).second.end();
         FreeInstI != FreeInstE && !case3found; ++FreeInstI) {
      // For each free instruction or call to a function which escapes
      // a free in pool (*ValueI).first
      for (set<Value *>::iterator PoolPtrsI = FuncPoolPtrs[F].begin(), 
       PoolPtrsE = FuncPoolPtrs[F].end(); 
     PoolPtrsI != PoolPtrsE && !case3found; 
     ++PoolPtrsI) {
        // For each pool pointer other than the one in the free 
        // instruction under consideration, check that allocs to it
        // don't follow the free on any path.
        if (*PoolPtrsI !=  (*ValueI).first) {
    map<Value *, set<Instruction *> >::iterator AllocSet= 
      FuncPoolAllocs.find(*PoolPtrsI);
    if (AllocSet != FuncPoolAllocs.end())
      for (set<Instruction *>::iterator AllocInstI = 
       (*AllocSet).second.begin(), 
       AllocInstE = (*AllocSet).second.end();
           AllocInstI != AllocInstE && !case3found; ++AllocInstI)
        if (followsInst(*AllocInstI, *FreeInstI, F) &&
      *AllocInstI != *FreeInstI)
          case3found = true;
        } else {
    map<Value *, set<Instruction *> >::iterator AllocSet= 
      FuncPoolAllocs.find(*PoolPtrsI);
    if (AllocSet != FuncPoolAllocs.end())
      for (set<Instruction *>::iterator AllocInstI = 
       (*AllocSet).second.begin(), 
       AllocInstE = (*AllocSet).second.end();
           AllocInstI != AllocInstE && !case3found; ++AllocInstI)
        if (followsInst(*AllocInstI, *FreeInstI, F) &&
      *AllocInstI != *FreeInstI)
          case2found = true;
        }
      }
    }
    if (case3found && case2found)
      std::cerr << (*ValueI).first->getName() 
          << ": Case 2 and 3 detected\n";
    else if (case3found)
      std::cerr << (*ValueI).first->getName() 
          << ": Case 3 detected\n";
    else if (case2found)
      std::cerr << (*ValueI).first->getName() 
          << ": Case 2 detected\n";
    else
      std::cerr << (*ValueI).first->getName() 
          << ": Case 1 detected\n";
  }
      }
      */
      
      // Assumption: if we have pool_destroy on a pool in a function, then it
      // is on all exit paths of the function
      // TODO: correct later. 
      // Therefore, all pool ptr arguments that have frees but no destroys
      // escape the function. Similarly all pool ptr arguments that have
      // allocs but no destroys escape the function      
      for (set<Value *>::iterator PoolPtrsI = FuncPoolPtrs[F].begin(), 
       PoolPtrsE = FuncPoolPtrs[F].end(); PoolPtrsI != PoolPtrsE; 
     ++PoolPtrsI) {
        if (isa<Argument>(*PoolPtrsI)) {
          // Only for pool pointers that are arguments
          if (FuncPoolFrees.find(*PoolPtrsI) != FuncPoolFrees.end() &&
              FuncPoolFrees[*PoolPtrsI].size())
            FuncFreedPools[F].insert(*PoolPtrsI);
          if (FuncPoolAllocs.find(*PoolPtrsI) != FuncPoolAllocs.end() &&
              FuncPoolAllocs[*PoolPtrsI].size())
            FuncAllocedPools[F].insert(*PoolPtrsI);
          if (FuncPoolDestroys.find(*PoolPtrsI) != FuncPoolDestroys.end() &&
              FuncPoolDestroys[*PoolPtrsI].size()) {
            FuncDestroyedPools[F].insert(*PoolPtrsI);
          }
        } 
      }
      
      // TODO
      // For each function, check that the frees in the function are case 1 
      // i.e. there are no mallocs between the free and its corresponding 
      // pool_destroy and then remove the pool free call.
    }
  }
    
  // Now, traverse the call graph top-down, updating information about pool 
  // pointers that may be collapsed and inserting runtime checks
  ReversePostOrderTraversal<CallGraph*> RPOT(&CG); 
  for (ReversePostOrderTraversal<CallGraph*>::rpo_iterator CGI = RPOT.begin(),
   CGE = RPOT.end(); CGI != CGE; ++CGI) {
    Function *F = (*CGI)->getFunction();
    
    if (!F)
      continue;
    
    // Ignore nodes representing external functions in the call graph
    if (!F->isDeclaration()) {
      // For each pool pointer def check its uses and ensure that there are 
      // no uses other than the pool_alloc, pool_free or pool_destroy calls
      
      PA::FuncInfo* PAFI = PoolInfo->getFuncInfoOrClone(*F);

      if (!PAFI) continue;

      if (PAFI->Clone && PAFI->Clone != F) continue;

      Function *Forig;
      if (PAFI->Clone) {
        for(Module::iterator MI = M.begin(), ME = M.end(); MI != ME; ++MI)
          if (PoolInfo->getFuncInfo(*MI))
            if (PoolInfo->getFuncInfo(*MI) == PAFI) {
              Forig = &*MI;
              break;
            }
      } else
        Forig = F;

      if (FuncPoolPtrs.count(F)) {
        for (set<Value *>::iterator PDI = FuncPoolPtrs[F].begin(), 
                                    PDE = FuncPoolPtrs[F].end();
             PDI != PDE; ++PDI) {
          if (isa<Argument>(*PDI)) {
            if (CollapsedPoolPtrs[F].find(*PDI) != CollapsedPoolPtrs[F].end())
              propagateCollapsedInfo(F, *PDI);
          } else {
            // This pool is poolinit'ed in this function or is a global pool
            const DSNode *PDINode;
            
            for (std::map<const DSNode*, Value*>::iterator PDMI = 
                 PAFI->PoolDescriptors.begin(), 
                 PDME = PAFI->PoolDescriptors.end(); PDMI != PDME; ++PDMI)
              if (PDMI->second == *PDI) {
                PDINode = PDMI->first;
                break;
              }
      
            if (PDINode->isNodeCompletelyFolded()) {
              CollapsedPoolPtrs[F].insert(*PDI);
        
              for (unsigned i = 0 ; i < PDINode->getNumLinks(); ++i)
                if (PDINode->getLink(i).getNode())
                  if (!PDINode->getLink(i).getNode()->isNodeCompletelyFolded()){
                    //Collapsed to non-collapsed, so insert a check
#if 1
                    //
                    // FIXME:
                    //  We need to ensure that these checks are performed by
                    //  the check insertion pass.
                    //
                    insertNonCollapsedChecks (Forig, F,
                                              PDINode->getLink(i).getNode());
#endif
                    //        abort();
                    break;
                  }

              // Propagate this information to all the callees only if this
              // is not a global pool
              if (!isa<GlobalVariable>(*PDI))
                propagateCollapsedInfo(F, *PDI);
            }
          }
        }

        // At this point, we know all the collapsed pools in this function
        // Add run-time checks before all stores to pointers pointing to 
        // collapsed pools
        addRuntimeChecks(F, Forig);  
      }
    }
  }
  return moduleChanged;
}

}
Pass *createEmbeCFreeRemovalPass() { return new EmbeCFreeRemoval(); }

