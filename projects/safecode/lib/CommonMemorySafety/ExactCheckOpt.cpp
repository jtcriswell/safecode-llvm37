//===- ExactCheckOpt.cpp - Convert checks into their fast versions --------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass replaces load/store/gep checks with their fast versions if the
// source memory objects can be found.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "exactcheck-opt"

#include "CommonMemorySafetyPasses.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/MemoryBuiltins.h"
#include "llvm/Analysis/MSCInfo.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Transforms/Instrumentation.h"
#include "llvm/Pass.h"

#include <map>
#include <queue>

using namespace llvm;

STATISTIC(GEPChecksConverted, "GEP checks converted to the fast version");
STATISTIC(MemoryChecksConverted,
          "Load/store checks converted to the fast version");

namespace {
  typedef std::pair <Value*, Value*> PtrSizePair;

  class ExactCheckOpt : public ModulePass {
    MSCInfo *MSCI;
    const DataLayout *TD;
#if 1 /* JRM */ 
    TargetLibraryInfo *TLI;
#endif /* JRM */
    ObjectSizeOffsetEvaluator *ObjSizeEval;

    PointerType *VoidPtrTy;

    // The set of allocas that are known to be alive to the end of the function.
    SmallSet <AllocaInst*, 32> FunctionScopedAllocas;

    void findFunctionScopedAllocas(Module &M);
    bool isSimpleMemoryObject(Value *V) const;
    PtrSizePair getPtrAndSize(Value *V, Type *SizeTy,
                              std::map <Value*, PtrSizePair> &M);

    bool optimizeCheck(CallInst *CI, CheckInfoType* Info);
    Type* getSizeType(CheckInfoType *Info, Module &M);
    void createFastCheck(CheckInfoType* Info, CallInst *CI, Value *ObjPtr,
                              Value *ObjSize);
    void optimizeAll(Module &M, CheckInfoType* Check, Statistic *StatsVar);

  public:
    static char ID;
    ExactCheckOpt(): ModulePass(ID) { }
    virtual bool runOnModule(Module &M);

    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.addRequired<MSCInfo>();
      AU.addRequired<TargetLibraryInfoWrapperPass>();
      AU.setPreservesCFG();
    }

    virtual const char *getPassName() const {
      return "ExactCheckOpt";
    }
  };
} // end anon namespace

char ExactCheckOpt::ID = 0;

INITIALIZE_PASS(ExactCheckOpt, "exactcheck-opt",
                "Convert checks into their fast versions", false, false)

ModulePass *llvm::createExactCheckOptPass() {
  return new ExactCheckOpt();
}

bool ExactCheckOpt::runOnModule(Module &M) {
  MSCI = &getAnalysis<MSCInfo>();
  TD = &M.getDataLayout();
#if 1 /* JRM */
  TLI = &getAnalysis<TargetLibraryInfoWrapperPass>().getTLI();
#endif /* JRM */

#if 0 /* JRM */ /* original code */
  ObjectSizeOffsetEvaluator TheObjSizeEval(TD, M.getContext());
#else /* JRM */ /* modified code */
  ObjectSizeOffsetEvaluator TheObjSizeEval(*TD, TLI, M.getContext());
#endif /* JRM */
  ObjSizeEval = &TheObjSizeEval;

  Type *VoidTy = Type::getVoidTy(M.getContext());
  VoidPtrTy = Type::getInt8PtrTy(M.getContext());
  Type *Int64Ty = IntegerType::getInt64Ty(M.getContext());

  findFunctionScopedAllocas(M);

  // Insert the fast check prototypes
  M.getOrInsertFunction("__fastloadcheck", VoidTy, VoidPtrTy, Int64Ty,
                        VoidPtrTy, Int64Ty, NULL);
  M.getOrInsertFunction("__faststorecheck", VoidTy, VoidPtrTy, Int64Ty,
                        VoidPtrTy, Int64Ty, NULL);
  M.getOrInsertFunction("__fastgepcheck", VoidPtrTy, VoidPtrTy, VoidPtrTy,
                        VoidPtrTy, Int64Ty, NULL);

  // TODO: move this to a more appropriate place.
  Type *Int32Type = IntegerType::getInt32Ty(M.getContext());
  M.getOrInsertFunction("exactcheck2", VoidPtrTy, VoidPtrTy, VoidPtrTy,
                        VoidPtrTy, Int32Type, NULL);
  M.getOrInsertFunction("fastlscheck", VoidTy, VoidPtrTy, VoidPtrTy, Int32Type,
                        Int32Type, NULL);

  //
  // Add the readnone attribute to the fast checks; they don't use global state
  // to determine if a pointer passes the check.
  //
  // To clarify, these fuction have Attribute::ReadNone because they are
  // purely functions of their input parameters -- unlike boundscheck()
  // (which has Attribute::ReadOnly) whose output can be influenced by 
  // changes in the heap.
  //
  M.getFunction("exactcheck2")->addFnAttr (Attribute::ReadNone);
  M.getFunction("fastlscheck")->addFnAttr (Attribute::ReadNone);

  CheckInfoListType CheckInfoList = MSCI->getCheckInfoList();
  for (size_t i = 0, N = CheckInfoList.size(); i < N; ++i) {
    CheckInfoType* Info = CheckInfoList[i];
    if (Info->IsFastCheck || !Info->FastVersionInfo)
      continue;

    if (Info->isMemoryCheck())
      optimizeAll(M, Info, &MemoryChecksConverted);
    else if(Info->isGEPCheck())
      optimizeAll(M, Info, &GEPChecksConverted);
  }

  return true; // assume that something was changed in the module
}

