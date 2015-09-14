//===- CodeDuplication.cpp: -----------------------------------------------===//
//
//                          The SAFECode Compiler
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file performs code duplication analysis and wraps codes into
// functions.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "code_duplication"
#include <set>
#include <iostream>

#include "llvm/Function.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/ADT/Statistic.h"

#include "safecode/VectorListHelper.h"
#include "safecode/CodeDuplication.h"
#include "SCUtils.h"

#if 0
static llvm::RegisterPass<llvm::CodeDuplicationAnalysis> sCodeDuplicationAnalysisPass("-code-dup-analysis", "Analysis for code duplication", false, false);

static llvm::RegisterPass<llvm::RemoveSelfLoopEdge> sRemoveSelfLoopEdgePass("-break-self-loop-edge", "Break all self-loop edges in basic blocks");

static llvm::RegisterPass<llvm::DuplicateCodeTransform> sDuplicateCodeTransformPass("-duplicate-code-transformation", "Duplicate codes for SAFECode checking");
#endif

static llvm::RegisterPass<llvm::DuplicateLoopAnalysis> sDuplicateLoopAnalysis("dup-loop-analysis", "Analysis for duplicating loop", false, false);

namespace {
  STATISTIC (DuplicatedLoop, "The number of loops are eligible for duplication");
}

namespace llvm {

  char CodeDuplicationAnalysis::ID = 0;

  /// Determine whether a basic block is eligible for code duplication
  /// Here are the criteria:
  ///
  /// 1. No call instructions (FIXME: what about internal function
  /// calls? )
  ///
  /// 2. Memory access patterns and control flows are memory
  /// indepdendent, i.e., the results of load instructions in the
  /// basic block cannot affect memory addresses and control flows.
  ///
  /// 3. Volative instructions(TODO: Implementation!)


  static bool isEligibleforCodeDuplication(BasicBlock * BB) {
    std::set<Instruction*> unsafeInstruction;
    for (BasicBlock::iterator I = BB->begin(), E = BB->end(); I != E; ++I) {
      if (isa<CallInst>(I)) {
	return false;
      }

      /*   } else if (isa<LoadInst>(I)) {
	/// Taint analysis for Load Instructions
	LoadInst * inst = dyn_cast<LoadInst>(I);
	unsafeInstruction.insert(inst);
	for (Value::use_iterator I = inst->use_begin(), E = inst->use_end(); I != E; ++ I) {
	  if (Instruction * in = dyn_cast<Instruction>(I)) {
	    if (in->getParent() != BB) {
	      break;
	    }
	    unsafeInstruction.insert(in);
	  }
	}
      }
    }

    /// Check GEP instructions
    for (BasicBlock::iterator I = BB->begin(), E = BB->end(); I != E; ++I) {
      if (isa<GetElementPtrInst>(I) && unsafeInstruction.count(I)) {
	return false;
      }
    }

    Instruction * termInst = BB->getTerminator();
    return unsafeInstruction.count(termInst) == 0;
      */
    }
      return true;
  }
  
  void CodeDuplicationAnalysis::calculateBBArgument(BasicBlock * BB, InputArgumentsTy & args) {
    for (BasicBlock::iterator I = BB->begin(), E = BB->end(); I != E; ++I) {
      Instruction * inst = &*I;

      // Add Phi node and load instructions into input arguments
      if (isa<PHINode>(inst) || isa<LoadInst>(inst)) {
	args.push_back(inst);
	continue;
      }

      /// 
      for (User::op_iterator op_it = inst->op_begin(), op_end = inst->op_end(); op_it != op_end; ++op_it) {
	Value * def = op_it->get();
	// Def is outside of the basic block
	Instruction * defInst = dyn_cast<Instruction>(def);
	if (defInst && defInst->getParent() != BB) {
	  args.push_back(defInst);
	}
      }
    }
  }

