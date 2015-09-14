//===- FaultInjector.cpp - Insert faults into programs -----------------------//
// 
//                          The SAFECode Compiler 
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file implements a pass that transforms the program to add the following
// kind of faults:
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "FaultInjector"

#include "dsa/DSGraph.h"

#include "llvm/DebugInfo.h"
#include "llvm/Constants.h"
#include "llvm/Instructions.h"
#include "llvm/IntrinsicInst.h"
#include "llvm/LLVMContext.h"
#include "llvm/Module.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/GetElementPtrTypeIterator.h"

#include "safecode/FaultInjector.h"
#include "SCUtils.h"

#include <cstdlib>
#include <limits.h>
#include <iostream>
#include <vector>

using namespace llvm;

char llvm::FaultInjector::ID = 0;

// Register the pass and tell a bad joke all at the same time.
// I know, I know; it's my own darn fault...
RegisterPass<FaultInjector> MyFault ("faultinjector", "Insert Faults");

///////////////////////////////////////////////////////////////////////////
// Command line options
///////////////////////////////////////////////////////////////////////////
cl::opt<bool>
InjectEasyDPFaults ("inject-easydp",
                    cl::init(false),
                    cl::desc("Inject Trivial Dangling Pointer Dereferences"));

cl::opt<bool>
InjectHardDPFaults ("inject-harddp",
                    cl::init(false),
                    cl::desc("Inject Non-Trivial Dangling Pointer Dereferences"));

cl::opt<bool>
InjectRealDPFaults ("inject-realdp",
                    cl::init(false),
                    cl::desc("Inject Only Dangling Pointer Dereferences"));

cl::opt<bool>
InjectBadSizes ("inject-badsize",
                cl::init(false),
                cl::desc("Inject Array Allocations of the Wrong Size"));

cl::opt<bool>
InjectBadIndices ("inject-badindices",
                  cl::init(false),
                  cl::desc("Inject Bad Indices in GEPs"));

cl::opt<bool>
InjectUninitUses ("inject-uninituses",
                  cl::init(false),
                  cl::desc("Inject Uses of Uninitialized Pointers"));

cl::opt<int>
Seed ("seed", cl::init(1),
      cl::desc("Seed Value for Random Number Generator"));

cl::opt<int>
Frequency ("freq", cl::init(100),
           cl::desc("Probability of Inserting a Fault"));

cl::list<std::string>
Funcs ("funcs",
       cl::value_desc("list"),
       cl::CommaSeparated,
       cl::desc ("List of functions to process"));

//
// Basic LLVM Types
//
static const Type * Int32Type = 0;

namespace {
  ///////////////////////////////////////////////////////////////////////////
  // Pass Statistics
  ///////////////////////////////////////////////////////////////////////////
  STATISTIC (DPFaults,       "Number of Dangling Pointer Faults Injected");
  STATISTIC (BadSizes,       "Number of Bad Allocation Size Faults Injected");
  STATISTIC (BadIndices,     "Number of Bad Indexing Faults Injected");
  STATISTIC (UsesBeforeInit, "Number of Injected Uses Before Initialization");
  STATISTIC (NumFuncs,       "Number of Functions Examined");

  // Threshold for determining whether a fault will be inserted
  int threshold;
}

///////////////////////////////////////////////////////////////////////////
// Static Functions
///////////////////////////////////////////////////////////////////////////

//
// Function: doFault()
//
// Description:
//  Uses random number generation to determine if a fault should be inserted.
//
// Return Value:
//  true  - A fault should be inserted.
//  false - A fault should not be inserted.
//
// Pre-conditions:
//  1) The random number generator routines should have been seeded.
//  2) The threshold variable should have been calculated.
//
static inline bool
doFault () {
  if (rand() < threshold)
    return true;
  else
    return false;
}