/// findFunctionScopedAllocas - store all allocas that are known to be valid
/// to the end of their function in a set. The current algorithm does this by
/// finding all the allocas in the entry block that are before the first
/// llvm.stacksave call (if any).
///
/// FIXME: There can also be allocas elsewhere that get deallocated at the end
/// of the function but they are pessimistically ignored for now.
///
void ExactCheckOpt::findFunctionScopedAllocas(Module &M) {
  for (Module::iterator F = M.begin(), E = M.end(); F != E; ++F) {
    if (F->empty())
      continue;

    BasicBlock &BB = F->getEntryBlock();
    for (BasicBlock::iterator I = BB.begin(), E = BB.end(); I != E; ++I) {
      if (AllocaInst *AI = dyn_cast<AllocaInst>(I)) {
        FunctionScopedAllocas.insert(AI);
      } else if(CallInst *CI = dyn_cast<CallInst>(I)) {
        Function *CalledFunction = CI->getCalledFunction();
        if (CalledFunction && CalledFunction->getName() == "llvm.stacksave")
          break;
      }
    }
  }
}

/// isSimpleMemoryObject - return true if the only argument is an allocation of
/// a memory object that can't be freed. Also consider constant null pointers to
/// have size zero.
///
bool ExactCheckOpt::isSimpleMemoryObject(Value *V) const {
  if (AllocaInst *AI = dyn_cast<AllocaInst>(V))
    return FunctionScopedAllocas.count(AI);

  if (GlobalValue *GV = dyn_cast<GlobalValue>(V)) {
    if (GV->isDeclaration())
      return false;
    return GV->hasExternalLinkage() || GV->hasLocalLinkage();
  }

  if (Argument *AI = dyn_cast<Argument>(V))
    return AI->hasByValAttr();

  if (isa<ConstantPointerNull>(V))
    return true;

  return false;
}

/// getPtrAndSize - return a pair of Value points where the first element is the
/// void pointer of the target memory object and the second element is its size.
/// The second argument is used for caching and avoiding loops.
///
PtrSizePair ExactCheckOpt::getPtrAndSize(Value *V, Type *SizeTy,
                                         std::map <Value*, PtrSizePair> &M) {
  V = V->stripPointerCasts();
  if (M.count(V))
    return M[V];

  if (PHINode *PHI = dyn_cast<PHINode>(V)) {
    // Create temporary phi nodes. They will be finalized later on.
    PHINode *Ptr = PHINode::Create(VoidPtrTy, PHI->getNumIncomingValues(),
                                   "obj_phi", PHI);
    PHINode *Size = PHINode::Create(SizeTy, PHI->getNumIncomingValues(),
                                    "size_phi", PHI);
    M[V] = std::make_pair(Ptr, Size);
  } else if (SelectInst *SI = dyn_cast<SelectInst>(V)) {
    PtrSizePair TrueCase = getPtrAndSize(SI->getTrueValue(), SizeTy, M);
    PtrSizePair FalseCase = getPtrAndSize(SI->getFalseValue(), SizeTy, M);

    SelectInst *Ptr = SelectInst::Create(SI->getCondition(), TrueCase.first,
                                         FalseCase.first, "obj_select", SI);
    SelectInst *Size = SelectInst::Create(SI->getCondition(), TrueCase.second,
                                          FalseCase.second, "size_select", SI);
    M[V] = std::make_pair(Ptr, Size);
  } else if (ConstantExpr *CE = dyn_cast<ConstantExpr>(V)) {
    assert(CE->getOpcode() == Instruction::GetElementPtr);
    M[V] = getPtrAndSize(CE->getOperand(0), SizeTy, M);
  } else if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(V)) {
    M[V] = getPtrAndSize(GEP->getPointerOperand(), SizeTy, M);
  }

  assert(M.count(V) && "The corresponding size should already exist.");
  return M[V];
}