  bool CodeDuplicationAnalysis::runOnModule(Module & M) {
    InputArgumentsTy args;

    for (Module::iterator FI = M.begin(), FE = M.end(); FI != FE; ++FI) {
      for (Function::iterator BI = FI->begin(), BE = FI->end(); BI != BE; ++BI) {
	BasicBlock * BB = BI;
	args.clear();

	if (isEligibleforCodeDuplication(BB)) {
	  calculateBBArgument(BB, args);
	  mBlockInfo[BB] = args;
	  /*	  //	  BB->dump();
	  std::cerr << "=== args ===" << std::endl;
	  for (size_t i = 0; i < args.size(); ++i) {
	    //	    args[i]->getType()->dump();
	    //args[i]->dump();
	    std::cerr << std::endl;
	  }
	  std::cerr << "====" << std::endl;
	  */
	}
      }
    }
    return false;
  }
  
  bool CodeDuplicationAnalysis::doInitialization(Module & M) {
    mBlockInfo.clear();
    return false;
  }

  bool CodeDuplicationAnalysis::doFinalization(Module & M) {
    mBlockInfo.clear();
    return false;
  }

  ///
  /// RemoveSelfLoopEdge Methods
  ///

  char RemoveSelfLoopEdge::ID = 0;

  /// A a dummy basic block at the end of the input block to eliminate
  /// self-loop edges.
  static void removeBBSelfLoopEdge(BasicBlock * BB) {
    Instruction * inst = BB->getTerminator();
    BranchInst * branchInst = dyn_cast<BranchInst>(inst);
    BasicBlock * newEndBB = BasicBlock::Create(BB->getContext(),
                                               BB->getName() + ".self_loop_edge:");

    BranchInst::Create(BB, newEndBB);

    Function * F = BB->getParent();
    Function::iterator bbIt = BB;
    F->getBasicBlockList().insert(++bbIt, newEndBB);

    assert(branchInst && "the terminator of input basic block should be a branch instruction");

    for (User::op_iterator op_it = branchInst->op_begin(), op_end = branchInst->op_end(); op_it != op_end; ++op_it) {
      BasicBlock * bb = dyn_cast<BasicBlock>(op_it->get());
      if (BB == bb) {
	op_it->set(newEndBB);
      }
    }

    // Deal with PHI Node, from BreakCritcalEdges.cpp
    for (BasicBlock::iterator I = BB->begin(); isa<PHINode>(I); ++I) {
      PHINode *PN = cast<PHINode>(I);
      int BBIdx = PN->getBasicBlockIndex(BB);
      PN->setIncomingBlock(BBIdx, newEndBB);
    }
  }

  bool RemoveSelfLoopEdge::runOnFunction(Function & F) {
    typedef std::set<BasicBlock* > BasicBlockSetTy;
    BasicBlockSetTy toBeProceed;
    for (Function::iterator it = F.begin(), it_end = F.end(); it != it_end; ++it) {
      Instruction * inst = it->getTerminator();
      if (BranchInst * branchInst = dyn_cast<BranchInst>(inst)) {
	for (User::op_iterator op_it = branchInst->op_begin(), op_end = branchInst->op_end(); op_it != op_end; ++op_it) {
	  BasicBlock * bb = dyn_cast<BasicBlock>(op_it->get());
	  if (bb == &*it) {
	    toBeProceed.insert(bb);
	  }
	}
      }
    }

    for (BasicBlockSetTy::iterator it = toBeProceed.begin(), it_end = toBeProceed.end(); it != it_end; ++it) {
      removeBBSelfLoopEdge(*it);
    }

    return toBeProceed.size() != 0;
  }

  /// DuplicateCodeTransform Methods

  char DuplicateCodeTransform::ID = 0;

  bool DuplicateCodeTransform::runOnModule(Module &M) {
    CodeDuplicationAnalysis & CDA = getAnalysis<CodeDuplicationAnalysis>();
    for (CodeDuplicationAnalysis::BlockInfoTy::const_iterator it = CDA.getBlockInfo().begin(), 
	   it_end = CDA.getBlockInfo().end(); it != it_end; ++it) {
      wrapCheckingRegionAsFunction(M, it->first, it->second);
    }
    return true;
  }