//
// Function: typeContainsPointer()
//
// Description:
//  This function determines whether the specified LLVM type is either a
//  pointer type or a derived type that contains a pointer.
//
// Inputs:
//  Ty      - The type created by the allocation.
//  Context - The LLVM Context in which to add integers.
//
// Outputs:
//  Indices - A vector of indices that can be used to create a GEP to the
//            pointer field of the type.  This vector is *always* modified by
//            this function, even if no pointer type is found.
//
// Return value:
//  true  - The type contains a pointer.
//  false - This function could not prove that this type contains a pointer.
//
static inline bool
typeContainsPointer (const Type * Ty, std::vector<Value *> & Indices,
                     LLVMContext * Context) {
  //
  // If this is a pointer type, stop the recursion.  We've found our pointer.
  //
  if (isa<PointerType>(Ty)) {
    return true;
  }

  //
  // If this is an array type or vector type, search within the type of
  // element.
  //
  if (const ArrayType * AT = dyn_cast<ArrayType>(Ty)) {
    Indices.push_back (ConstantInt::get (Int32Type, 0));
    return typeContainsPointer (AT->getElementType(), Indices, Context);
  }

  if (const VectorType * VT = dyn_cast<VectorType>(Ty)) {
    Indices.push_back (ConstantInt::get (Int32Type, 0));
    return typeContainsPointer (VT->getElementType(), Indices, Context);
  }

  //
  // If this is a structure type, search for a pointer within each element type
  // of the structure.
  //
  if (const StructType * ST = dyn_cast<StructType>(Ty)) {
    for (unsigned index = 0; index < ST->getNumElements(); ++index) {
      Indices.push_back (ConstantInt::get (Int32Type, index));
      if (typeContainsPointer (ST->getElementType(index), Indices, Context)) {
        return true;
      } else {
        Indices.pop_back ();
      }
    }
  }

  //
  // We don't know what this is.  Say it doesn't contain a pointer.
  //
  return false;
}

//
// Function: printSourceInfo()
//
// Description:
//  Print source file and line number information about the instruction to
//  standard output.
//
static void
printSourceInfo (std::string errorType, Instruction * I) {
  //
  // Print out where the fault will be inserted in the source code.
  // If we can't find the source line information, use a dummy line number and
  // the function name by default.
  //
  std::string fname = I->getParent()->getParent()->getNameStr();
  std::string funcname = fname;
  uint64_t lineno = 0;
  unsigned dbgKind = I->getContext().getMDKindID("dbg");
  if (MDNode *Dbg = I->getMetadata(dbgKind)) {
    DILocation Loc (Dbg);
    fname = Loc.getDirectory().str() + Loc.getFilename().str();
    lineno   = Loc.getLineNumber();
  }

  std::cout << "Inject: " << errorType << ": "
            << funcname   << ": " << fname << ": " << lineno << "\n";
  return;
}

static inline Function *
createFreeFunction (Module & M) {
  const Type * VoidType = Type::getVoidTy(M.getContext());
  return (Function *) M.getOrInsertFunction ("free",
                                             VoidType,
                                             getVoidPtrType(M),
                                             NULL);
}

//
// Function: getFunctionList()
//
// Description:
//  Determine which functions should be processed.
//
void
getFunctionList (Module & M, std::vector<Function *> & List) {
  //
  // If no functions were listed on the command line, then process *all*
  // functions within the module.  Otherwise, create a list of only those given
  // on the command line.
  //
  if (Funcs.size() == 0) {
    for (Module::iterator F = M.begin(), E = M.end(); F != E; ++F) {
      List.push_back (F);
    }
  } else {
    for (unsigned index = 0; index < Funcs.size(); ++index) {
      if (Function * F = M.getFunction(Funcs[index]))
        List.push_back (F);
    }
  }

  //
  // Update the statistics on how many functions we'll examine.
  //
  NumFuncs += List.size();
  return;
}

