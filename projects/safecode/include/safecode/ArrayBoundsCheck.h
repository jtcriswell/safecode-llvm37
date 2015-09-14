//===- ArrayBoundsCheck.h ---------------------------------------*- C++ -*----//
// 
//                          The SAFECode Compiler 
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This pass implements a static array bounds checking analysis pass.
//
//===----------------------------------------------------------------------===//

#ifndef ARRAY_BOUNDS_CHECK_H_
#define ARRAY_BOUNDS_CHECK_H_

#include "safecode/AllocatorInfo.h"

#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/Pass.h"

namespace llvm {
/// This class defines the interface of array bounds checking.
class ArrayBoundsCheckGroup {
public:
  static char ID;
  /// Determine whether a particular GEP instruction is always safe of not.
  virtual bool isGEPSafe(GetElementPtrInst * GEP) { return false; }
  virtual ~ArrayBoundsCheckGroup() = 0;
};

/// This is the dummy version of array bounds checking. It simply assumes that
/// every GEP instruction is unsafe.
class ArrayBoundsCheckDummy : public ArrayBoundsCheckGroup,
                              public ImmutablePass {
public:
  static char ID;
  ArrayBoundsCheckDummy() : ImmutablePass(ID) {}
  /// When chaining analyses, changing the pointer to the correct pass
  virtual void *getAdjustedAnalysisPointer(const void * ID) {
      if (ID == (&ArrayBoundsCheckGroup::ID))
        return (ArrayBoundsCheckGroup*)this;
      return this;
  }
};


/// ArrayBoundsCheckLocal - It tries to prove a GEP is safe only based on local
/// information, that is, the size of global variables and the size of objects
/// being allocated inside a function.
class ArrayBoundsCheckLocal : public FunctionPass,
                              public InstVisitor<ArrayBoundsCheckLocal> {
public:
  static char ID;
  ArrayBoundsCheckLocal() : FunctionPass(ID) {}
  virtual bool isGEPSafe(GetElementPtrInst * GEP);
  virtual void getAnalysisUsage(AnalysisUsage & AU) const {
    AU.addRequired<AllocatorInfoPass>();
    AU.addRequired<ScalarEvolution>();
    AU.setPreservesAll();  
  }
  virtual bool runOnFunction(Function & F);

  virtual void releaseMemory() {
    SafeGEPs.clear();
  }

  /// When chaining analyses, changing the pointer to the correct pass
  virtual void *getAdjustedAnalysisPointer(const void * ID) {
      if (ID == (&ArrayBoundsCheckGroup::ID))
        return (ArrayBoundsCheckGroup*)this;
      return this;
  }

  void visitGetElementPtrInst (GetElementPtrInst & GEP);

private:
  // Required passes
  const DataLayout * TD;
  ScalarEvolution * SE;

  // Container holding safe GEPs
  std::set<GetElementPtrInst *> SafeGEPs;
};

}

#endif