  void DuplicateCodeTransform::wrapCheckingRegionAsFunction(Module & M, const BasicBlock * bb,
							    const CodeDuplicationAnalysis::InputArgumentsTy & args) {
    std::vector<const Type *> argType;
    for (CodeDuplicationAnalysis::InputArgumentsTy::const_iterator it = args.begin(), end = args.end(); it != end; ++it) {
      argType.push_back((*it)->getType());
    }

    const Type * VoidType  = Type::getVoidTy(M.getContext());
    FunctionType * FTy = FunctionType::get(VoidType,  argType, false);
    Function * F = Function::Create(FTy, GlobalValue::InternalLinkage, bb->getName() + ".dup", &M);

    /// Mapping from original def to function arguments
    typedef std::map<Value *, Argument *> DefToArgMapTy;
    DefToArgMapTy defToArgMap;
    Function::arg_iterator it_arg, it_arg_end;
    CodeDuplicationAnalysis::InputArgumentsTy::const_iterator it_arg_val, it_arg_val_end;
    for (it_arg = F->arg_begin(), it_arg_end = F->arg_end(), it_arg_val = args.begin(), it_arg_val_end = args.end();
	 it_arg != it_arg_end; ++it_arg, ++it_arg_val) {
      Value * argVal = *it_arg_val;
      it_arg->setName(argVal->getName() + ".dup");
      defToArgMap[argVal] = it_arg;
    }

    DenseMap<const Value*, Value*> valMapping;
    BasicBlock * newBB = CloneBasicBlock(bb, valMapping, "", F);
    Instruction * termInst = newBB->getTerminator();
    termInst->eraseFromParent();
    ReturnInst::Create(M.getContext(), NULL, newBB);

    /// Replace defs inside the basic blocks with function arguments
    for (CodeDuplicationAnalysis::InputArgumentsTy::const_iterator it = args.begin(), end = args.end(); it != end; ++it) {
      if (valMapping.find(*it) != valMapping.end()) {
	Instruction * defInst = dyn_cast<Instruction>(valMapping[*it]);
	defInst->replaceAllUsesWith(defToArgMap[*it]);
        defInst->eraseFromParent();
      }
    }

    /// Eliminate stores
    std::set<Instruction *> toBeRemoved;
    for (BasicBlock::iterator it = newBB->begin(), end = newBB->end(); it != end; ++it) {
      if (isa<StoreInst>(it)) toBeRemoved.insert(it);
    }

    for (std::set<Instruction *>::iterator it = toBeRemoved.begin(), end = toBeRemoved.end(); it != end; ++it) {
      (*it)->removeFromParent();
    }

    /// Replace all uses whose defs
    for (BasicBlock::iterator it = newBB->begin(), end = newBB->end(); it != end; ++it) {
      for (DefToArgMapTy::iterator AI = defToArgMap.begin(), AE = defToArgMap.end(); AI != AE; ++AI) {
	it->replaceUsesOfWith(AI->first, AI->second);
      }

      for (DenseMap<const Value *, Value *>::iterator AI = valMapping.begin(), AE = valMapping.end(); AI != AE; ++AI) {
	it->replaceUsesOfWith(const_cast<Value*>(AI->first), AI->second);
      }
    }
  }

  ///
  /// Template Helper to remove instructions from loop
  ///
  template <class T>
	static void removeInstructionFromLoop(Loop * L) {
		T op;
		std::vector<Instruction*> toBeRemoved;

		for (Loop::block_iterator I = L->block_begin(), E = L->block_end(); I != E; ++I) {
			for (BasicBlock::iterator BI = (*I)->begin(), BE = (*I)->end(); BI != BE; ++BI) {
				if (op(BI)) {
					toBeRemoved.push_back(BI);
				}
			}
		}

		for(std::vector<Instruction*>::iterator it = toBeRemoved.begin(), end = toBeRemoved.end(); it != end; ++it) {
			(*it)->eraseFromParent();
		}

	}
  

  ///
  /// Loop Duplication Methods
  ///


  struct StoreInstPred {
    bool operator()(Instruction * I) const {
      return isa<StoreInst>(I);
    }
  };
  
	struct ExactCheckCallPred {
		bool operator()(Instruction * I) const {
			if (CallInst * CI = dyn_cast<CallInst>(I)) {
				Function * F = CI->getCalledFunction();
				if (F && (F->getName() == "exactcheck" || F->getName() == "exactcheck2")) {
					return true;
				}
			}
			return false;
		} 
  };

  struct CheckingCallPred {
		bool operator()(Instruction * I) const {
			if (CallInst * CI = dyn_cast<CallInst>(I)) {
				Function * F = CI->getCalledFunction();
				if (F && isCheckingCall(F->getName())) {
					return true;
				}
			}
			return false;
		} 
  };