//
// Method: insertEasyDanglingPointers()
//
// Description:
//  Insert dangling pointer dereferences into the code.  This is done by
//  finding load/store instructions and inserting a free on the pointer to
//  ensure the dereference (and all future dereferences) are illegal.
//
// Return value:
//  true  - The module was modified.
//  false - The module was left unmodified.
//
// Notes:
//  This code utilizes DSA to ensure that the pointer can pointer to heap
//  memory (although the pointer is allowed to alias global and stack memory).
//
bool
FaultInjector::insertEasyDanglingPointers (Function & F) {
  //
  // Ensure that we can get analysis information for this function.
  //
  if (!(TDPass->hasDSGraph(F)))
    return false;

  //
  // Scan through each instruction of the function looking for load and store
  // instructions.  Free the pointer right before.
  //
  DSGraph * DSG = TDPass->getDSGraph(F);
  for (Function::iterator fI = F.begin(), fE = F.end(); fI != fE; ++fI) {
    BasicBlock & BB = *fI;
    for (BasicBlock::iterator bI = BB.begin(), bE = BB.end(); bI != bE; ++bI) {
      Instruction * I = bI;

      //
      // Look to see if there is an instruction that uses a pointer.  If so,
      // then free the pointer before the use.
      //
      Value * Pointer = 0;
      if (LoadInst * LI = dyn_cast<LoadInst>(I))
        Pointer = LI->getPointerOperand();
      else if (StoreInst * SI = dyn_cast<StoreInst>(I))
        Pointer = SI->getPointerOperand();
      else
        continue;

      //
      // Check to ensure that this pointer aliases with the heap.  If so, go
      // ahead and add the free.  Note that we may introduce an invalid free,
      // but we're injecting errors, so I think that's okay.
      //
      DSNode * Node = DSG->getNodeForValue(Pointer).getNode();
      if (Node && (Node->isHeapNode())) {
        //
        // Avoid free'ing pointers that are trivially stack objects or global
        // variables.
        //
        if (isa<GlobalValue>(Pointer->stripPointerCasts()) ||
            isa<AllocaInst>(Pointer->stripPointerCasts())) {
          continue;
        }

        // Skip if we should not insert a fault.
        if (!doFault()) continue;

        //
        // Print information about where the fault is being inserted.
        //
        printSourceInfo ("Easy dangling pointer", I);

        CallInst::Create (Free, Pointer, "", I);
        ++DPFaults;
      }
    }
  }

  return (DPFaults > 0);
}

//
// Method: insertHardDanglingPointers()
//
// Description:
//  Insert dangling pointer dereferences into the code.  This is done by
//  finding instructions that store pointers to memory and free'ing those
//  pointers before the store.  Subsequent loads and uses of the pointer will
//  cause a dangling pointer dereference.
//
// Return value:
//  true  - The module was modified.
//  false - The module was left unmodified.
//
// Notes:
//  This code utilizes DSA to ensure that the pointer can point to heap
//  memory (although the pointer is allowed to alias global and stack memory).
//
bool
FaultInjector::insertHardDanglingPointers (Function & F) {
  //
  // Ensure that we can get analysis information for this function.
  //
  if (!(TDPass->hasDSGraph(F)))
    return false;

  //
  // Scan through each instruction of the function looking for store
  // instructions that store a pointer to memory.  Free the pointer right
  // before the store instruction.
  //
  DSGraph * DSG = TDPass->getDSGraph(F);
  for (Function::iterator fI = F.begin(), fE = F.end(); fI != fE; ++fI) {
    BasicBlock & BB = *fI;
    for (BasicBlock::iterator bI = BB.begin(), bE = BB.end(); bI != bE; ++bI) {
      Instruction * I = bI;

      //
      // Look to see if there is an instruction that stores a pointer to
      // memory.  If so, then free the pointer before the store.
      //
      if (StoreInst * SI = dyn_cast<StoreInst>(I)) {
        if (isa<PointerType>(SI->getOperand(0)->getType())) {
          Value * Pointer = SI->getOperand(0);

          //
          // Check to ensure that the pointer aliases with the heap.  If so, go
          // ahead and add the free.  Note that we may introduce an invalid
          // free, but we're injecting errors, so I think that's okay.
          //
          DSNode * Node = DSG->getNodeForValue(Pointer).getNode();
          if (Node && (Node->isHeapNode())) {
            // Skip if we should not insert a fault.
            if (!doFault()) continue;

            //
            // Print information about where the fault is being inserted.
            //
            printSourceInfo ("Hard dangling pointer", I);

            CallInst::Create (Free, Pointer, "", I);
            ++DPFaults;
          }
        }
      }
    }
  }

  return (DPFaults > 0);
}

