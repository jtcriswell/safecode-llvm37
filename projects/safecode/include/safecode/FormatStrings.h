//===------------ FormatStrings.h - Secure format string function calls ---===//
// 
//                          The SAFECode Compiler 
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This pass finds calls to format string functions and replaces them with
// calls to secured runtime wrappers.
//
//===----------------------------------------------------------------------===//

#ifndef FORMAT_STRINGS_H
#define FORMAT_STRINGS_H

#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

#include "safecode/SAFECode.h"

#include <map>
#include <set>
#include <utility>

#include <stdint.h>

using std::map;
using std::set;
using std::pair;

namespace llvm
{
  class FormatStringTransform : public ModulePass
  {
  private:
    // The fsparameter function.
    Value *FSParameter;
    // The fscallinfo function.
    Value *FSCallInfo;
    // The type for the pointer_info structure.
    Type *PointerInfoType;
    // A map from a function to the instruction where the call_info structure
    // allocated for that function.
    map<Function *, Instruction *> CallInfoStructures;
    // A map from a function to the instruction where the pointer_info array
    // is allocated for that function.
    map<Function *, Instruction *> PointerInfoStructures;
    // This represents a pointer value and the corresponding call where it is
    // passed as a parameter.
    typedef pair<Instruction *, Value *> PointerArgument;
    // A map from (call, pointer value) pairs to the corresponding fsparameter
    // calls which wrap the pointer value in a pointer_info structure.
    map<PointerArgument, Value *> FSParameterCalls;
    // A map from call to the number of pointer_info structures which are used
    // by the transformed version of that call.
    map<Instruction *, unsigned> PointerInfoArrayUsage;
    // A map from function to the size of the pointer_info array for that
    // function.
    map<Function *, unsigned> PointerInfoAllocSizes;
    // A map from function to the size of the call_info whitelist for that
    // function.
    map<Function *, unsigned> CallInfoWhitelistSizes;

    // Builds the pointer_info structure type.
    Type *makePointerInfoType(LLVMContext &ctx) const;
    // Builds a call_info structure type with a whitelist of size argc.
    Type *makeCallInfoType(LLVMContext &ctx, unsigned argc) const;
    // Builds a type consistent with the transformed format string function
    // type.
    FunctionType *xfrmFType(FunctionType *F, LLVMContext &c) const;
    // Scans the module and makes the array allocations that the pass added all
    // the proper size.
    void fillArraySizes(Module &M);
    // Transform all calls of the given function.
    bool transform(
      Module &M, const char *name, unsigned argc, const char *to, Statistic &st
    );
    // Adds intrinsic declarations to the module.
    void addFormatStringIntrinsics(Module &M);
    // Adds a call to fsparameter for the given (instruction, pointer value)
    // pair.
    Value *wrapPointerArgument(PointerArgument arg);
    // Adds a call to fscallinfo for the given function call.
    Value *addCallInfo(Instruction *i, uint32_t vargc, const set<Value*> &ptrs);
    // Creates a call to the transformed function out of a previous call
    // instruction.
    CallInst *buildSecuredCall(Value *newFunc, CallSite &oldCall);

  public:
    static char ID;
    FormatStringTransform() : ModulePass(ID) {}
    virtual bool runOnModule(Module &M);
  };
}

#endif