	template <class T1, class T2>
	struct pred_and {
		T1 t1; T2 t2;
		bool operator()(Instruction * I) const {
			return t1(I) && t2(I);
		}
	};

	template <class T>
	struct pred_not {
		T t;
		bool operator()(Instruction * I) const {
			return !t(I);
		}
	};

  char DuplicateLoopAnalysis::ID = 0;
  
  bool
  DuplicateLoopAnalysis::doInitialization(Module &) {
    cloneFunction.clear();
    return false;
  }

  bool
  DuplicateLoopAnalysis::runOnFunction(Function & F) {
		if (cloneFunction.find(&F) != cloneFunction.end())
			return false;

    Module * M = F.getParent();
    LI = &getAnalysis<LoopInfo>();
    for (LoopInfo::iterator it = LI->begin(), end = LI->end(); it != end; ++it) {
			duplicateLoop(*it, M);
		}

		return false;
  }

	void
	DuplicateLoopAnalysis::duplicateLoop(Loop * L, Module * M) {
		dupLoopArgument.clear();
    cloneValueMap.clear();
    if (isEligibleforDuplication(L)) {
      calculateArgument(L);
      Function * wrapFunction =  wrapLoopIntoFunction(L, M);
      cloneFunction.insert(wrapFunction);
      ++DuplicatedLoop;
    } else {
			// Try all subloops
			for(Loop::iterator it = L->begin(), end = L->end(); it != end; ++it) {
				duplicateLoop(*it, M);
			}
		}
	}

  bool
	DuplicateLoopAnalysis::isEligibleforDuplication(const Loop * L) const {
		// Loop should have a preheader for adding synchronization points
		if (!L->getLoopPreheader())
			return false;

		// Only duplicate loop with checking calls

		bool hasCheckingCalls = false;
		for (Loop::block_iterator I = L->block_begin(), E = L->block_end(); I != E; ++I) {
			for (BasicBlock::iterator BI = (*I)->begin(), BE = (*I)->end(); BI != BE; ++BI) {
				if (BI->mayWriteToMemory()) {
					if (isa<StoreInst>(BI)) {
						// FIXME: Check whether the store instruction is safe or not
						continue;
					} else if (isa<CallInst>(BI)) {
						CallInst * CI = dyn_cast<CallInst>(BI);
						Function * F = CI->getCalledFunction();
						if (F) {
							bool flag = isCheckingCall(F->getName());
							hasCheckingCalls |= flag;
							if (!flag) {
								return false;
							}
						} else {
							return false;
						}
					}
				}
			}
		}

		if (!hasCheckingCalls) return false;

		return true;
	}

  void
  DuplicateLoopAnalysis::calculateArgument(const Loop * L) {
    assert(dupLoopArgument.size() == 0);
    std::set<Value*> argSet;
    for (Loop::block_iterator I = L->block_begin(), E = L->block_end(); I != E; ++I) {
      for (BasicBlock::iterator BI = (*I)->begin(), BE = (*I)->end(); BI != BE; ++BI) {
	for(User::const_op_iterator OI = BI->op_begin(), OE = BI->op_end(); OI != OE; ++OI) {
	  Value * val = *OI;
	  if (Instruction * I = dyn_cast<Instruction>(val))  {
	    if (!L->contains(I->getParent())) {
	    	argSet.insert(val);
			}
	  } else if (Argument * arg = dyn_cast<Argument>(val)) {
	    argSet.insert(arg);
		}
	}
      }
    }
    
    for(std::set<Value*>::iterator it = argSet.begin(), end = argSet.end(); it != end; ++it) {
      dupLoopArgument.push_back(*it);
    }
  }