//
// Method: insertRealDanglingPointers()
//
// Description:
//  Insert dangling pointer dereferences into the code.  This is done by
//  finding heap allocation instructions and adding code to free the allocated
//  pointer.  These errors will be more trivial than the hard dangling pointer
//  injection method but will also be more accurate (i.e., it will only free
//  heap objects and only cause dangling pointer errors; it will *not* insert
//  other invalid free errors).
//
// Return value:
//  true  - The module was modified.
//  false - The module was left unmodified.
//
bool
FaultInjector::insertRealDanglingPointers (Function & F) {
  //
  // Scan through each instruction of the function looking for malloc
  // instructions.  Free the pointer immediently after the allocation.
  //
#if 0
  std::vector<MallocInst *> Worklist;
  for (Function::iterator fI = F.begin(), fE = F.end(); fI != fE; ++fI) {
    BasicBlock & BB = *fI;
    for (BasicBlock::iterator bI = BB.begin(), bE = BB.end(); bI != bE; ++bI) {
      Instruction * I = bI;

      //
      // Look to see if there is an instruction that stores a pointer to
      // memory.  If so, then free the pointer before the store.
      //
      if (MallocInst * MI = dyn_cast<MallocInst>(I)) {
        // Skip if we should not insert a fault.
        if (!doFault()) continue;

        //
        // Add the malloc instruction to the list of places to insert frees.
        // We do this here to avoid iterator invalidation.
        //
        Worklist.push_back (MI);
      }
    }
  }

  //
  // Insert a free after every malloc in the work list.
  //
  while (Worklist.size()) {
    MallocInst * MI = Worklist.back();
    Worklist.pop_back();

    BasicBlock::iterator InsertPt = MI;
    ++InsertPt;

    //
    // Print information about where the fault is being inserted.
    //
    printSourceInfo ("Real dangling pointer", MI);

    //
    // Insert a call to free to deallocate the allocated memory.
    //
    new FreeInst (MI, InsertPt);
    ++DPFaults;
  }

  return (DPFaults > 0);
#else
  return false;
#endif
}

//
// Method: insertBadAllocationSizes()
//
// Description:
//  This method will look for allocations and change their size to be
//  incorrect.  It does the following:
//    o) Changes the number of array elements allocated by alloca and malloc.
//
// Return value:
//  true  - The module was modified.
//  false - The module was left unmodified.
//
bool
FaultInjector::insertBadAllocationSizes  (Function & F) {
  // Worklist of allocation sites to rewrite
  std::vector<AllocaInst * > WorkList;

  for (Function::iterator fI = F.begin(), fE = F.end(); fI != fE; ++fI) {
    BasicBlock & BB = *fI;
    for (BasicBlock::iterator I = BB.begin(), bE = BB.end(); I != bE; ++I) {
      if (AllocaInst * AI = dyn_cast<AllocaInst>(I)) {
        if (AI->isArrayAllocation()) {
          // Skip if we should not insert a fault.
          if (!doFault()) continue;

          WorkList.push_back(AI);
        }
      }
    }
  }

  while (WorkList.size()) {
    AllocaInst * AI = WorkList.back();
    WorkList.pop_back();

    //
    // Print information about where the fault is being inserted.
    //
    printSourceInfo ("Bad allocation size", AI);

    Instruction * NewAlloc = 0;
    NewAlloc =  new AllocaInst (AI->getAllocatedType(),
                                ConstantInt::get(Int32Type,0),
                                AI->getAlignment(),
                                AI->getName(),
                                AI);
    AI->replaceAllUsesWith (NewAlloc);
    AI->eraseFromParent();
    ++BadSizes;
  }

  //
  // Try harder to make bad allocation sizes.
  //
  WorkList.clear();
  for (Function::iterator fI = F.begin(), fE = F.end(); fI != fE; ++fI) {
    BasicBlock & BB = *fI;
    for (BasicBlock::iterator I = BB.begin(), bE = BB.end(); I != bE; ++I) {
      if (AllocaInst * AI = dyn_cast<AllocaInst>(I)) {
        //
        // Determine if this is a data type that we can make smaller.
        //
        if (((TD->getTypeAllocSize(AI->getAllocatedType())) > 4) && doFault()) {
          WorkList.push_back(AI);
        }
      }
    }
  }

  //
  // Replace these allocations with an allocation of an integer and cast the
  // result back into the appropriate type.
  //
  while (WorkList.size()) {
    AllocaInst * AI = WorkList.back();
    WorkList.pop_back();

    Instruction * NewAlloc = 0;
    NewAlloc =  new AllocaInst (Int32Type,
                                AI->getArraySize(),
                                AI->getAlignment(),
                                AI->getName(),
                                AI);
    NewAlloc = castTo (NewAlloc, AI->getType(), "", AI);
    AI->replaceAllUsesWith (NewAlloc);
    AI->eraseFromParent();
    ++BadSizes;
  }

  return (BadSizes > 0);
}

