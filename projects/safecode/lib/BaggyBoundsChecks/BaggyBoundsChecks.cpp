//===- BaggyBoundChecks.cpp - Instrumentation for Baggy Bounds ------------ --//
// 
//                          The SAFECode Compiler 
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This pass aligns globals and stack allocated values to the correct power of 
// two boundary.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "baggy-bound-checks"

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/TypeBuilder.h"
#include "llvm/IR/Value.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include "safecode/BaggyBoundsChecks.h"
#include "safecode/Runtime/BBMetaData.h"

#include <iostream>
#include <set>
#include <string>
#include <functional>

static const unsigned SLOT_SIZE = 4;
static const unsigned SLOT = 16;

using namespace llvm;

namespace llvm {

// Identifier variable for the pass
char InsertBaggyBoundsChecks::ID = 0;

// Statistics

// Register the pass
static RegisterPass<InsertBaggyBoundsChecks> P("baggy bounds aligning", 
                                               "Baggy Bounds Transform");

//
// Function: findP2Size()
//
// Description:
//  Find the power-of-two size that is greater than or equal to the specified
//  size.  Note that we will round small sizes up to SLOT_SIZE.
//
// Inputs:
//  objectSize - The size of the original object in bytes.
//
// Return value:
//  The exponent of the required size rounded to a power of two.  For example,
//  if we need 8 (2^3) bytes, we'd return 3.
//
static inline unsigned
findP2Size (unsigned long objectSize) {
  unsigned int size = SLOT_SIZE;
  while (((unsigned int)(1u<<size)) < objectSize) {
    ++size;
  }

  return size;
}

//
// Description:
//  Define BBMetaData struct type using TypeBuilder template. So for global and 
//  stack variables, we can use this type to record their metadata when padding 
//  and aligning them.
//
template<bool xcompile> class TypeBuilder<BBMetaData, xcompile> {
public:
  static  StructType* get(LLVMContext& context) {
    return StructType::get(
      TypeBuilder<types::i<32>, xcompile>::get(context),
      TypeBuilder<types::i<8>*, xcompile>::get(context),
      NULL);
   }
};

//
// Function: mustAdjustGlobalValue()
//
// Description:
//  This function determines whether the global value must be adjusted for
//  baggy bounds checking.
//
// Return value:
//  0 - The value does not need to be adjusted for baggy bounds checking.
//  Otherwise, a pointer to the value is returned.
//
GlobalVariable *
mustAdjustGlobalValue (GlobalValue * V) {
  //
  // Only modify global variables.  Everything else is left unchanged.
  //
  GlobalVariable * GV = dyn_cast<GlobalVariable>(V);
  if (!GV) return 0;

  //
  // Don't adjust a global which has an opaque type.
  //
  if (StructType * ST = dyn_cast<StructType>(GV->getType()->getElementType())) {
    if (ST->isOpaque()) {
      return 0;
    }
  }

  //
  // Don't modify external global variables or variables with no uses.
  // 
  //if (GV->isDeclaration()) {
    //return 0;
 // }

  //
  // Don't bother modifying the size of metadata.
  //
  if (!strcmp(GV->getSection(), "llvm.metadata")) return 0;

  std::string name = GV->getName();
  if (strncmp(name.c_str(), "llvm.", 5) == 0) return 0;
  if (strncmp(name.c_str(), "baggy.", 6) == 0) return 0;
  if (strncmp(name.c_str(), "__poolalloc", 11) == 0) return 0;

  // Don't modify globals in the exitcall section of the Linux kernel
  if (!strcmp(GV->getSection(), ".exitcall.exit")) return 0;

  //
  // Don't modify globals that are not emitted into the final executable.
  //
  if (GV->hasAvailableExternallyLinkage()) return 0;

  return GV;
}

//
// Method: adjustGlobalValue()
//
// Description:
//  This method adjusts the size and alignment of a global variable to suit
//  baggy bounds checking.
//

void
InsertBaggyBoundsChecks::adjustGlobalValue (GlobalValue * V) {
  //
  // Only modify global variables.  Everything else is left unchanged.
  //
  GlobalVariable * GV = mustAdjustGlobalValue(V);
  if (!GV) return;
  if (!GV->hasInitializer()) return;

  //
  // Find the greatest power-of-two size that is larger than the object's
  // current size.
  //
  Type * GlobalType = GV->getType()->getElementType();
  unsigned long objectSize = TD->getTypeAllocSize(GlobalType);
  if (!objectSize) return;
  unsigned long adjustedSize = objectSize + sizeof(BBMetaData);
  unsigned int size = findP2Size (adjustedSize);

  //
  // Find the optimal alignment for the memory object.  Note that we can use
  // a larger alignment than needed.
  //
  unsigned int alignment = 1u << (size); 
  if (GV->getAlignment() > alignment) alignment = GV->getAlignment();

  //
  // Create a structure type.  The first element will be the global memory
  // object; the second will be an array of bytes that will pad the size out;
  // the third will be the metadata for this object.
  //
  Type *Int8Type = Type::getInt8Ty(GV->getContext());
  Type *newType1 = ArrayType::get (Int8Type, (1u<<size) - adjustedSize);
  Type *metadataType = TypeBuilder<BBMetaData, false>::get(GV->getContext());
  StructType *newType = StructType::get(GlobalType,
                                        newType1,
                                        metadataType,
                                        NULL);

  //
  // Store the object's size into a metadata variable.
  //
  Type *Int32Type = Type::getInt32Ty (GV->getContext());
  Type *Int8Ptr = PointerType::get(Int8Type, 0);
  std::vector<Constant *> metaVals(2);
  metaVals[0] = ConstantInt::get(Int32Type, objectSize);
  metaVals[1] = Constant::getNullValue(Int8Ptr);
  Constant *c = ConstantStruct::get((StructType *)metadataType, metaVals);
  GlobalVariable *metaData = new GlobalVariable (*(GV->getParent()),
                                                 metadataType,
                                                 GV->isConstant(),
                                                 GlobalValue::PrivateLinkage,
                                                 c,
                                                 "meta." + GV->getName());

  //
  // Create a global initializer.  The first element has the initializer of
  // the original memory object, the second initializes the padding array,
  // the third initializes the object's metadata using the metadata variable.
  //
  std::vector<Constant *> vals(3);
  vals[0] = GV->getInitializer();
  vals[1] = Constant::getNullValue(newType1);
  vals[2] = metaData->getInitializer();
  c = ConstantStruct::get(newType, vals);

  //
  // Create the new global memory object with the correct alignment.
  //
  GlobalValue::LinkageTypes LinkTy = GV->getLinkage();
  if (GV->getLinkage() == GlobalValue::CommonLinkage)
    LinkTy = GlobalValue::ExternalLinkage;

  GlobalVariable *GV_new = new GlobalVariable (*(GV->getParent()),
                                                 newType,
                                                 GV->isConstant(),
                                                 LinkTy,
                                                 c,
                                                 "baggy." + GV->getName());
  GV_new->copyAttributesFrom (GV);
  GV_new->setAlignment(1u<<size);
  GV_new->takeName (GV);
    
  //
  // Create a GEP expression that will represent the global value and replace
  // all uses of the global value with the new constant GEP.
  //
  Value *Zero = ConstantInt::getSigned(Int32Type, 0);
  Value *idx1[2] = {Zero, Zero};
  Constant *init = ConstantExpr::getGetElementPtr(newType, GV_new, idx1, 2);
  GV->replaceAllUsesWith(init);
  GV->eraseFromParent();

  return;
}

//
// Method: adjustAlloca()
//
// Description:
//  Modify the specified alloca instruction (if necessary) to give it the
//  needed alignment and padding for baggy bounds checking.
//
void
InsertBaggyBoundsChecks::adjustAlloca (AllocaInst * AI) {
  //
  // Get the power-of-two size for the alloca.
  //
  unsigned objectSize = TD->getTypeAllocSize (AI->getAllocatedType());
  
  //
  // If the allocation allocates an array, then the allocated size is a
  // multiplication.
  //
  if (AI->isArrayAllocation()) {
    unsigned num = cast<ConstantInt>(AI->getOperand(0))->getZExtValue();
    objectSize = objectSize * num;
  }
  unsigned adjustedSize = objectSize + sizeof(BBMetaData);
  unsigned char size = findP2Size (adjustedSize);

  //
  // Create necessary types.
  //
  Type *Int8Type = Type::getInt8Ty (AI->getContext());
  Type *Int32Type = Type::getInt32Ty (AI->getContext());

  //
  // Create a structure type.  The first element will be the global memory
  // object; the second will be an array of bytes that will pad the size out;
  // the third will be the metadata for this object.
  //
  Type *newType1 = ArrayType::get(Int8Type, (1<<size) - adjustedSize);
  Type *metadataType = TypeBuilder<BBMetaData, false>::get(AI->getContext());
  
  Type *ty = AI->getType()->getElementType();
  if (AI->isArrayAllocation()) {
    ty = ArrayType::get(Int8Type, objectSize);
  }
  
  StructType *newType = StructType::get(ty,
                              newType1,
                              metadataType,
                              NULL);
    
  //
  // Create the new alloca instruction and set its alignment.
  //
  AllocaInst * AI_new = new AllocaInst (newType,
                                             0,
                                        (1<<size),
                                        "baggy." + AI->getName(),
                                        AI);
  AI_new->setAlignment(1u<<size);

  //
  // Store the object size information into the medadata.
  //
  Value *Zero = ConstantInt::getSigned(Int32Type, 0);
  Value *Two = ConstantInt::getSigned(Int32Type, 2);
  Value *idx[3] = {Zero, Two, Zero};
  Value *V = GetElementPtrInst::Create(newType, AI_new, idx, Twine(""), AI);
  new StoreInst(ConstantInt::get(Int32Type, objectSize), V, AI);

  //
  // Create a GEP that accesses the first element of this new structure.
  //
  Value *idx1[2] = {Zero, Zero};
  Instruction *init = GetElementPtrInst::Create(newType, AI_new,
                                                idx1,
                                                Twine(""),
                                                AI);
  AI->replaceAllUsesWith(init);
  AI->removeFromParent(); 
  AI_new->setName(AI->getName());

  return;
}

//
// Method: adjustAllocasFor()
//
// Description:
//  Look for allocas used in calls to the specified function and adjust their
//  size and alignment for baggy bounds checking.
//
void
InsertBaggyBoundsChecks::adjustAllocasFor (Function * F) {
  //
  // If there is no such function, do nothing.
  //
  if (!F) return;

  //
  // Scan through all uses of the function and process any allocas used by it.
  //
  for (Value::use_iterator FU = F->use_begin(); FU != F->use_end(); ++FU) {
    if (CallInst * CI = dyn_cast<CallInst>(*FU)) {
      Value * Ptr = CI->getArgOperand(1)->stripPointerCasts();
      if (AllocaInst * AI = dyn_cast<AllocaInst>(Ptr)){
        adjustAlloca (AI);
      }
    }
  }

  return;
}

//
// Method: adjustArgv()
//
// Description:
//  This function adjusts the argv strings for baggy bounds checking.
//
void
InsertBaggyBoundsChecks::adjustArgv (Function * F) {
  //assert (F && "FIXME: Should not assume that argvregister is used!");
  if (!F) return;
  if (!F->use_empty()) {
    assert (isa<PointerType>(F->getReturnType()));
    assert (F->getNumUses() == 1);
    CallInst *CI = cast<CallInst>(*(F->use_begin()));
    Value *Argv = CI->getArgOperand(1);
    BasicBlock::iterator I = CI;
    I++;
    BitCastInst *BI = new BitCastInst(CI,
                                      Argv->getType(),
                                      "argv_temp",
                                      cast<Instruction>(I));
    std::vector<User *> Uses;
    Value::use_iterator UI = Argv->use_begin();
    for (; UI != Argv->use_end(); ++UI) {
      if (Instruction * Use = dyn_cast<Instruction>(*UI))
        if (CI != Use) {
          Uses.push_back (UI->getUser());
        }
    }

    while (Uses.size()) {
      User *Use = Uses.back();
      Uses.pop_back();
      Use->replaceUsesOfWith (Argv, BI);
    }
  }

  return;
}


//
// Function: mustCloneFunction()
//
// Description:
//  This function determines whether a function must be cloned when
//  dealing with byval argments for baggy bounds checking.
//
// Return value:
//  0 - The function does not need to be cloned for baggy bounds checking.
//  1 - The function need to be cloned for baggy bounds checking.
//
bool 
mustCloneFunction (Function * F) {
  if (F->isDeclaration()) return 0;

  if (F->hasName()) {
    std::string Name = F->getName();
    if ((Name.find ("__poolalloc") == 0) || (Name.find ("sc.") == 0)
        || (Name.find("baggy.") == 0) || (Name.find(".TEST") != Name.npos))
      return 0;
  }

  //
  // Loop over all the arguments of the function. If one argument has the byval
  // attribute and has use, then this function need to be cloned.
  //  
  for (Function::arg_iterator I = F->arg_begin(), E = F->arg_end();
       I != E; ++I) {
    if (I->hasByValAttr()) {
      if(I->use_empty()) {
        continue;
      }
      return 1;
    }
  }
  return 0;
}

// Baggy bounds specific version of the CloneFunctionInto() function
// found in the llvm file lib/Transforms/Utils/CloneFunction.cpp.
//
// An outline of the processing of the function follows, with 
// deltas from the original version noted:
//
// 1) Instead of setting the attributes of the new function, this 
//    version of cloneFunctionInto() simply verifies that:
//
//    a) Non byval or unused parameters have identical type 
//       and alignment.
//
//    b) used byval parameters must have different type and 
//       and alignment.  
//
//    c) The target type of the pointer type of each OldFunc
//       used byval parameter must match the type of the first 
//       argument of the structure type that is the target type 
//       matching NewFunc parameter.
//
//    c) VMap is setup as expected.  Specifically, each OldFunc
//       parameter must map to the matching parameter of the 
//       NewFunc.
//
// 2) Unlike the original version of the function, loop over all 
//    the used byval arguments in the old function.  For each such 
//    argument, 
// 
//    a) If we haven't created it already, create a basic block 
//       for the new function.
//
//    b) Find the associated byval argument of the new function,
//       We do this by simply looking it up in VMap[].
//
//    c) construct a GEP instruction that computes a pointer 
//       to the copy of the byval argument in the old function
//       that resides in the structure pointed to by the associated
//       argument in the new function, and stores this value in a 
//       SSA virtual register.
//    
//       In passing, insert the GEP instruction into the new 
//       basic block we inserted in the new function.
// 
// 3) Clone the basic blocks of the old function into the new, as 
//    per the original version of the function.  Like the original 
//    version, transform the old arguments into references to 
//    the associated VMap values
//
// 4) Assuming it was created, add an unconditional branch from the 
//    end of the basic block created in 2) above to the first 
//    basic block cloned from the old function.
//
//                                            JRM -- 2/5/12
//
void 
InsertBaggyBoundsChecks::cloneFunctionInto(Function *NewFunc, 
                  const Function *OldFunc,
                  ValueToValueMapTy &VMap,
                  bool ModuleLevelChanges,
                  SmallVectorImpl<ReturnInst*> &Returns,
                  const char *NameSuffix, 
                  ClonedCodeInfo *CodeInfo,
                  ValueMapTypeRemapper *TypeMapper) {
  assert(NameSuffix && "NameSuffix cannot be null!");

#ifndef NDEBUG
  for (Function::const_arg_iterator I = OldFunc->arg_begin(),
       E = OldFunc->arg_end(); I != E; ++I)
    assert(VMap.count(I) && "No mapping from source argument specified!");

  // scan the parameters of the old and new functions.  Unused and/or 
  // non-byval parameters should have the same type and alignment.  Used
  // byval parameters from the old function must be the first argument 
  // of structure type that is the type of the coresponding argument in 
  // the new function.
  {
    int i = 1;
    Function::const_arg_iterator Io = OldFunc->arg_begin();
    Function::const_arg_iterator Eo = OldFunc->arg_end();
    Function::arg_iterator In = NewFunc->arg_begin(), En = NewFunc->arg_end();

    while((Io != Eo) && (In != En)) {

      // verify that argument byval attributes match:
      assert((Io->hasByValAttr() == In->hasByValAttr()) &&
             "old/new function parameter byval attribute mismatch!");

      // The use_empty attributes of all the new function parameters 
      // must be set, since the function at present should not 
      // contain any code.
      assert((In->use_empty()) &&
             "new function parameter not use_empty?");

      // verify that the VMap maps the parameter of the old function to 
      // those of the new function in listed order.
      assert((VMap[Io] == In) && 
             "Unexpected mapping between params of old and new fcns.");

      if (!Io->hasByValAttr() || Io->use_empty()) {
        // verify that arguments without byval are of same type and 
        // alignment. 
        assert((Io->getType() == In->getType()) &&
               "non byval or use_empty type mismatch");
        assert((OldFunc->getParamAlignment(i) == NewFunc->getParamAlignment(i)) 
               && "non byval or use_empty alignment mismatch");

      } else /* Io->hasByValAttr() && !Io->use_empty() */ {

        assert((Io->getType() != In->getType()) &&
               "types of used byval arguments matches!");
        
        PointerType *oldTypePtr = dyn_cast<PointerType>(Io->getType());
        assert((oldTypePtr != NULL) && 
                "old used byval argument type not PointerType!");
        
        PointerType *newTypePtr = dyn_cast<PointerType>(In->getType());
        assert((newTypePtr != NULL) && 
                "new used byval argument type not PointerType!");

        StructType * newStructType =  
           dyn_cast<StructType>(newTypePtr->getElementType());

        assert((newStructType != NULL) && 
                "new used byval argument not ptr to StructType!");

        assert((newStructType->getNumElements() == 3) &&
                "new used byval argument struct type doesn't have 3 fields!");

        assert((newStructType->getElementType(0) == 
                 oldTypePtr->getElementType()) &&
               "new used byval arg struct first field != old byval tgt type.");

      }
      ++Io;
      ++In; 
      ++i;
    }
  }
#endif 

  // Loop over all the used byval arguments in the old function.  For 
  // each such argument, 
  // 
  // 0) If we haven't created it already, create a basic block for the 
  //    new function.
  //
  // 1) find the associated byval argument of the new function,
  //    we do this by simply looking it up in VMap[].
  //
  // 2) construct a GEP instruction that computes a pointer 
  //    to the copy of the byval argument in the old function
  //    that resides in the structure pointed to by the associated
  //    argument in the new function, and stores this value in a 
  //    SSA virtual register.
  //    
  //    In passing, insert the GEP instruction into the new 
  //    basic block we inserted in the new function.
  //
  // 3) Modify the VMap, so that it associates the byval parameter in 
  //    old function with the new SSA register.  Note that on entry 
  //    VMap associates the byval parameter in the old function with 
  //    the corresponding parameter in the new function.

  BasicBlock * header_blk = NULL;
  BasicBlock * first_cloned_blk = NULL;

  {
    Function::const_arg_iterator Io = OldFunc->arg_begin();
    Function::const_arg_iterator Eo = OldFunc->arg_end();
    Function::arg_iterator In = NewFunc->arg_begin(), En = NewFunc->arg_end();

    while((Io != Eo) && (In != En)) {

      if (Io->hasByValAttr() && !Io->use_empty()) {

        // construct a basic block for the new function if we haven't 
        // done so already.
        if ( header_blk == NULL ) {
          header_blk = BasicBlock::Create(NewFunc->getContext(), "header", 
                                          NewFunc);
          IRBuilder<> builder(header_blk);
        }

        Value *Idx[2];
        Idx[0] = ConstantInt::get(Type::getInt32Ty(NewFunc->getContext()), 0);
        Idx[1] = ConstantInt::get(Type::getInt32Ty(NewFunc->getContext()), 0);

        GetElementPtrInst * gep_inst = 
	  GetElementPtrInst::Create(Io->getType(), VMap[Io], Idx, 
                                 (VMap[Io])->getName() + ".cooked", header_blk);

        VMap[Io] = gep_inst;
      }
      ++Io;
      ++In; 
    }
  }

  // Loop over all of the basic blocks in the function, cloning them as
  // appropriate.  Note that we save BE this way in order to handle cloning of
  // recursive functions into themselves.
  //
  for (Function::const_iterator BI = OldFunc->begin(), BE = OldFunc->end();
       BI != BE; ++BI) {
    const BasicBlock &BB = *BI;

    // Create a new basic block and copy instructions into it!
    BasicBlock *CBB = CloneBasicBlock(&BB, VMap, NameSuffix, NewFunc, CodeInfo);

    // Make note of the first cloned basic block
    if ( first_cloned_blk == NULL ) {
      first_cloned_blk = CBB;
    }

    // Add basic block mapping.
    VMap[&BB] = CBB;

    // It is only legal to clone a function if a block address within that
    // function is never referenced outside of the function.  Given that, we
    // want to map block addresses from the old function to block addresses in
    // the clone. (This is different from the generic ValueMapper
    // implementation, which generates an invalid blockaddress when
    // cloning a function.)
    if (BB.hasAddressTaken()) {
      Constant *OldBBAddr = BlockAddress::get(const_cast<Function*>(OldFunc),
                                              const_cast<BasicBlock*>(&BB));
      VMap[OldBBAddr] = BlockAddress::get(NewFunc, CBB);                         
    }

    // Note return instructions for the caller.
    if (ReturnInst *RI = dyn_cast<ReturnInst>(CBB->getTerminator()))
      Returns.push_back(RI);
  }

  // Loop over all of the instructions in the function, fixing up operand
  // references as we go.  This uses VMap to do all the hard work.
  for (Function::iterator BB = cast<BasicBlock>(VMap[OldFunc->begin()]),
         BE = NewFunc->end(); BB != BE; ++BB)
    // Loop over all instructions, fixing each one as we find it...
    for (BasicBlock::iterator II = BB->begin(); II != BB->end(); ++II)
      RemapInstruction(II, VMap,
                       ModuleLevelChanges ? RF_None : RF_NoModuleLevelChanges,
                       TypeMapper);

  // Assuming it exists, add an unconditional branch from the end of the 
  // header block to the first block cloned over from the old function.
  if ( header_blk != NULL ) {
    assert((first_cloned_blk != NULL) && "First cloned block is NULL?!?");
    BranchInst::Create(first_cloned_blk, header_blk);
  }

   return;
}


//
// Function: cloneFunction()
//
// Description:
//  It clones a function when dealing with byval argments for baggy bounds 
//  checking. The cloned function pads and aligns the byvalue arguments in
//  the original function. After cloned, the original function calls this
//  cloned function, so that externel code and indirect calls use the original 
//  to call the cloned function.
//
Function *
InsertBaggyBoundsChecks::cloneFunction (Function * F) {

  Type *Int8Type = Type::getInt8Ty(F->getContext());
  Value *zero = ConstantInt::get(Type::getInt32Ty(F->getContext()), 0);
  Value *Idx[] = { zero, zero };
  
  // Get the function type.
  FunctionType *FTy = F->getFunctionType();

  // Vector to store all arguments' types.
  std::vector<Type*> TP;

  // Vector to store new types for byval arguments
  std::vector<Type*> NTP;

  // Vector to store the alignment size of new padded types.
  std::vector<unsigned int> LEN; 

  unsigned int i = 0;
  
  //
  // Loop over all the arguments of the function. If one argument has the byval
  // attribute, it will be padded and push into the vector; If it does not have
  // the byval attribute, it will be pushed into the vector without any change.
  // Then all the types in vector will be used to create the clone function.  
  //
  for (Function::arg_iterator I = F->arg_begin(), E = F->arg_end();
       I != E; ++I, ++i) {

    // Deal with the argument that without byval attribute
    if (!I->hasByValAttr()) {
      TP.push_back(FTy->getParamType(i));
      continue; 
    }

    // Deal with the argument that with byval attribute, but without use.
    if(I->use_empty()) {
      TP.push_back(FTy->getParamType(i));
      continue;
    }

    //
    // Find the greatest power-of-two size that is larger than the argument's
    // current size with metadata's size.
    //
    assert (isa<PointerType>(I->getType()));
    Type * ET = cast<PointerType>(I->getType())->getElementType();
    unsigned long AllocSize = TD->getTypeAllocSize(ET);
    unsigned long adjustedSize = AllocSize + sizeof(BBMetaData);
    unsigned int size = findP2Size (adjustedSize);

    // Get the alignment size and push it into the vector.
    unsigned int alignment = 1u << size;
    LEN.push_back(alignment);

    //
    // Create a structure type to pad the argument. The first element will 
    // be the argument's type; the second will be an array of bytes that 
    // will pad the size out; the third will be the metadata type.
    //
    Type *newType1 = ArrayType::get(Int8Type, alignment - adjustedSize);
    Type *metadataType = TypeBuilder<BBMetaData, false>::get(I->getContext());
    StructType *newType = StructType::get(ET, newType1, metadataType, NULL);

    // push the padded type into the vectors
    TP.push_back(newType->getPointerTo());
    NTP.push_back(newType);
  }//end for arguments handling

  // Create the new function. Return type is same as that of original
  // instruction.

  // Setup NewF with non-byval arguments as per F, and byval arguments
  // of type padded out to a power of two.
  FunctionType *NewFTy = FunctionType::get(FTy->getReturnType(), TP, false);
  Function *NewF = Function::Create(NewFTy,
                          GlobalValue::InternalLinkage,
                          F->getName() + ".TEST",
                          F->getParent());

  // iterate through the parameter list, and set the alignment of all
  // byval arguments.
  {
    std::vector<unsigned int>::iterator it = LEN.begin();
    i = 1;

    for (Function::arg_iterator Io = F->arg_begin(), Eo = F->arg_end(),
                                In = NewF->arg_begin(), En = NewF->arg_end();
         (Io != Eo) && (In != En); 
         ++Io, ++In, ++i) {
      // set argument names
      In->setName(Io->getName());

      // skip arguments without byval attribute or use.
      if (!Io->hasByValAttr() || Io->use_empty()) continue;

      {
        AttrBuilder AB0;
        AB0.addAttribute(Attribute::ByVal);
        In->addAttr(AttributeSet::get(NewF->getContext(), 0, AB0));

        AttrBuilder AB1;
        AB1.addAlignmentAttr (*it);
        In->addAttr(AttributeSet::get(In->getContext(), 0, AB1));

        ++it;
      }
    }
  }

  //
  // Create the arguments mapping between the original and the clonal function
  // to prepare for cloning the whole function.
  //
  ValueToValueMapTy VMap;
  Function::arg_iterator DestI = NewF->arg_begin();

  for (Function::arg_iterator I = F->arg_begin(), E = F->arg_end();
       I != E; ++I) {
    DestI->setName(I->getName());
    VMap[I] = DestI++;
  }

  // Perform the cloning.
  SmallVector<ReturnInst*, 8> Returns;
  cloneFunctionInto(NewF, F, VMap, false, Returns);


  //
  // Since externel code and indirect call use the original function
  // So we make the original function to call the clone function.
  // First delete the body of the function and creat a block in it.
  //
  F->dropAllReferences();
  BasicBlock * BB = BasicBlock::Create(F->getContext(), "clone", F, 0);

  //
  // Create an STL container with the arguments to call the clone function.
  std::vector<Value *> args;


  //
  // Look over all arguments. If the argument has byval attribute,
  // alloca its padded new type, store the argument's value into
  // it, and push the allocated type into the vector. If the 
  // argument has no such attribute, just push it into the vector.
  //

  // Iterator to get the new types stores in the vector.
  std::vector<Type*>::iterator iter = NTP.begin();

  for (Function::arg_iterator I = F->arg_begin(), E = F->arg_end();
       I != E; ++I) {
    if (!I->hasByValAttr()) {  // add "|| I->use_empty()" here?
      args.push_back(I);
       continue;
     }
      
    Type* newType = *iter++;
    AllocaInst *AINew = new AllocaInst(newType, "", BB);
    LoadInst *LINew = new LoadInst(I, "", BB);
    GetElementPtrInst *GEPNew = GetElementPtrInst::Create(newType, AINew,
                                                          Idx,
                                                          Twine(""),
                                                          BB);
    new StoreInst(LINew, GEPNew, BB);
    args.push_back(AINew);
  }
   
  //
  // Use the arguments in the vector to call the cloned function.
  //
  // If F is not void, return the return value of NewF.  Otherwise,
  // just return.
  //

  CallInst * call_to_new_func = CallInst::Create(NewF, args, "", BB);

  if ( F->getReturnType() == Type::getVoidTy(F->getContext())) {
    ReturnInst::Create(F->getContext(), BB);
  } else {
    ReturnInst::Create(F->getContext(), call_to_new_func, BB);
  }
  
  return NewF;
}

//
// Function: callClonedFunction()
//
// Description:
//  It changes all the uses for the original function with byval arguments
//  A direct call to the orignal function is replaced with a call to the 
//  cloned function.
//
void
InsertBaggyBoundsChecks::callClonedFunction (Function * F, Function * NewF) {

  Type *Int8Type = Type::getInt8Ty(F->getContext());

  // Vector to store the alignment size of new padded types.
  std::vector<unsigned int> LEN; 

  //
  //Change uses so that the direct calls to the original function become direct
  // calls to the cloned function.
  //
  for (Value::use_iterator FU = F->use_begin(), FE = F->use_end();
       FU != FE; ++FU) {
    if (CallInst * CI = dyn_cast<CallInst>(*FU)) {
      if (CI->getCalledFunction() == F) {
        Function *Caller = CI->getParent()->getParent();
        Instruction *InsertPoint;
        BasicBlock::iterator insrt = Caller->front().begin();
        for (; isa<AllocaInst>(InsertPoint = insrt); ++insrt) {;}
               
        //
        // Create an STL container with the arguments to call the cloned
        // function.
        //
        std::vector<Value *> args;
        unsigned int i = 0;

        // Look over all arguments. If the argument has byval attribute,
        // alloca its padded new type, store the argument's value into it.
        // and push the allocated type into the vector. If the argument
        // has no such attribute, just push it into the vector.
        //
        for (Function::arg_iterator I = F->arg_begin(), E = F->arg_end();
             I != E; ++I, ++i) {
          if (!I->hasByValAttr() || I->use_empty()) {
            args.push_back(I);
            continue;
          }
          assert (isa<PointerType>(I->getType()));
          Type * ET = cast<PointerType>(I->getType())->getElementType();
          unsigned long AllocSize = TD->getTypeAllocSize(ET);
          unsigned long adjustedSize = AllocSize + sizeof(BBMetaData);
          unsigned int size = findP2Size (adjustedSize);

          // Get the alignment size and push it into the vector.
          unsigned int alignment = 1u << size;
          LEN.push_back(alignment);

          // Create a structure type to pad the argument. The first element
          // will be the argument's type; the second will be an array of 
          // bytes that will pad the size out; the third will be the metadata 
          // type.
          //
          Type *newType1 = ArrayType::get(Int8Type, alignment - adjustedSize);
          Type *meteTP = TypeBuilder<BBMetaData, false>::get(I->getContext());
          StructType *newType = StructType::get(ET,
                                                newType1,
                                                meteTP,
                                                NULL);

              
          Value *zero = ConstantInt::get(Type::getInt32Ty(F->getContext()),0);
          Value *Idx[] = { zero, zero }; 
          AllocaInst *AINew = new AllocaInst(newType, 0, alignment, "", InsertPoint);
          LoadInst *LINew = new LoadInst(CI->getOperand(i), "", CI);
          GetElementPtrInst *GEPNew = GetElementPtrInst::Create(newType, AINew,
                                                                Idx,
                                                                Twine(""),
                                                                CI);
          new StoreInst(LINew, GEPNew, CI);
          args.push_back(AINew);
         
        }

        // replace the original function with the cloned one.
        CallInst *CallI = CallInst::Create(NewF, args,"", CI);

        // Add alignment attribute when calling the cloned function.
        std::vector<unsigned int>::iterator iiter = LEN.begin();
        i = 0;

        for (Function::arg_iterator I = F->arg_begin(), E = F->arg_end();
             I != E; ++I, ++i) {
          if (I->hasByValAttr() && !I->use_empty()) {

#if 0
            // Remove the old alignment attribute
            CallI->removeAttribute(i + 1, Attributes(Attribute::Alignment));
#endif

            // Add the new alignment attribute
            AttrBuilder AB;
            AB.addAlignmentAttr (*iiter++);
            AttributeSet AS = CallI->getAttributes();
            AS = AS.addAttributes(F->getContext(), i + 1,
                                  AttributeSet::get(F->getContext(), 0, AB));
            CallI->setAttributes(AS);
          }
        }
        CallI->setCallingConv(CI->getCallingConv());
        CI->replaceAllUsesWith(CallI);
        CI->eraseFromParent();
      }
    }
  } // end for use changes
  return;
}

//
// Method: runOnModule()
//
// Description:
//  Entry point for this LLVM pass.
//
// Return value:
//  true  - The module was modified.
//  false - The module was not modified.
//
bool
InsertBaggyBoundsChecks::runOnModule (Module & M) {
  // Get prerequisite analysis results
  TD = &M.getDataLayout();
  //
  // Align and pad global variables.
  //
  std::vector<GlobalVariable *> varsToTransform;
  Module::global_iterator GI = M.global_begin(), GE = M.global_end();
  for (; GI != GE; ++GI) {
    if (GlobalVariable * GV = mustAdjustGlobalValue (GI))
      varsToTransform.push_back (GV);
  }

  for (unsigned index = 0; index < varsToTransform.size(); ++index) {
    adjustGlobalValue (varsToTransform[index]);
  }
  varsToTransform.clear();

  //
  // Align and pad stack allocations (allocas) that are registered with the
  // run-time.  We don't do all stack objects because we don't need to adjust
  // the size of an object that is never returned in a table lookup.
  //
  adjustAllocasFor (M.getFunction ("pool_register_stack"));
  adjustAllocasFor (M.getFunction ("pool_register_stack_debug"));


  // changes for register argv
  adjustArgv(M.getFunction ("poolargvregister"));
  
  // Deal with byval argument.
  for (Module::iterator I = M.begin(), E = M.end(); I != E; ++ I) {
    Function *F = I;
    if (!mustCloneFunction(F)) continue;
    
#if 0
    Function *NewF = cloneFunction(F);
    callClonedFunction(F, NewF);
#else
    cloneFunction(F);
#endif
  }
  return true;
}

}

