//===- InlineFastChecks.cpp - Inline Fast Checks -------------------------- --//
// 
//                          The SAFECode Compiler 
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This pass replaces calls to fastlscheck within inline code to perform the
// check.  It is designed to provide the advantage of libLTO without libLTO.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "inline-fastchecks"

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include <vector>

namespace {
  STATISTIC (Inlined, "Number of Fast Checks Inlined");
}

namespace llvm {
  //
  // Pass: InlineFastChecks
  //
  // Description:
  //  This pass inlines fast checks to make them faster.
  //
  struct InlineFastChecks : public ModulePass {
   public:
    static char ID;
    InlineFastChecks() : ModulePass(ID) {}
     virtual bool runOnModule (Module & M);
     const char *getPassName() const {
       return "Inline fast checks transform";
     }
    
     virtual void getAnalysisUsage(AnalysisUsage &AU) const {
       return;
     }

   private:
     // Private methods
     bool inlineCheck (Function * F);
     bool createBodyFor (Function * F);
     bool createDebugBodyFor (Function * F);
     Value * castToInt (Value * Pointer, BasicBlock * BB);
     Value * addComparisons (BasicBlock *, Value *, Value *, Value *);
  };
}

using namespace llvm;

//
// Method: findChecks()
//
// Description:
//  Find the checks that need to be inlined and inline them.
//
// Inputs:
//  F - A pointer to the function.  Calls to this function will be inlined.
//      The pointer is allowed to be NULL.
//
// Return value:
//  true  - One or more calls to the check were inlined.
//  false - No calls to the check were inlined.
//
bool
llvm::InlineFastChecks::inlineCheck (Function * F) {
  //
  // Get the runtime function in the code.  If no calls to the run-time
  // function were added to the code, do nothing.
  //
  if (!F) return false;

  //
  // Iterate though all calls to the function and search for pointers that are
  // checked but only used in comparisons.  If so, then schedule the check
  // (i.e., the call) for removal.
  //
  bool modified = false;
  std::vector<CallInst *> CallsToInline;
  for (Value::use_iterator FU = F->use_begin(); FU != F->use_end(); ++FU) {
    //
    // We are only concerned about call instructions; any other use is of
    // no interest to the organization.
    //
    if (CallInst * CI = dyn_cast<CallInst>(*FU)) {
      //
      // If the call instruction has no uses, we can remove it.
      //
      if (CI->use_begin() == CI->use_end())
        CallsToInline.push_back (CI);
    }
  }

  //
  // Update the statistics and determine if we will modify anything.
  //
  if (CallsToInline.size()) {
    modified = true;
    Inlined += CallsToInline.size();
  }

  //
  // Inline all of the fast calls we found.
  //
  const DataLayout & TD = F->getParent()->getDataLayout();
  InlineFunctionInfo IFI;
  for (unsigned index = 0; index < CallsToInline.size(); ++index) {
    InlineFunction (CallsToInline[index], IFI);
  }

  return modified;
}

//
// Function: createFaultBlock()
//
// Description:
//  Create a basic block which will cause the program to terminate.
//
// Inputs:
//  F - A reference to a function to which a faulting basic block will be added.
//
static BasicBlock *
createFaultBlock (Function & F) {
  //
  // Create the basic block.
  //
  BasicBlock * faultBB = BasicBlock::Create (F.getContext(), "fault", &F);

  //
  // Terminate the basic block with an unreachable instruction.
  //
  Instruction * UI = new UnreachableInst (F.getContext(), faultBB);

  //
  // Add an instruction that will generate a trap.
  //
  LLVMContext & Context = F.getContext();
  Module * M = F.getParent();
  M->getOrInsertFunction ("abort", Type::getVoidTy (Context), NULL);
  CallInst::Create (M->getFunction ("abort"), "", UI);

  return faultBB;
}

//
// Function: createDebugFaultBlock()
//
// Description:
//  Create a basic block which will cause the program to report a memory safety
//  error.
//
// Inputs:
//  F - A reference to a function to which a faulting basic block will be added.
//
static BasicBlock *
createDebugFaultBlock (Function & F) {
  //
  // Create the basic block.
  //
  BasicBlock * faultBB = BasicBlock::Create (F.getContext(), "fault", &F);

  //
  // Terminate the basic block with a return instruction.
  //
  Instruction * Ret = ReturnInst::Create (F.getContext(), faultBB);

  //
  // Create needed types.
  //
  LLVMContext & Context = F.getContext();
  Type * Int8Type  = IntegerType::getInt8Ty (Context);

  //
  // Add a call to print the debug information.
  //
  Module * M = F.getParent();
  M->getOrInsertFunction ("failLSCheck",
                          Type::getVoidTy (Context),
                          PointerType::getUnqual(Int8Type),
                          PointerType::getUnqual(Int8Type),
                          IntegerType::getInt32Ty(Context),
                          PointerType::getUnqual(Int8Type),
                          IntegerType::getInt32Ty (Context),
                          NULL);
  std::vector<Value *> args;
  unsigned index = 0;
  for (Function::arg_iterator arg = F.arg_begin();
       arg != F.arg_end();
       ++arg, ++index) {
    if ((index < 3) || (index > 4)) {
      args.push_back (&*arg);
    }
  }
  CallInst::Create (M->getFunction ("failLSCheck"), args, "", Ret);

  return faultBB;
}