//
// Methods: insertBadIndexing()
//
// Description:
//  This method modifieds GEP indexing expressions so that their indices are
//  (most likely) below the bounds of the object pointed to by the source
//  pointer.  It does this by modifying the first index to be -1.
//
// Return value:
//  true  - One or more changes were made to the program.
//  false - No changes were made to the program.
//
bool
FaultInjector::insertBadIndexing (Function & F) {
  // Worklist of allocation sites to rewrite
  std::vector<GetElementPtrInst *> WorkList;

  //
  // Find GEP instructions that index into an array.  Add these to the
  // worklist.
  //
  for (Function::iterator fI = F.begin(), fE = F.end(); fI != fE; ++fI) {
    BasicBlock & BB = *fI;
    for (BasicBlock::iterator I = BB.begin(), bE = BB.end(); I != bE; ++I) {
      if (GetElementPtrInst * GEP = dyn_cast<GetElementPtrInst>(I)) {
        // Skip if we should not insert a fault.
        if (!doFault()) continue;

        WorkList.push_back (GEP);
      }
    }
  }

  // Flag whether the program was modified
  bool modified = (WorkList.size() > 0);

  //
  // Iterator through the worklist and transform each GEP.
  //
  while (WorkList.size()) {
    GetElementPtrInst * GEP = WorkList.back();
    WorkList.pop_back();

    //
    // Print out where the fault will be inserted in the source code.
    //
    printSourceInfo ("Bad indexing", GEP);

    // The index arguments to the new GEP
    std::vector<Value *> args;

    //
    // Create a copy of the GEP's indices.
    //
    User::op_iterator i = GEP->idx_begin();
    if (i == GEP->idx_end()) continue;
    args.push_back (ConstantInt::get (Int32Type, INT_MAX, true));
    for (++i; i != GEP->idx_end(); ++i) {
      args.push_back (*i);
    }

    //
    // Create the new GEP instruction.
    //
    Value * Pointer = GEP->getPointerOperand();
    Twine name = GEP->getName() + "badindex";
    GetElementPtrInst * NewGEP = GetElementPtrInst::Create (Pointer,
                                                            args.begin(),
                                                            args.end(),
                                                            name,
                                                            GEP);
    GEP->replaceAllUsesWith (NewGEP);
    GEP->eraseFromParent();
    ++BadIndices;
  }

  return modified;
}

