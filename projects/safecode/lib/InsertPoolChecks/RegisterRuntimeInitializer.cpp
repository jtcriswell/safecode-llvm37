//===- RegisterRuntimeInitializer.cpp ---------------------------*- C++ -*----//
// 
//                     The LLVM Compiler Infrastructure
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// Pass to register runtime initialization calls into user-space programs.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/Constants.h"
#include "llvm/IR/LLVMContext.h"
#include "safecode/RegisterRuntimeInitializer.h"
#include "safecode/Utility.h"

using namespace llvm;

namespace llvm {

char RegisterRuntimeInitializer::ID = 0;

static llvm::RegisterPass<RegisterRuntimeInitializer>
X1 ("reg-runtime-init", "Register runtime initializer into programs");

bool
RegisterRuntimeInitializer::runOnModule(llvm::Module & M) {
  constructInitializer(M);
  insertInitializerIntoGlobalCtorList(M);
  setLogFileName (M);
  return true;
}

//
// Method: setLogFileName()
//
// Description:
//  Insert a call into main() that tells the run-time the name of the log file
//  into which to put SAFECode error messages.
//
//  Note that we do *not* put them into a constructor.  Some libc functions are
//  initialized by constructors; functions like fprintf() won't work before
//  before these constructors are called.  Therefore, we put the call into
//  main(); any SAFECode errors before main() will just go to stderr.
//
void
RegisterRuntimeInitializer::setLogFileName (llvm::Module & M) {
  //
  // Do nothing if the name is empty.
  //
  if (strcmp (logfilename, "") == 0)
    return;

  //
  // See if there is a main function.  If not, just do nothing.
  //
  Function * Main = M.getFunction ("main");
  if (!Main)
    return;

  //
  // Find a place to insert the call within main().
  //
  BasicBlock::iterator InsertPt = Main->getEntryBlock().begin();
  while (isa<AllocaInst>(InsertPt))
    ++InsertPt;
 
  //
  // Create the function that sets the log filename.
  //
  Constant * SetLogC = M.getOrInsertFunction ("pool_init_logfile",
                                              Type::getVoidTy (M.getContext()),
                                              getVoidPtrType (M),
                                              NULL);
  Function * SetLog = cast<Function>(SetLogC);

  //
  // Create a global variable containing the log filename.
  //
  Constant * LogNameInit = ConstantDataArray::getString (M.getContext(),
                                                         logfilename);
  Value * LogName = new GlobalVariable (M,
                                        LogNameInit->getType(),
                                        true,
                                        GlobalValue::InternalLinkage,
                                        LogNameInit,
                                        "logname");

  //
  // Create a call to set the log filename.
  //
  Value * Param = castTo (LogName, getVoidPtrType (M), "logname", InsertPt);
  CallInst::Create (SetLog, Param, "", InsertPt); 
  return;
}

void
RegisterRuntimeInitializer::constructInitializer(llvm::Module & M) {
  //
  // Create a new function with zero arguments.  This will be the SAFECode
  // run-time constructor; it will be called by static global variable
  // constructor magic before main() is called.
  //
  Type * VoidTy  = Type::getVoidTy (M.getContext());
  Type * Int32Ty = IntegerType::getInt32Ty(M.getContext());
  Function * RuntimeCtor = (Function *) M.getOrInsertFunction("pool_ctor",
                                                              VoidTy,
                                                              NULL);

  Function * RuntimeInit = (Function *) M.getOrInsertFunction("pool_init_runtime", VoidTy, Int32Ty, Int32Ty, Int32Ty, NULL);
  Constant * CF = M.getOrInsertFunction ("sc.register_globals", VoidTy, NULL);
  Function * RegGlobals  = dyn_cast<Function>(CF);

  //
  // Make the global registration function internal.
  //
  RegGlobals->setDoesNotThrow();
  RegGlobals->setLinkage(GlobalValue::InternalLinkage);

  // Make the runtime constructor compatible with other ctors
  RuntimeCtor->setDoesNotThrow();
  RuntimeCtor->setLinkage(GlobalValue::InternalLinkage);

  //
  // Empty out the default definition of the SAFECode constructor function.
  // We'll replace it with our own code.
  //
  destroyFunction (RuntimeCtor);

  //
  // Add a call in the new constructor function to the SAFECode initialization
  // function.
  //
  BasicBlock * BB = BasicBlock::Create (M.getContext(), "entry", RuntimeCtor);
 
  // Delegate the responbilities of initializing pool descriptor to the 
  // SAFECode runtime initializer
//  CallInst::Create (PoolInit, "", BB); 

  Type * Int32Type = IntegerType::getInt32Ty(M.getContext());
  std::vector<Value *> args;

  //
  // By default, explicit dangling pointer checks are disabled,
  // rewrite pointers are enabled, and we should not terminate on
  // errors.  Some more refactoring will be needed to make all of this
  // work properly.
  //
  args.push_back (ConstantInt::get(Int32Type, 0));
  args.push_back (ConstantInt::get(Int32Type, 1));
  args.push_back (ConstantInt::get(Int32Type, 0));
  CallInst::Create (RuntimeInit, args, "", BB); 

  args.clear();
  CallInst::Create (RegGlobals, args, "", BB);


  //
  // Add a return instruction at the end of the basic block.
  //
  ReturnInst::Create (M.getContext(), BB);
}

void
RegisterRuntimeInitializer::insertInitializerIntoGlobalCtorList(Module & M) {
  Function * RuntimeCtor = M.getFunction ("pool_ctor");

  //
  // Create needed types.
  //
  Type * Int32Type = IntegerType::getInt32Ty(M.getContext());
  PointerType * CharPointer = PointerType::getInt8PtrTy(M.getContext());

  //
  // Insert the run-time ctor into the ctor list.
  // Make the priority 1 so we can allow the poolalloc constructor to go first.
  //
  std::vector<Constant *> CtorInits;
  CtorInits.push_back (ConstantInt::get (Int32Type, 1));
  CtorInits.push_back (RuntimeCtor);
  CtorInits.push_back (ConstantPointerNull::get (CharPointer));
  StructType * ST = ConstantStruct::getTypeForElements (CtorInits, false);
  Constant * RuntimeCtorInit = ConstantStruct::get (ST, CtorInits);

  //
  // Get the current set of static global constructors and add the new ctor
  // to the list.
  //
  std::vector<Constant *> CurrentCtors;
  GlobalVariable * GVCtor = M.getNamedGlobal ("llvm.global_ctors");
  if (GVCtor) {
    if (Constant * C = GVCtor->getInitializer()) {
      for (unsigned index = 0; index < C->getNumOperands(); ++index) {
        CurrentCtors.push_back (dyn_cast<Constant>(C->getOperand (index)));
	CurrentCtors.back()->dump();
      }
    }
  }

  //
  // The ctor list seems to be initialized in different orders on different
  // platforms, and the priority settings don't seem to work.  Examine the
  // module's platform string and take a best guess to the order.
  //
  if (M.getTargetTriple().find ("linux") == std::string::npos)
    CurrentCtors.insert (CurrentCtors.begin(), RuntimeCtorInit);
  else
    CurrentCtors.push_back (RuntimeCtorInit);

  assert (CurrentCtors.back()->getType() == RuntimeCtorInit->getType());

  //
  // Create a new initializer.
  //
  ArrayType * AT = ArrayType::get (RuntimeCtorInit-> getType(),
                                   CurrentCtors.size());
  Constant * NewInit=ConstantArray::get (AT, CurrentCtors);

  //
  // Create the new llvm.global_ctors global variable and remove the old one
  // if it existed.
  //
  Value * newGVCtor = new GlobalVariable (M,
                                          NewInit->getType(),
                                          false,
                                          GlobalValue::AppendingLinkage,
                                          NewInit,
                                          "llvm.global_ctors");
  if (GVCtor) {
    newGVCtor->takeName (GVCtor);
    GVCtor->eraseFromParent ();
  }

  return;
}

}