//
// Method: castToInt()
//
// Description:
//  Cast the given pointer value into an integer.
//
Value *
llvm::InlineFastChecks::castToInt (Value * Pointer, BasicBlock * BB) {
  //
  // Assert that the caller is giving us a pointer value.
  //
  assert (isa<PointerType>(Pointer->getType()));
  
  //
  // Get information on the size of pointers.
  //
  const DataLayout & TD = BB->getModule()->getDataLayout();

  //
  // Create the actual cast instrution.
  //
  return new PtrToIntInst (Pointer, TD.getIntPtrType(Pointer->getType()), "tmp", BB);
}

//
// Method: addComparisons()
//
// Description:
//  This function adds the comparisons needs for load/store checks.
//
// Return value:
//  A pointer to an LLVM boolean value representing the logical AND of the two
//  comparisons is returned.  If the value is true, then the pointer is within
//  bounds.  Otherwise, it is out of bounds.
//
Value *
llvm::InlineFastChecks::addComparisons (BasicBlock * BB,
                                        Value * Base,
                                        Value * Result,
                                        Value * Size) {
  //
  // Compare the base of the object to the pointer being checked.
  //
  ICmpInst * Compare1 = new ICmpInst (*BB,
                                      CmpInst::ICMP_ULE,
                                      Base,
                                      Result,
                                      "cmp1");

  //
  // Calculate the address of the first byte beyond the memory object
  //
  const DataLayout & TD = BB->getModule()->getDataLayout();
  Value * SizeInt = Size;
  if (SizeInt->getType() != TD.getIntPtrType(BB->getType())) {
    SizeInt = new ZExtInst (Size, TD.getIntPtrType(BB->getType()), "size", BB);
  }
  Value * LastByte = BinaryOperator::Create (Instruction::Add,
                                             Base,
                                             SizeInt,
                                             "lastbyte",
                                             BB);

  //
  // Compare the pointer to the first byte beyond the end of the memory object
  //
  Value * Compare2 = new ICmpInst (*BB,
                                   CmpInst::ICMP_ULT,
                                   Result,
                                   LastByte,
                                   "cmp2");

  //
  // Combine the results of both comparisons.
  //
  return (BinaryOperator::Create (Instruction::And,
                                  Compare1,
                                  Compare2,
                                  "and",
                                  BB));
}

//
// Method: createBodyFor()
//
// Description:
//  Create the function body for the fastlscheck() function.
//
// Inputs:
//  F - A pointer to a function with no body.  This pointer can be NULL.
//
bool
llvm::InlineFastChecks::createBodyFor (Function * F) {
  //
  // If the function does not exist, do nothing.
  //
  if (!F) return false;

  //
  // If the function has a body, do nothing.
  //
  if (!(F->isDeclaration())) return false;

  //
  // Create an entry block that will perform the comparisons and branch either
  // to the success block or the fault block.
  //
  LLVMContext & Context = F->getContext();
  BasicBlock * entryBB = BasicBlock::Create (Context, "entry", F);

  //
  // Create a basic block that just returns.
  //
  BasicBlock * goodBB = BasicBlock::Create (Context, "pass", F);
  ReturnInst::Create (F->getContext(), goodBB);

  //
  // Create a basic block that handles the run-time check failures.
  //
  BasicBlock * faultBB = createFaultBlock (*F);

  //
  // Add instructions to the entry block to compare the first dereferenced
  // address.
  // 
  Function::arg_iterator arg = F->arg_begin();
  Value * Base = castToInt (arg++, entryBB);
  Value * Result = castToInt (arg++, entryBB);
  Value * Size = arg++;
  Value * MemSize = arg++;
  Value * Sum1 = addComparisons (entryBB, Base, Result, Size);

  //
  // Now add instructions to compare the last byte dereferenced with the
  // memory object's bounds.
  //
  const DataLayout & TD = F->getParent()->getDataLayout();
  Value * SizeInt = MemSize;
  if (SizeInt->getType() != TD.getIntPtrType(entryBB->getType())) {
    SizeInt = new ZExtInst (MemSize, TD.getIntPtrType(entryBB->getType()), "size", entryBB);
  }
  Value * LastByte = BinaryOperator::Create (Instruction::Add,
                                             Result,
                                             SizeInt,
                                             "lastbyte",
                                             entryBB);
  Constant * MinusOne = ConstantInt::getSigned (TD.getIntPtrType(entryBB->getType()), -1);
  LastByte = BinaryOperator::Create (Instruction::Add,
                                     LastByte,
                                     MinusOne,
                                     "lastbyte",
                                     entryBB);
  Value * Sum2 = addComparisons (entryBB, Base, LastByte, Size);

  //
  // The check only passes if both the first and last byte accessed are within
  // bounds.
  //
  Value * Sum = BinaryOperator::Create (Instruction::And,
                                        Sum1,
                                        Sum2,
                                        "and",
                                        entryBB);


  //
  // Create the branch instruction.
  //
  BranchInst::Create (goodBB, faultBB, Sum, entryBB);

  //
  // Make the function internal.
  //
  F->setLinkage (GlobalValue::InternalLinkage);
  return true;
}