  Function *
  DuplicateLoopAnalysis::wrapLoopIntoFunction(Loop * L, Module * M) {
    std::vector<const Type *> argType;
    for (DuplicateLoopAnalysis::InputArgumentsTy::const_iterator it = dupLoopArgument.begin(), end = dupLoopArgument.end(); it != end; ++it) {
      argType.push_back((*it)->getType());
    }

    const Type * VoidType  = Type::getVoidTy(M->getContext());
    StructType * checkArgumentsType=StructType::get(M->getContext(),argType);
    std::vector<const Type *> funcArgType;
    funcArgType.push_back(PointerType::getUnqual(checkArgumentsType));
    FunctionType * FTy = FunctionType::get(VoidType,  funcArgType, false);
    Function * F = Function::Create(FTy, GlobalValue::InternalLinkage, ".codedup", M);
    Argument * funcActual = F->arg_begin();
    funcActual->setName("args");
    /*
    /// Mapping from original def to function arguments
    typedef std::map<Value *, Argument *> DefToArgMapTy;
    DefToArgMapTy defToArgMap;
    Function::arg_iterator it_arg, it_arg_end;
    DuplicateLoopAnalysis::InputArgumentsTy::const_iterator it_arg_val, it_arg_val_end;
    for (it_arg = F->arg_begin(), it_arg_end = F->arg_end(), it_arg_val = args.begin(), it_arg_val_end = args.end();
	 it_arg != it_arg_end; ++it_arg, ++it_arg_val) {
      Value * argVal = *it_arg_val;
      it_arg->setName(argVal->getName() + ".dup");
      defToArgMap[argVal] = it_arg;
    }
    */

    // Add codes into the function and clone the loop

    BasicBlock * entryBlock = BasicBlock::Create(M->getContext(),"entry",F);
    BasicBlock * exitBlock= BasicBlock::Create(M->getContext(),"loopexit",F);
    ReturnInst::Create(M->getContext(), NULL, exitBlock);
    DenseMap<const Value *, Value*> & valMapping = cloneValueMap;
    valMapping[L->getLoopPreheader()] = entryBlock;
    SmallVector<BasicBlock*, 8> exitBlocks;
    L->getUniqueExitBlocks(exitBlocks);
    for (SmallVector<BasicBlock*, 8>::iterator ExitBlocksIt = exitBlocks.begin(), ExitBlocksEnd = exitBlocks.end();
	 ExitBlocksIt != ExitBlocksEnd; ++ExitBlocksIt) {
      valMapping[*ExitBlocksIt] = exitBlock;
    }

    // Generate loads for arguments
    int arg_counter = 0;
    for (DuplicateLoopAnalysis::InputArgumentsTy::const_iterator it = dupLoopArgument.begin(), end = dupLoopArgument.end(); it != end; ++it) {
      const Type * Int32Type = IntegerType::getInt32Ty(M->getContext());
      std::vector<Value*> idxVal;
      idxVal.push_back(ConstantInt::get(Int32Type, 0));
      idxVal.push_back(ConstantInt::get(Int32Type, arg_counter));

      GetElementPtrInst * GEP = GetElementPtrInst::Create(funcActual, idxVal.begin(), idxVal.end(), "", entryBlock);
      Value * loadArg = new LoadInst(GEP, ".arg", entryBlock);
      valMapping[*it] = loadArg;
      ++arg_counter;
    }

    
    // Clone the loop

    Loop * NewLoop = new Loop();

    for (Loop::block_iterator I = L->block_begin(), E = L->block_end(); I != E; ++I) {
      BasicBlock * bb = CloneBasicBlock(*I, valMapping, ".dup", F);
      valMapping[*I] = bb;
      NewLoop->addBasicBlockToLoop(bb, LI->getBase());
    }


    BasicBlock * loopHeader = NewLoop->getHeader();
    BranchInst::Create(loopHeader, entryBlock);
    loopHeader->moveAfter(entryBlock);
    exitBlock->moveAfter(&(F->back()));

    // Replace all uses in the loop with function arguments
    for (Loop::block_iterator I = NewLoop->block_begin(), E = NewLoop->block_end(); I != E; ++I) {
      for (BasicBlock::iterator BI = (*I)->begin(), BE = (*I)->end(); BI != BE; ++BI) {
	for(User::op_iterator OI = BI->op_begin(), OE = BI->op_end(); OI != OE; ++OI) {
	  DenseMap<const Value*, Value*>::iterator valMapIt = valMapping.find(*OI);
	  if (valMapIt != valMapping.end()) {
	    OI->set(valMapIt->second);
	  }
	
	}
      }
    }

    removeInstructionFromLoop<StoreInstPred>(NewLoop);
    removeInstructionFromLoop<ExactCheckCallPred>(NewLoop);

    removeInstructionFromLoop<pred_and<CheckingCallPred, pred_not<ExactCheckCallPred> > >(L);

    replaceIntrinsic(NewLoop, M);
    
    // Insert checking calls
    insertCheckingCallInLoop(L, F, checkArgumentsType, M);

    return F;
  }

