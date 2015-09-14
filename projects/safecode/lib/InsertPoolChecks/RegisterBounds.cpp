//===- RegisterBounds.cpp ---------------------------------------*- C++ -*----//
// 
//                     The LLVM Compiler Infrastructure
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// Various passes to register the bound information of variables into the pools
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "sc-register"

#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/InstIterator.h"
#include "safecode/RegisterBounds.h"
#include "safecode/AllocatorInfo.h"
#include "safecode/Utility.h"

#include <functional>

using namespace llvm;

namespace {
  // Statistics
  STATISTIC (RegisteredGVs,      "Number of registered global variables");
  STATISTIC (RegisteredByVals,   "Number of registered byval arguments");
  STATISTIC (RegisteredHeapObjs, "Number of registered heap objects");
}

namespace llvm {

char RegisterGlobalVariables::ID = 0;
char RegisterMainArgs::ID = 0;
char RegisterFunctionByvalArguments::ID=0;
char RegisterCustomizedAllocation::ID = 0;


static llvm::RegisterPass<RegisterGlobalVariables>
X1 ("reg-globals", "Register globals into pools", true);

static llvm::RegisterPass<RegisterMainArgs>
X2 ("reg-argv", "Register argv[] into pools", true);

static llvm::RegisterPass<RegisterCustomizedAllocation>
X3 ("reg-custom-alloc", "Register customized allocators", true);

static llvm::RegisterPass<RegisterFunctionByvalArguments>
X4 ("reg-byval-args", "Register byval arguments for functions", true);

//
// Method: registerGV()
//
// Description:
//  This method adds code into a program to register a global variable into its
//  pool.
//
void
RegisterGlobalVariables::registerGV (GlobalVariable * GV,
                                     Instruction * InsertBefore) {
  //
  // Do not register the global variable if it has opaque type.  This is
  // because we cannot determine the size of an opaque type.
  //
  Type * GlobalType = GV->getType()->getElementType();
  if (StructType * ST = dyn_cast<StructType>(GlobalType))
    if (ST->isOpaque())
      return;

  //
  // Get the pool into which the global should be registered.
  //
  Value * PH = ConstantPointerNull::get (getVoidPtrType(GV->getContext()));
  Type* csiType = IntegerType::getInt32Ty(GV->getContext());
  unsigned TypeSize = TD->getTypeAllocSize((GlobalType));
  if (!TypeSize) {
    llvm::errs() << "FIXME: Ignoring global of size zero: ";
    GV->dump();
    return;
  }
  Value * AllocSize = ConstantInt::get (csiType, TypeSize);
  RegisterVariableIntoPool(PH, GV, AllocSize, InsertBefore);

  // Update statistics
  ++RegisteredGVs;
}

bool
RegisterGlobalVariables::runOnModule(Module & M) {
  init(M, "pool_register_global");

  //
  // Get required analysis passes.
  //
  TD       = &M.getDataLayout();

  //
  // Create a skeleton function that will register the global variables.
  //
  Type * VoidTy = Type::getVoidTy (M.getContext());
  Constant * CF = M.getOrInsertFunction ("sc.register_globals", VoidTy, NULL);
  Function * F = dyn_cast<Function>(CF);

  //
  // Create the basic registration function.
  //
  Instruction * InsertPt = CreateRegistrationFunction (F);

  //
  // Skip over several types of globals, including:
  //  llvm.used
  //  llvm.noinline
  //  llvm.global_ctors
  //  Any global pool descriptor
  //  Any global in the meta-data seciton
  //
  // The llvm.global_ctors requires special note.  Apparently, it will not
  // be code generated as the list of constructors if it has any uses
  // within the program.  This transform must ensure, then, that it is
  // never used, even if such a use would otherwise be innocuous.
  //
  Module::global_iterator GI = M.global_begin(), GE = M.global_end();
  for ( ; GI != GE; ++GI) {
    GlobalVariable *GV = dyn_cast<GlobalVariable>(GI);
    if (!GV) continue;

    // Don't register  external global variables
    //if (GV->isDeclaration()) continue;

    std::string name = GV->getName();

    // Skip globals in special sections
    if (!strcmp((GV->getSection()), "llvm.metadata")) continue;

    if (strncmp(name.c_str(), "llvm.", 5) == 0) continue;
    if (strncmp(name.c_str(), "__poolalloc", 11) == 0) continue;
   
    // Linking fails when registering objects in section exitcall.exit
    // This is needed for the Linux kernel.
    if (!strcmp(GV->getSection(), ".exitcall.exit")) continue;

    //
    // Skip globals that may not be emitted into the final executable.
    //
    if (GV->hasAvailableExternallyLinkage()) continue;
    registerGV(GV, InsertPt);    
  }

  return true;
}

bool
RegisterMainArgs::runOnModule(Module & M) {
  init(M, "pool_register");
  Function *MainFunc = M.getFunction("main");
  if (MainFunc == 0 || MainFunc->isDeclaration()) {
    return false;
  }

  //
  // If there are no argc and argv arguments, don't register them.
  //
  if (MainFunc->arg_size() < 2) {
    return false;
  }

  Function::arg_iterator AI = MainFunc->arg_begin();
  Value *Argc = AI;
  Value *Argv = ++AI;


  Instruction * InsertPt = MainFunc->front().begin(); 

  //
  // FIXME:
  //  This is a hack around what appears to be a DSA bug.  These pointers
  //  should be marked incomplete, but for some reason, in at least one test
  //  case, they are not.
  //
  // Register all of the argv strings
  //
  Type * VoidPtrType = getVoidPtrType(M.getContext());
  Type * Int32Type = IntegerType::getInt32Ty(M.getContext());
  Constant * CF = M.getOrInsertFunction ("poolargvregister",
                                          getVoidPtrType(M.getContext()),
                                          Int32Type,
                                          PointerType::getUnqual (VoidPtrType),
                                          NULL);
  Function * RegisterArgv = dyn_cast<Function>(CF);

  std::vector<Value *> fargs;
  fargs.push_back (Argc);
  fargs.push_back (Argv);
  CallInst::Create (RegisterArgv, fargs, "", InsertPt);
  return true;
}


///
/// Methods for RegisterCustomizedAllocations
///
void
RegisterCustomizedAllocation::proceedAllocator(Module * M, AllocatorInfo * info) {
  Function * allocFunc = M->getFunction(info->getAllocCallName());
  if (allocFunc) {
    for (Value::use_iterator it = allocFunc->use_begin(), 
           end = allocFunc->use_end(); it != end; ++it) {
      if (CallInst * CI = dyn_cast<CallInst>(*it)) {
        if (CI->getCalledValue() == allocFunc) {
          registerAllocationSite(CI, info);
          ++RegisteredHeapObjs;
        }
      }

      //
      // If the user is a constant expression, the constant expression may be
      // a cast that is used by a call instruction.  Get the enclosing call
      // instruction if so.
      //
      if (ConstantExpr * CE = dyn_cast<ConstantExpr>(*it)) {
        if (CE->isCast()) {
          for (Value::use_iterator iit = CE->use_begin(),
                 end = CE->use_end(); iit != end; ++iit) {
            if (CallInst * CI = dyn_cast<CallInst>(*iit)) {
              if (CI->getCalledValue() == CE) {
                registerAllocationSite(CI, info);
                ++RegisteredHeapObjs;
              }
            }
          }
        }
      }
    }
  }
  
  //
  // Find the deallocation function, visit all uses of it, and process all
  // calls to it.
  //
  Function * freeFunc = M->getFunction(info->getFreeCallName());
  if (freeFunc) {
    for (Value::use_iterator it = freeFunc->use_begin(),
           end = freeFunc->use_end(); it != end; ++it) {
      if (CallInst * CI = dyn_cast<CallInst>(*it)) {
        if (CI->getCalledValue() == freeFunc) {
          registerFreeSite(CI, info);
        }
      }

      //
      // If the user is a constant expression, the constant expression may be
      // a cast that is used by a call instruction.  Get the enclosing call
      // instruction if so.
      //
      if (ConstantExpr * CE = dyn_cast<ConstantExpr>(*it)) {
        if (CE->isCast()) {
          for (Value::use_iterator iit = CE->use_begin(),
                 end = CE->use_end(); iit != end; ++iit) {
            if (CallInst * CI = dyn_cast<CallInst>(*iit)) {
              if (CI->getCalledValue() == CE) {
                registerFreeSite(CI, info);
              }
            }
          }
        }
      }
    }
  }
}

void
RegisterCustomizedAllocation::proceedReallocator(Module * M, ReAllocatorInfo * info) {
  Function * allocFunc = M->getFunction(info->getAllocCallName());
  if (allocFunc) {
    for (Value::use_iterator it = allocFunc->use_begin(), 
           end = allocFunc->use_end(); it != end; ++it) {
      if (CallInst * CI = dyn_cast<CallInst>(*it)) {
        if (CI->getCalledValue()->stripPointerCasts() == allocFunc) {
          registerReallocationSite(CI, info);
          ++RegisteredHeapObjs;
        }
      }

      //
      // If the user is a constant expression, the constant expression may be
      // a cast that is used by a call instruction.  Get the enclosing call
      // instruction if so.
      //
      if (ConstantExpr * CE = dyn_cast<ConstantExpr>(*it)) {
        if (CE->isCast()) {
          for (Value::use_iterator iit = CE->use_begin(),
                 end = CE->use_end(); iit != end; ++iit) {
            if (CallInst * CI = dyn_cast<CallInst>(*iit)) {
              if (CI->getCalledValue() == CE) {
                registerReallocationSite(CI, info);
                ++RegisteredHeapObjs;
              }
            }
          }
        }
      }

    }
  }
  
  Function * freeFunc = M->getFunction(info->getFreeCallName());
  if (freeFunc) {
    for (Value::use_iterator it = freeFunc->use_begin(),
           end = freeFunc->use_end(); it != end; ++it) {
      if (CallInst * CI = dyn_cast<CallInst>(*it)) {
        if (CI->getCalledValue()->stripPointerCasts() == freeFunc) {
          registerFreeSite(CI, info);
        }
      }

      //
      // If the user is a constant expression, the constant expression may be
      // a cast that is used by a call instruction.  Get the enclosing call
      // instruction if so.
      //
      if (ConstantExpr * CE = dyn_cast<ConstantExpr>(*it)) {
        if (CE->isCast()) {
          for (Value::use_iterator iit = CE->use_begin(),
                 end = CE->use_end(); iit != end; ++iit) {
            if (CallInst * CI = dyn_cast<CallInst>(*iit)) {
              if (CI->getCalledValue() == CE) {
                registerFreeSite(CI, info);
              }
            }
          }
        }
      }
    }
  }
}

bool
RegisterCustomizedAllocation::runOnModule(Module & M) {
  init(M, "pool_register");

  //
  // Ensure that a prototype for strlen() exists.
  //
  const DataLayout & TD = M.getDataLayout();
  M.getOrInsertFunction ("nullstrlen",
                         TD.getIntPtrType(M.getContext(), 0),
                         getVoidPtrType(M.getContext()),
                         NULL);

  //
  // Get the functions for reregistering and deregistering memory objects.
  //
  Type * Int32Type = IntegerType::getInt32Ty (M.getContext());
  PoolReregisterFunc = (Function *) M.getOrInsertFunction ("pool_reregister",
                                                           Type::getVoidTy (M.getContext()),
                                                           getVoidPtrType (M),
                                                           getVoidPtrType (M),
                                                           getVoidPtrType (M),
                                                           Int32Type,
                                                           NULL);

  Constant * CF = M.getOrInsertFunction ("pool_unregister",
                                          Type::getVoidTy (M.getContext()),
                                          getVoidPtrType(M.getContext()),
                                          getVoidPtrType(M.getContext()),
                                          NULL);
  PoolUnregisterFunc = dyn_cast<Function>(CF);

  AllocatorInfoPass & AIP = getAnalysis<AllocatorInfoPass>();
  for (AllocatorInfoPass::alloc_iterator it = AIP.alloc_begin(),
      end = AIP.alloc_end(); it != end; ++it) {
    proceedAllocator(&M, *it);
  }

  for (AllocatorInfoPass::realloc_iterator it = AIP.realloc_begin(),
      end = AIP.realloc_end(); it != end; ++it) {
    proceedReallocator(&M, *it);
  }

  return true;
}

void
RegisterCustomizedAllocation::registerAllocationSite(CallInst * AllocSite, AllocatorInfo * info) {
  //
  // Get the pool handle for the node.
  //
  LLVMContext & Context = AllocSite->getContext();
  Value * PH = ConstantPointerNull::get (getVoidPtrType (Context));


  //
  // Find a place to insert the registration.
  //
  BasicBlock::iterator InsertPt = AllocSite;
  ++InsertPt;

  //
  // Find or create an LLVM value representing the size.  If that is not
  // possible, do not register the memory object.
  //
  // We do not assert out here (like one would think) because autoconf scripts
  // will create calls to strdup() with zero arguments.
  //
  Value * AllocSize = info->getOrCreateAllocSize(AllocSite);
  if (!AllocSize)
    return;

  //
  // Cast the size to the correct type.
  //
  if (!AllocSize->getType()->isIntegerTy(32)) {
    AllocSize = CastInst::CreateIntegerCast (AllocSize,
                                             Type::getInt32Ty(Context),
                                             false,
                                             AllocSize->getName(),
                                             InsertPt);
  }

  //
  // Create the registration of the object in the pool.
  //
  RegisterVariableIntoPool (PH, AllocSite, AllocSize, InsertPt);
}

void
RegisterCustomizedAllocation::registerReallocationSite(CallInst * AllocSite, ReAllocatorInfo * info) {
  //
  // Get the pool handle for the node.
  //
  LLVMContext & Context = AllocSite->getContext();
  Value * PH = ConstantPointerNull::get (getVoidPtrType (Context));

  //
  // Find the instruction following the reallocation site; this will be where
  // we insert the reallocation registration call.
  //
  BasicBlock::iterator InsertPt = AllocSite;
  ++InsertPt;

  //
  // Get the size of the allocation and cast it to the desired type.
  //
  Value * AllocSize = info->getOrCreateAllocSize(AllocSite);
  if (!AllocSize->getType()->isIntegerTy(32)) {
    AllocSize = CastInst::CreateIntegerCast (AllocSize,
                                             Type::getInt32Ty(Context),
                                             false,
                                             AllocSize->getName(),
                                             InsertPt);
  }

  //
  // Get the pointers to the old and new memory buffer.
  //
  Value * OldPtr = castTo (info->getAllocedPointer (AllocSite),
                           getVoidPtrType(PH->getContext()),
                           (info->getAllocedPointer (AllocSite))->getName(),
                           InsertPt);
  Value * NewPtr = castTo (AllocSite,
                           getVoidPtrType(PH->getContext()),
                           AllocSite->getName(),
                           InsertPt);

  //
  // Create the call to reregister the allocation.
  //
  std::vector<Value *> args;
  args.push_back (PH);
  args.push_back (NewPtr);
  args.push_back (OldPtr);
  args.push_back (AllocSize);
  CallInst * CI = CallInst::Create(PoolReregisterFunc, args, "", InsertPt); 

  //
  // If there's debug information on the allocation instruction, add it to the
  // registration call.
  //
  if (MDNode * MD = AllocSite->getMetadata ("dbg"))
    CI->setMetadata ("dbg", MD);

  return;
}

void
RegisterCustomizedAllocation::registerFreeSite (CallInst * FreeSite,
                                                AllocatorInfo * info) {
  //
  // Get the pointer being deallocated.  Strip away casts as these may have
  // been inserted after the DSA pass was executed and may, therefore, not have
  // a pool handle.
  //
  Value * ptr = info->getFreedPointer(FreeSite)->stripPointerCasts();

  //
  // If the pointer is a constant NULL pointer, then don't bother inserting
  // an unregister call.
  //
  if (isa<ConstantPointerNull>(ptr))
    return;

  //
  // Get the pool handle for the freed pointer.
  //
  LLVMContext & Context = FreeSite->getContext();
  Value * PH = ConstantPointerNull::get (getVoidPtrType(Context));

  //
  // Cast the pointer being unregistered and the pool handle into void pointer
  // types.
  //
  Value * Casted = castTo (ptr,
                           getVoidPtrType(Context),
                           ptr->getName()+".casted",
                           FreeSite);

  Value * PHCasted = castTo (PH,
                             getVoidPtrType(Context), 
                             PH->getName()+".casted",
                             FreeSite);

  //
  // Create a call that will unregister the object.
  //
  std::vector<Value *> args;
  args.push_back (PHCasted);
  args.push_back (Casted);
  CallInst::Create (PoolUnregisterFunc, args, "", FreeSite);
}

Instruction *
RegisterVariables::CreateRegistrationFunction(Function * F) {
  //
  // Destroy any code that currently exists in the function.  We are going to
  // replace it.
  //
  destroyFunction (F);

  //
  // Add a call in the new constructor function to the SAFECode initialization
  // function.
  //
  BasicBlock * BB = BasicBlock::Create (F->getContext(), "entry", F);

  //
  // Add a return instruction at the end of the basic block.
  //
  return ReturnInst::Create (F->getContext(), BB);
}

RegisterVariables::~RegisterVariables() {}

//
// Method: init()
//
// Description:
//  This method performs some initialization that is common to all subclasses
//  of this pass.
//
// Inputs:
//  M            - The module in which to insert the function.
//  registerName - The name of the function with which to register object.
//
void
RegisterVariables::init (Module & M, std::string registerName) {
  //
  // Create the type of the registration function.
  //
  Type * Int8Type    = IntegerType::getInt8Ty(M.getContext());
  Type * VoidTy      = Type::getVoidTy(M.getContext());

  std::vector<Type *> ArgTypes;
  ArgTypes.push_back (PointerType::getUnqual(Int8Type));
  ArgTypes.push_back (PointerType::getUnqual(Int8Type));
  ArgTypes.push_back (IntegerType::getInt32Ty(M.getContext()));
  FunctionType * PoolRegTy = FunctionType::get (VoidTy, ArgTypes, false);

  //
  // Create the function.
  //
  PoolRegisterFunc = dyn_cast<Function>(M.getOrInsertFunction (registerName,
                                                               PoolRegTy));
  return;
}

void
RegisterVariables::RegisterVariableIntoPool(Value * PH, Value * val, Value * AllocSize, Instruction * InsertBefore) {
  if (!PH) {
    llvm::errs() << "pool descriptor not present for " << val->getName().str()
                 << "\n";
    return;
  }

  Value *GVCasted = castTo (val,
                            getVoidPtrType(PH->getContext()), 
                            val->getName()+".casted",
                            InsertBefore);
  Value * PHCasted = castTo (PH,
                             getVoidPtrType(PH->getContext()), 
                             PH->getName()+".casted",
                             InsertBefore);
  std::vector<Value *> args;
  args.push_back (PHCasted);
  args.push_back (GVCasted);
  args.push_back (AllocSize);
  CallInst * CI = CallInst::Create(PoolRegisterFunc, args, "", InsertBefore); 

  //
  // If there's debug information on the allocation instruction, add it to the
  // registration call.
  //
  if (Instruction * I = dyn_cast<Instruction>(val->stripPointerCasts()))
    if (MDNode * MD = I->getMetadata ("dbg"))
      CI->setMetadata ("dbg", MD);
  return;
}

bool
RegisterFunctionByvalArguments::runOnModule(Module & M) {
  init(M, "pool_register_stack");

  //
  // Fetch prerequisite analysis passes.
  //
  TD        = &M.getDataLayout();

  //
  // Insert required intrinsics.
  //
  Constant * CF = M.getOrInsertFunction ("pool_unregister_stack",
                                          Type::getVoidTy (M.getContext()),
                                          getVoidPtrType(M.getContext()),
                                          getVoidPtrType(M.getContext()),
                                          NULL);
  StackFree = dyn_cast<Function>(CF);

  for (Module::iterator I = M.begin(), E = M.end(); I != E; ++ I) {
    //
    // Don't process declarations.
    //
    if (I->isDeclaration()) continue;

    //
    // Check the name of the function to see if it is a run-time function that
    // we should not process.
    //
    if (I->hasName()) {
      std::string Name = I->getName();
      if ((Name.find ("__poolalloc") == 0) || (Name.find ("poolregister") == 0))
        continue;
    }

    runOnFunction(*I);
  }

  return true;
}

//
// Method: runOnFunction()
//
// Description:
//  Entry point for this function pass.  This method will insert calls to
//  register the memory allocated for the byval arguments passed into the
//  specified function.
//
// Return value:
//  true  - The function was modified.
//  false - The function was not modified.
//
bool
RegisterFunctionByvalArguments::runOnFunction (Function & F) {
  //
  // Scan through all arguments of the function.  For each byval argument,
  // insert code to register the argument into its repspective pool.  Also
  // record the mapping between argument and pool so that we can insert
  // deregistration code at function exit.
  //
  typedef SmallVector<std::pair<Value*, Argument *>, 4> RegisteredArgTy;
  RegisteredArgTy registeredArguments;
  LLVMContext & Context = F.getContext();
  for (Function::arg_iterator I = F.arg_begin(), E = F.arg_end(); I != E; ++I) {
    if (I->hasByValAttr()) {
      assert (isa<PointerType>(I->getType()));
      PointerType * PT = cast<PointerType>(I->getType());
      Type * ET = PT->getElementType();
      Value * AllocSize = ConstantInt::get
        (IntegerType::getInt32Ty(Context), TD->getTypeAllocSize(ET));
      Value * PH = ConstantPointerNull::get (getVoidPtrType(Context));
      Instruction * InsertBefore = &(F.getEntryBlock().front());
      RegisterVariableIntoPool(PH, &*I, AllocSize, InsertBefore);
      registeredArguments.push_back(std::make_pair<>(PH, &*I));
    }
  }

  //
  // Find all basic blocks which terminate the function.
  //
  SmallSet<BasicBlock *, 4> exitBlocks;
  for (inst_iterator I = inst_begin(&F), E = inst_end(&F); I != E; ++I) {
    if (isa<ReturnInst>(*I) || isa<ResumeInst>(*I)) {
      exitBlocks.insert(I->getParent());
    }
  }

  //
  // At each function exit, insert code to deregister all byval arguments.
  //
  for (SmallSet<BasicBlock*, 4>::const_iterator BI = exitBlocks.begin(),
                                                BE = exitBlocks.end();
       BI != BE; ++BI) {
    for (RegisteredArgTy::const_iterator I = registeredArguments.begin(),
                                         E = registeredArguments.end();
         I != E; ++I) {
      SmallVector<Value *, 2> args;
      Instruction * Pt = &((*BI)->back());
      Value *CastPH  = castTo (I->first, getVoidPtrType(Context), Pt);
      Value *CastV = castTo (I->second, getVoidPtrType(Context), Pt);
      args.push_back (CastPH);
      args.push_back (CastV);
      CallInst::Create (StackFree, args, "", Pt);
    }
  }

  //
  // Update the statistics on the number of registered byval arguments.
  //  
  if (registeredArguments.size())
    RegisteredByVals += registeredArguments.size();

  return true;
}

}