//
// Method: insertUninitializedUse()
//
// Description:
//  This method will insert uses of uninitialized pointers.
//
// Inputs:
//  F - The function in which to inject errors.
//
// Return value:
//  true  - This method modified the given function.
//  false - This method did not modify the given function.
//
// Pre-conditions:
//  The seed value for the random number generator used to determine if we
//  inject faults must already have been called.
//
// Post-conditions:
//  The global statistics variable will have been updated to reflect the number
//  of uninitialized uses added.
//
bool
FaultInjector::insertUninitializedUse (Function & F) {
  // Worklist of allocation sites to instrument
  std::map<AllocaInst *, std::vector<Value *> > WorkList;

  //
  // Look for allocation instructions that allocate structures with pointers
  // in them.
  //
  for (Function::iterator fI = F.begin(), fE = F.end(); fI != fE; ++fI) {
    BasicBlock & BB = *fI;
    for (BasicBlock::iterator I = BB.begin(), bE = BB.end(); I != bE; ++I) {
      if (AllocaInst * AI = dyn_cast<AllocaInst>(I)) {
        //
        // Only inject a fault if the allocated memory has a pointer in it.
        //
        std::vector<Value *> Indices;
        Indices.push_back (ConstantInt::get (Int32Type, 0));
        if (typeContainsPointer (AI->getAllocatedType(),
                                 Indices,
                                 &(F.getContext()))) {
          // Skip if we should not insert a fault.
          if (!doFault()) continue;
          WorkList.insert(std::make_pair (AI, Indices));
        }
      }
    }
  }

  //
  // Flag whether we'll have modified something.
  //
  bool modified = (WorkList.size() > 0);

  std::map<AllocaInst *, std::vector<Value *> >::iterator i;
  for (i = WorkList.begin(); i != WorkList.end(); ++i) {
    // Get the allocation instruction which we will use.
    AllocaInst * AI = i->first;

    // Get the set of indices that we found for accessing the pointer element
    std::vector<Value *> Indices = i->second;

    //
    // Print information about where the fault is being inserted.
    //
    printSourceInfo ("Uninitialized pointer", AI);

    //
    // Find the insertion point; it should be the next instruction after the
    // allocation.
    //
    BasicBlock::iterator InsertPt = AI;
    ++InsertPt;

    //
    // Insert a GEP expression for the pointer using the indices we found when
    // we went searching for a pointer value.
    //
    GetElementPtrInst * GEP = GetElementPtrInst::Create (AI,
                                                         Indices.begin(),
                                                         Indices.end(),
                                                         "gep",
                                                         InsertPt);

    //
    // Now load the uninitialized pointer.
    //
    LoadInst * BadPtr = new LoadInst (GEP, "badptr", InsertPt);

    //
    // Check to see if the type of the loaded pointer is a function pointer.
    // If so, we cannot create a load from it.
    //
    const PointerType * PT = dyn_cast<PointerType>(BadPtr->getType());
    assert (PT && "Load of non-pointer type!\n");
    if (isa<FunctionType>(PT->getElementType())) continue;

    //
    // Now my evil plan is complete!  Dereference this pointer and take the
    // first step into oblivion!
    //
    new LoadInst (BadPtr, "shouldfault", true, InsertPt);

    //
    // Update the statistics.
    //
    ++UsesBeforeInit;
  }

  return modified;
}

//
// Method: runOnModule()
//
// Description:
//  This is where the pass begin execution.
//
// Return value:
//  true  - The module was modified.
//  false - The module was left unmodified.
//
bool
FaultInjector::runOnModule(Module &M) {
  // Track whether anything has been modified
  bool modified = false;

  //
  // Create needed LLVM types.
  //
  Int32Type = IntegerType::getInt32Ty(M.getContext());

  // Get analysis results from DSA.
  TDPass = &getAnalysis<TDDataStructures>();

  // Get information on the target architecture for this program
  TD     = &getAnalysis<DataLayout>();

  // Initialize the random number generator
  srand (Seed);

  // Calculate the threshold for when a fault should be inserted
  threshold = (RAND_MAX / 100 * Frequency);

  // Create the heap deallocation function
  Free = createFreeFunction (M);

  // List of functions to process
  std::vector<Function *> FunctionList;

  // Process each function
  getFunctionList (M, FunctionList);
  while (FunctionList.size()) {
    Function * F = FunctionList.back();
    FunctionList.pop_back();

    // Insert dangling pointer errors
    if (InjectEasyDPFaults) modified |= insertEasyDanglingPointers(*F);
    if (InjectHardDPFaults) modified |= insertHardDanglingPointers(*F);
    if (InjectRealDPFaults) modified |= insertRealDanglingPointers(*F);

    // Insert bad allocation sizes
    if (InjectBadSizes) modified |= insertBadAllocationSizes (*F);

    // Insert incorrect indices in GEPs
    if (InjectBadIndices) modified |= insertBadIndexing (*F);

    // Insert uses of uninitialized pointers
    if (InjectUninitUses) modified |= insertUninitializedUse (*F);
  }

  return modified;
}