//
// Method: createDebugBodyFor()
//
// Description:
//  Create the function body for the fastlscheck_debug() function.
//
// Inputs:
//  F - A pointer to a function with no body.  This pointer can be NULL.
//
bool
llvm::InlineFastChecks::createDebugBodyFor (Function * F) {
  //
  // If the function does not exist, do nothing.
  //
  if (!F) return false;

  //
  // If the function has a body, do nothing.
  //
  if (!(F->isDeclaration())) return false;

  //
  // Create an entry block that will perform the comparisons and branch either
  // to the success block or the fault block.
  //
  LLVMContext & Context = F->getContext();
  BasicBlock * entryBB = BasicBlock::Create (Context, "entry", F);

  //
  // Create a basic block that just returns.
  //
  BasicBlock * goodBB = BasicBlock::Create (Context, "pass", F);
  ReturnInst::Create (F->getContext(), goodBB);

  //
  // Create a basic block that handles the run-time check failures.
  //
  BasicBlock * faultBB = createDebugFaultBlock (*F);

  //
  // Add instructions to the entry block to compare the first dereferenced
  // address.
  // 
  Function::arg_iterator arg = F->arg_begin();
  Value * Base = castToInt (arg++, entryBB);
  Value * Result = castToInt (arg++, entryBB);
  Value * Size = arg++;
  Value * MemSize = arg++;
  Value * Sum1 = addComparisons (entryBB, Base, Result, Size);

  //
  // Now add instructions to compare the last byte dereferenced with the
  // memory object's bounds.
  //
  const DataLayout & TD = F->getParent()->getDataLayout();
  Value * SizeInt = MemSize;
  if (SizeInt->getType() != TD.getIntPtrType(entryBB->getType())) {
    SizeInt = new ZExtInst (MemSize, TD.getIntPtrType(entryBB->getType()), "size", entryBB);
  }
  Value * LastByte = BinaryOperator::Create (Instruction::Add,
                                             Result,
                                             SizeInt,
                                             "lastbyte",
                                             entryBB);
  Constant * MinusOne = ConstantInt::getSigned (TD.getIntPtrType(entryBB->getType()), -1);
  LastByte = BinaryOperator::Create (Instruction::Add,
                                     LastByte,
                                     MinusOne,
                                     "lastbyte",
                                     entryBB);
  Value * Sum2 = addComparisons (entryBB, Base, LastByte, Size);

  //
  // The check only passes if both the first and last byte accessed are within
  // bounds.
  //
  Value * Sum = BinaryOperator::Create (Instruction::And,
                                        Sum1,
                                        Sum2,
                                        "and",
                                        entryBB);

  //
  // Create the branch instruction.  Both comparisons must return true for the
  // pointer to be within bounds.
  //
  BranchInst::Create (goodBB, faultBB, Sum, entryBB);

  //
  // Make the function internal.
  //
  F->setLinkage (GlobalValue::InternalLinkage);
  return true;
}

bool
llvm::InlineFastChecks::runOnModule (Module & M) {
  //
  // Create a function body for the fastlscheck call.
  //
  createBodyFor (M.getFunction ("fastlscheck"));
  createDebugBodyFor (M.getFunction ("fastlscheck_debug"));

  //
  // Search for call sites to the function and forcibly inline them.
  //
  inlineCheck (M.getFunction ("fastlscheck"));
  inlineCheck (M.getFunction ("fastlscheck_debug"));
  return true;
}

namespace llvm {
  char InlineFastChecks::ID = 0;

  static RegisterPass<InlineFastChecks>
  X ("inline-fastchecks", "Inline fast run-time checks", true);

  ModulePass * createInlineFastChecksPass (void) {
    return new InlineFastChecks();
  }
}
