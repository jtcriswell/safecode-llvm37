// Prototype creator for SoftBoundPass

#ifndef INITIALIZE_SOFTBOUND_H
#define INITIALIZE_SOFTBOUND_H

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/DataLayout.h"


using namespace llvm;

class InitializeSoftBound: public ModulePass {

 private:

 public:
  bool runOnModule(Module &);
  static char ID;

  void constructCheckHandlers(Module &);
  void constructMetadataHandlers(Module &);
  void constructShadowStackHandlers(Module &);
  void constructAuxillaryFunctionHandlers(Module &);
  InitializeSoftBound(): ModulePass(ID){        
  }
  
  const char* getPassName() const { return "InitializeSoftBound";}
};

#endif