  void
  DuplicateLoopAnalysis::replaceIntrinsic(Loop * L, Module * M) {
    for(Loop::block_iterator BI = L->block_begin(), BE = L->block_end(); BI != BE; ++BI) {
      for(BasicBlock::iterator I = (*BI)->begin(), E = (*BI)->end(); I != E; ++I) {
	if (CallInst * CI = dyn_cast<CallInst>(I)) {
	  Function * F = CI->getCalledFunction();
	  if (!F || !isCheckingCall(F->getName()))
	    continue;

	  Constant * newF = M->getOrInsertFunction(F->getName().str() + ".serial" , F->getFunctionType());
	  CI->setOperand(0, newF);
	}
      }
    }
  }

  void
	DuplicateLoopAnalysis::insertCheckingCallInLoop(Loop * L, Function * checkingFunction, StructType * checkArgumentType, Module * M) {
    const Type * VoidType  = Type::getVoidTy(M->getContext());
		static Constant * sFuncWaitForSyncToken = 
			M->getOrInsertFunction("__sc_par_wait_for_completion", 
					FunctionType::get
					(VoidType, std::vector<const Type*>(), false));

		static Constant * sFuncEnqueueCheckingFunction = 
			M->getOrInsertFunction("__sc_par_enqueue_code_dup", 
			FunctionType::get
			 (VoidType, args<const Type*>::list(getVoidPtrType(*M), getVoidPtrType(*M)), false));

		Instruction * termInst = L->getHeader()->getTerminator();
		Value * allocaInst = new AllocaInst(checkArgumentType, "checkarg", &L->getHeader()->getParent()->front().front());

		size_t arg_counter = 0;
		for (InputArgumentsTy::const_iterator it = dupLoopArgument.begin(), end = dupLoopArgument.end(); it !=end; ++it) {
      const Type * Int32Type = IntegerType::getInt32Ty(M->getContext());
			std::vector<Value *> idxVal;
			idxVal.push_back(ConstantInt::get(Int32Type, 0));
			idxVal.push_back(ConstantInt::get(Int32Type, arg_counter));
			GetElementPtrInst * GEP = GetElementPtrInst::Create(allocaInst, idxVal.begin(), idxVal.end(), "", termInst);
			new StoreInst(*it, GEP, termInst);
			++arg_counter;
		}


		// enqueue
		std::vector<Value*> enqueueArg;
		enqueueArg.push_back(new BitCastInst(checkingFunction, getVoidPtrType(*M), "", termInst));
		enqueueArg.push_back(new BitCastInst(allocaInst, getVoidPtrType(*M), "", termInst));
		CallInst::Create(sFuncEnqueueCheckingFunction, enqueueArg.begin(), enqueueArg.end(), "", termInst);


		SmallVector<BasicBlock*, 8> exitBlocks;
		L->getUniqueExitBlocks(exitBlocks);
		for (SmallVector<BasicBlock*, 8>::iterator it = exitBlocks.begin(), end = exitBlocks.end(); it != end; ++it) {	
			CallInst::Create(sFuncWaitForSyncToken, "", &((*it)->back()));
		}

/*
	Function * origFunc = L->getHeader()->getParent();
		for (Function::iterator it = origFunc->begin(), end = origFunc->end(); it != end; ++it) {
			Instruction * TI = it->getTerminator();
			if (isa<ReturnInst>(TI) || isa<ResumeInst>(TI)) {
				CallInst::Create(sFuncWaitForSyncToken, "", TI);
			}
		}
*/
	}

  // Helper Function
  bool isCheckingCall(const std::string & name) {
    static std::string checkFuncs[] = {
      "poolcheck", "poolcheckui", "poolcheckui",
			"poolcheckalign", "poolcheckalignui",
      "exactcheck", "exactcheck2", 
			"boundscheck", "boundscheckui", 
			"funccheck"
    };
    
    for (size_t i = 0; i < sizeof(checkFuncs) / sizeof(std::string); ++i) {
      if (name == checkFuncs[i]) return true;
    }

    return false;

    }
}