/// optimizeCheck - replace the given check CallInst with the check's fast
/// version if all the source memory objects can be found and it is obvious
/// that none of them have been freed at the point where the check is made.
/// Returns the new call if possible and NULL otherwise.
///
/// This currently works only with memory objects that can't be freed:
/// * global variables
/// * allocas that trivially have function scope
/// * byval arguments
///
bool ExactCheckOpt::optimizeCheck(CallInst *CI, CheckInfoType *Info) {
  // Examined values
  SmallSet<Value*, 16> Visited;
  // Potential memory objects
  SmallSet<Value*, 4> Objects;

  std::queue<Value*> Q;
  // Start from the the pointer operand
  Value *StartPtr = CI->getArgOperand(Info->PtrArgNo)->stripPointerCasts();
  Q.push(StartPtr);

  // Use BFS to find all potential memory objects
  while(!Q.empty()) {
    Value *o = Q.front()->stripPointerCasts();
    Q.pop();
    if(Visited.count(o))
      continue;
    Visited.insert(o);

    if (ConstantExpr *CE = dyn_cast<ConstantExpr>(o)) {
      if (CE->getOpcode() == Instruction::GetElementPtr) {
        Q.push(CE->getOperand(0));
      } else {
        // Exit early if any of the objects are unsupported.
        if (!isSimpleMemoryObject(o))
          return false;
        Objects.insert(o);
      }
    } else if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(o)) {
      Q.push(GEP->getPointerOperand());
      // It is fine to ignore the case of indexing into null with a pointer
      // because that case is invalid for LLVM-aware objects such as allocas,
      // globals, and objects pointed to by noalias pointers.
    } else if(PHINode *PHI = dyn_cast<PHINode>(o)) {
      for (unsigned i = 0, num = PHI->getNumIncomingValues(); i != num; ++i)
        Q.push(PHI->getIncomingValue(i));
    } else if (SelectInst *SI = dyn_cast<SelectInst>(o)) {
      Q.push(SI->getTrueValue());
      Q.push(SI->getFalseValue());
    } else {
      // Exit early if any of the objects are unsupported.
      if (!isSimpleMemoryObject(o))
        return false;
      Objects.insert(o);
    }
  }

  // Mapping from the initial value to the corresponding size and void pointer:
  // * memory object -> its size and pointer
  // * phi/select -> corresponding phi/select for the sizes and pointers
  // * anything else -> the corresponding size and pointer on the path
  std::map <Value*, PtrSizePair> M;

  Module &Mod = *CI->getParent()->getParent()->getParent();
  Type *SizeTy = getSizeType(Info, Mod);

  // Add non-instruction non-constant allocation object pointers to the front
  // of the function's entry block.
  BasicBlock &EntryBlock = CI->getParent()->getParent()->getEntryBlock();
  Instruction *FirstInsertionPoint = ++BasicBlock::iterator(EntryBlock.begin());

  for (SmallSet<Value*, 16>::const_iterator It = Objects.begin(),
       E = Objects.end();
       It != E;
       ++It) {
    // Obj is a memory object pointer: alloca, argument, load, callinst, etc.
    Value *Obj = *It;

    // Insert instruction-based allocation pointers just after the allocation.
    Instruction *InsertBefore = FirstInsertionPoint;
    if (Instruction *I = dyn_cast<Instruction>(Obj))
      InsertBefore = ++BasicBlock::iterator(I);
    IRBuilder<> Builder(InsertBefore);

    SizeOffsetEvalType SizeOffset = ObjSizeEval->compute(Obj);
    assert(ObjSizeEval->bothKnown(SizeOffset));
    assert(dyn_cast<ConstantInt>(SizeOffset.second)->isZero());
    Value *Size = Builder.CreateIntCast(SizeOffset.first, SizeTy,
                                        /*isSigned=*/false);

    Value *Ptr = Builder.CreatePointerCast(Obj, VoidPtrTy);
    M[Obj] = std::make_pair(Ptr, Size);
  }

  // Create the rest of the size values and object pointers.
  // The phi nodes will be finished later.
  for (SmallSet<Value*, 16>::const_iterator I = Visited.begin(),
       E = Visited.end();
       I != E;
       ++I) {
    getPtrAndSize(*I, SizeTy, M);
  }

  // Finalize the phi nodes.
  for (SmallSet<Value*, 16>::const_iterator I = Visited.begin(),
       E = Visited.end();
       I != E;
       ++I) {
    if (PHINode *PHI = dyn_cast<PHINode>(*I)) {
      assert(M.count(PHI));
      PHINode *PtrPHI = cast<PHINode>(M[PHI].first);
      PHINode *SizePHI = cast<PHINode>(M[PHI].second);

      for(unsigned i = 0, num = PHI->getNumIncomingValues(); i != num; ++i) {
        Value *IncomingValue = PHI->getIncomingValue(i)->stripPointerCasts();
        assert(M.count(IncomingValue));

        PtrPHI->addIncoming(M[IncomingValue].first, PHI->getIncomingBlock(i));
        SizePHI->addIncoming(M[IncomingValue].second, PHI->getIncomingBlock(i));
      }
    }
  }

  // Insert the fast version of the check just before the regular version.
  assert(M.count(StartPtr) && "The memory object and its size should be known");
  createFastCheck(Info, CI, M[StartPtr].first, M[StartPtr].second);
  return true;
}

/// getSizeType - return the integer type being used to represent the size of
/// the memory object. This may be different from the system's size_t.
///
Type* ExactCheckOpt::getSizeType(CheckInfoType *Info, Module &M) {
  CheckInfoType *FastInfo = Info->FastVersionInfo;
  Function *FastFn = FastInfo->getFunction(M);
  assert(FastFn && "The fast check function should be defined.");
  return FastFn->getFunctionType()->getParamType(FastInfo->ObjSizeArgNo);
}

/// createFastCheck - create the fast memory safety check given the old check
/// and the corresponding object and its size.
///
void ExactCheckOpt::createFastCheck(CheckInfoType* Info, CallInst *CI,
                                    Value *ObjPtr, Value *ObjSize) {
  Module &M = *CI->getParent()->getParent()->getParent();

  // Get a pointer to the fast check function.
  CheckInfoType *FastInfo = Info->FastVersionInfo;
  Function *FastFn = FastInfo->getFunction(M);
  assert(FastFn && "The fast check function should be defined.");

  // Copy the old arguments to preserve extra arguments in fixed positions.
  SmallVector <Value*, 8> Args(FastFn->arg_size());
  assert(FastFn->arg_size() >= CI->getNumArgOperands());
  for (unsigned i = 0, N = CI->getNumArgOperands(); i < N; ++i)
    Args[i] = CI->getArgOperand(i);

  // Set the known arguments to right values.
  Args[FastInfo->PtrArgNo] = CI->getArgOperand(Info->PtrArgNo);
  Args[FastInfo->ObjArgNo] = ObjPtr;
  Args[FastInfo->ObjSizeArgNo] = ObjSize;

  if (Info->isMemoryCheck())
    Args[FastInfo->SizeArgNo] = CI->getArgOperand(Info->SizeArgNo);
  else  // must be a gep check
    Args[FastInfo->DestPtrArgNo] = CI->getArgOperand(Info->DestPtrArgNo);

  // Create the call just before the old call.
  IRBuilder<> Builder(CI);
  CallInst *FastCI = Builder.CreateCall(FastFn, Args);

  // Copy the debug information if it is present.
  if (MDNode *MD = CI->getMetadata("dbg"))
    FastCI->setMetadata("dbg", MD);

  if (Info->isGEPCheck())
    CI->replaceAllUsesWith(FastCI);
}

/// optimizeAllChecks - try to replace every check of the given type with
/// its fast version.
///
void ExactCheckOpt::optimizeAll(Module &M, CheckInfoType *Info,
                                Statistic *Stats) {
  Function *CheckFn = Info->getFunction(M);
  // Early return in case the regular check function doesn't exist
  if (!CheckFn)
    return;

  SmallVector <CallInst*, 64> Converted;
  // Convert the checks that can be safely converted.
  for (Value::use_iterator UI = CheckFn->use_begin(), E = CheckFn->use_end();
        UI != E;
        ++UI) {
    if (CallInst *CI = dyn_cast<CallInst>(*UI)) {
      if (optimizeCheck(CI, Info))
        Converted.push_back(CI);
    }
  }

  // Erase the regular versions of the converted checks.
  for (size_t i = 0, num = Converted.size(); i < num; ++i)
    Converted[i]->eraseFromParent();
  *Stats += Converted.size();
}
