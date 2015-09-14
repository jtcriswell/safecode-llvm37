//===- FormatStrings.cpp - Secure calls to printf/scanf style functions ---===//
// 
//                            The SAFECode Compiler
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file implements a pass to insert calls to runtime wrapper functions for
// printf(), scanf(), and related format string functions.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "formatstrings"

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"

#include "safecode/FormatStrings.h"
#include "safecode/Utility.h"
#include "safecode/VectorListHelper.h"

#include <set>
#include <map>
#include <vector>
#include <algorithm>

using std::map;
using std::max;
using std::set;
using std::vector;

namespace llvm
{

static RegisterPass<FormatStringTransform>
R("formatstrings", "Secure calls to format string functions");

#define ADD_STATISTIC_FOR(func) \
  STATISTIC(stat_ ## func, "Number of calls to " #func "() that were secured")

ADD_STATISTIC_FOR(printf);
ADD_STATISTIC_FOR(fprintf);
ADD_STATISTIC_FOR(sprintf);
ADD_STATISTIC_FOR(snprintf);
ADD_STATISTIC_FOR(err);
ADD_STATISTIC_FOR(errx);
ADD_STATISTIC_FOR(warn);
ADD_STATISTIC_FOR(warnx);
ADD_STATISTIC_FOR(syslog);
ADD_STATISTIC_FOR(scanf);
ADD_STATISTIC_FOR(fscanf);
ADD_STATISTIC_FOR(sscanf);
ADD_STATISTIC_FOR(__printf_chk);
ADD_STATISTIC_FOR(__fprintf_chk);
ADD_STATISTIC_FOR(__sprintf_chk);
ADD_STATISTIC_FOR(__snprintf_chk);
ADD_STATISTIC_FOR(__isoc99_scanf);
ADD_STATISTIC_FOR(__isoc99_fscanf);
ADD_STATISTIC_FOR(__isoc99_sscanf);

char FormatStringTransform::ID = 0;


//
// Constructs a FunctionType which is consistent with the type of a tranformed
// format string function.
//
// Inputs:
//   argc - the expected number of fixed arguments the function type takes
//   F    - the original function type
//
FunctionType *
FormatStringTransform::xfrmFType(FunctionType *F, LLVMContext &ctx) const
{
  Type *int8ptr = Type::getInt8PtrTy(ctx);
  vector<Type *> NewParamTypes;
  //
  // The initial argument is a pointer to the call_info structure.
  //
  NewParamTypes.push_back(int8ptr);

  //
  // Append all other arguments.
  //
  FunctionType::param_iterator i = F->param_begin(), end = F->param_end();
  for (; i != end; ++i)
    NewParamTypes.push_back(isa<PointerType>(*i) ? int8ptr : *i);

  return FunctionType::get(F->getReturnType(), NewParamTypes, true);
}

bool
FormatStringTransform::runOnModule(Module &M)
{
  //
  // Get the type of the pointer_info structure.
  //
  PointerInfoType = makePointerInfoType(M.getContext());

  FSCallInfo = FSParameter = 0;

  bool changed = false;

  struct FormatStringFuncEntry
  {
    const char *name;
    unsigned fargc;
    Statistic *stat;
    const char *replacement;
  } Entries[] =
  {
    { "printf",          1, &stat_printf,          "pool_printf"              },
    { "fprintf",         2, &stat_fprintf,         "pool_fprintf"             },
    { "sprintf",         2, &stat_sprintf,         "pool_sprintf"             },
    { "snprintf",        3, &stat_snprintf,        "pool_snprintf"            },
    { "err",             2, &stat_err,             "pool_err"                 },
    { "errx",            2, &stat_errx,            "pool_errx"                },
    { "warn",            1, &stat_warn,            "pool_warn"                },
    { "warnx",           1, &stat_warnx,           "pool_warnx"               },
    { "syslog",          2, &stat_syslog,          "pool_syslog"              },
    { "scanf",           1, &stat_scanf,           "pool_scanf"               },
    { "fscanf",          2, &stat_fscanf,          "pool_fscanf"              },
    { "sscanf",          2, &stat_sscanf,          "pool_sscanf"              },
    //
    // The __printf_chk() family is like printf(), but it attempts to make sure
    // the stack isn't accessed improperly. The SAFECode runtime also does this
    // (and more) so we can transform calls to this function.
    //
    { "__printf_chk",    2, &stat___printf_chk,    "pool___printf_chk"        },
    { "__fprintf_chk",   3, &stat___fprintf_chk,   "pool___fprintf_chk"       },
    { "__sprintf_chk",   4, &stat___sprintf_chk,   "pool___sprintf_chk"       },
    { "__snprintf_chk",  5, &stat___snprintf_chk,  "pool___snprintf_chk"      },
    //
    // The __isoc99_scanf() family is found in glibc and is like scanf() without
    // GNU extensions, which is the same functionality as the SAFECode version.
    //
    { "__isoc99_scanf",  1, &stat___isoc99_scanf,  "pool_scanf"               },
    { "__isoc99_fscanf", 2, &stat___isoc99_fscanf, "pool_fscanf"              },
    { "__isoc99_sscanf", 2, &stat___isoc99_sscanf, "pool_sscanf"              }
  };

  for (size_t i = 0; i < sizeof(Entries) / sizeof(FormatStringFuncEntry); i++)
  {
    FormatStringFuncEntry &e = Entries[i];
    changed |= transform(M, e.name, e.fargc, e.replacement, *e.stat);
  }

  //
  // The transformations use placehold arrays of size 0. This call changes
  // those arrays to be allocated to the proper size.
  //
  if (changed)
    fillArraySizes(M);

  return changed;
}

//
// Adds declarations of the format string function intrinsics
// sc.fsparameter and sc.callinfo into the given module.
//
// Sets the value of FSParameter and FSCallInfo to the relevant intrinsics.
//
void
FormatStringTransform::addFormatStringIntrinsics(Module &M)
{
  Type *int8    = Type::getInt8Ty(M.getContext());
  Type *int32   = Type::getInt32Ty(M.getContext());
  Type *int8ptr = Type::getInt8PtrTy(M.getContext());
  //
  // Build parameter lists.
  //
  vector<Type *> FSPArgs =
    args<Type *>::list(int8ptr, int8ptr, int8ptr, int8);
  vector<Type *> FSCIArgs = args<Type *>::list(int8ptr, int32);
  //
  // Build the function types.
  //
  FunctionType *FSParameterType = FunctionType::get(int8ptr, FSPArgs, false);
  FunctionType *FSCallInfoType  = FunctionType::get(int8ptr, FSCIArgs, true);
  //
  // Check if the functions are already declared.
  //
  Function *FSParameterInModule = M.getFunction("__sc_fsparameter");
  Function *FSCallInfoInModule  = M.getFunction("__sc_fscallinfo");
  if (FSParameterInModule != 0)
    assert((FSParameterInModule->getFunctionType() == FSParameterType ||
      FSParameterInModule->hasLocalLinkage()) &&
      "Intrinsic declared with wrong type!");
  if (FSCallInfoInModule != 0)
    assert((FSCallInfoInModule->getFunctionType() == FSCallInfoType ||
      FSParameterInModule->hasLocalLinkage()) &&
      "Intrinsic declared with wrong type!");
  //
  // Add the function declarations to the module and globally for this pass.
  //
  FSParameter = M.getOrInsertFunction("__sc_fsparameter", FSParameterType);
  FSCallInfo  = M.getOrInsertFunction("__sc_fscallinfo",  FSCallInfoType);
}

//
// Transform all calls of a given function into their secured analogue.
//
// A format string function of the form
//
//   int xprintf(arg1, arg2, ...);
//
// will be transformed into a call of the function of the form
//
//   int pool_xprintf(call_info *, arg1, arg2, ...);
//
// with the call_info * structure containing information about the vararg
// arguments passed into the call. All pointer arguments to the call will
// be wrapped around a pointer_info structure. The space for the call_info
// and pointer_info structures is allocated on the stack.
//
// Inputs:
//  M           - a reference to the current Module
//  name        - the name of the function to transform
//  argc        - the number of (fixed) arguments to the function
//  replacement - the name of the resulting function
//  stat        - a statistic pertaining to the number of transfomations
//                that have been performed
//
// Returns:
//  This function returns true if the module was modified, false otherwise.
//
bool
FormatStringTransform::transform(Module &M,
                                 const char *name,
                                 unsigned argc,
                                 const char *replacement,
                                 Statistic &stat)
{
  Function *f = M.getFunction(name);
  if (f == 0)
    return false;

  //
  // Ensure the function is of the expected type. If not, skip over it.
  //
  FunctionType *fType = f->getFunctionType();
  if (!fType->isVarArg() || fType->getNumParams() != argc)
    return false;

  vector<CallSite> Calls;

  //
  // Locate all the instructions which call the named function.
  //
  for (Function::use_iterator i = f->use_begin(); i != f->use_end(); ++i)
  {
    CallSite C(*i);
    if (!C || C.getCalledFunction() != f)
      continue;
    Calls.push_back(C);
  }

  if (Calls.empty())
    return false;

  FunctionType *rType = xfrmFType(fType, f->getContext());
#ifndef NDEBUG
  Function *found = M.getFunction(replacement);
  assert((found == 0 || found->getFunctionType() == rType
    || found->hasLocalLinkage()) && 
    "Replacement function already declared in module with incorrect type");
#endif

  Value *replacementFunc = M.getOrInsertFunction(replacement, rType);

  //
  // If we get this far, make sure the intrinsics have been declared so we can
  // call them.
  //
  if (FSParameter == 0 || FSCallInfo == 0)
    addFormatStringIntrinsics(M);

  //
  // Iterate over the found call sites and replace them with transformed
  // calls.
  //
  vector<CallSite>::iterator i = Calls.begin(), end = Calls.end();
  for (; i != end; ++i)
  {
    Instruction *OldCall = i->getInstruction();
    CallInst *NewCall = buildSecuredCall(replacementFunc, *i);
    NewCall->insertBefore(OldCall);
    OldCall->replaceAllUsesWith(NewCall);
    //
    // When transforming an invoke instruction, create a branch to the normal
    // label, since the transformed call doesn't throw exceptions.
    //
    if (isa<InvokeInst>(OldCall))
    {
      InvokeInst *Invoke = cast<InvokeInst>(OldCall);
      removeInvokeUnwindPHIs(Invoke);
      BranchInst *Br = BranchInst::Create(Invoke->getNormalDest());
      Invoke->eraseFromParent();
      Br->insertAfter(NewCall);
    }
    else
      OldCall->eraseFromParent();
    ++stat;
  }

  return true;
}

//
// Goes over all the arrays that were allocated as helpers to the intrinsics
// and makes them the proper size.
//
void
FormatStringTransform::fillArraySizes(Module &M)
{
  LLVMContext &C = M.getContext();
  IRBuilder<> builder(C);
  Type *int8ptr = Type::getInt8PtrTy(C);
  Type *int32 = Type::getInt32Ty(C);

  //
  // Make the CallInfo structure allocations the right size.
  //
  for (map<Function *, unsigned>::iterator i = CallInfoWhitelistSizes.begin();
       i != CallInfoWhitelistSizes.end();
       ++i)
  {
    Function *f = i->first;
    unsigned count = i->second;
    Type *CIType = makeCallInfoType(C, count);
    AllocaInst *newAlloc = builder.CreateAlloca(CIType);
    Instruction *newCast = cast<Instruction>(
      builder.CreateBitCast(newAlloc, int8ptr)
    );

    //
    // The CallInfo structure is cast to i8* before being passed into any
    // function calls.
    //
    // The placeholder cast is located in CallInfoStructures.
    //
    Instruction *oldCast  = CallInfoStructures[f];
    Instruction *oldAlloc = cast<Instruction>(oldCast->getOperand(0));

    newAlloc->insertBefore(oldAlloc);
    newCast->insertAfter(newAlloc);
    oldCast->replaceAllUsesWith(newCast);

    oldCast->eraseFromParent();
    oldAlloc->eraseFromParent();
  }

  //
  // Make the PointerInfo structure array allocations the right size.
  //
  for (map<Function *, unsigned>::iterator i = PointerInfoAllocSizes.begin();
       i != PointerInfoAllocSizes.end();
       ++i)
  {
    Function *f = i->first;
    Instruction *oldAlloc = PointerInfoStructures[f];
    Value *sz = ConstantInt::get(int32, i->second);
    AllocaInst *newAlloc = builder.CreateAlloca(PointerInfoType, sz);
    newAlloc->insertBefore(oldAlloc);
    oldAlloc->replaceAllUsesWith(newAlloc);
    oldAlloc->eraseFromParent();
  }

}

//
// Builds a call to fsparameter which registers the given parameter as a
// pointer.
//
// Inputs:
//   arg - the pointer value / instruction pair to register
//
// The function inserts the call to fsparameter before the associated
// instruction.
// Since only one call is needed to fsparameter per pointer / instruction pair,
// the function keeps track of redundant calls to itself and returns the same
// Value each time.
//
// Output:
//   The function returns a Value which is the result of wrapping the pointer
//   parameter using fsparameter. The type is i8 *.
//
Value *
FormatStringTransform::wrapPointerArgument(PointerArgument arg)
{
  //
  // Determine if the value has already been registered for this instruction.
  // If so, return the registered value.
  //
  if (FSParameterCalls.count(arg))
    return FSParameterCalls[arg];

  Instruction *i = arg.first;
  Value *ptr     = arg.second;

  Function *f = i->getParent()->getParent();
  LLVMContext &ctx = f->getContext();
  IRBuilder<> builder(ctx);

  //
  // Otherwise use the next free PointerInfo structure.
  //
  // First determine if the array of PointerInfo structures has already
  // been allocated on the function's stack. If not, do so.
  //
  if (!PointerInfoAllocSizes.count(f))
  {
    Value *zero = ConstantInt::get(Type::getInt32Ty(ctx), 0);
    AllocaInst *allocation = builder.CreateAlloca(PointerInfoType, zero);
    //
    // Allocate the array at the entry point of the function.
    //
    BasicBlock::InstListType &instList =
      i->getParent()->getParent()->getEntryBlock().getInstList();
    instList.insert(instList.begin(), allocation);
    PointerInfoStructures[f] = allocation;
    PointerInfoAllocSizes[f] = 0;
  }

  //
  // This is the index of the array that will be used.
  //
  const unsigned nextStructure = PointerInfoArrayUsage[i]++;
  Type *int8    = Type::getInt8Ty(ctx);
  Type *int8ptr = Type::getInt8PtrTy(ctx);

  //
  // Update the per-function count of the number of pointer_info structures
  // that are used. This used is for allocating the correct size on the stack in
  // fillArraySizes().
  //
  PointerInfoAllocSizes[f] = max(PointerInfoAllocSizes[f], 1 + nextStructure);

  //
  // Index into the next free position in the PointerInfo array.
  //
  Value *array = PointerInfoStructures[f];
  Instruction *gep = cast<Instruction>(
    builder.CreateConstGEP1_32(array, nextStructure)
  );
  Instruction *bitcast = cast<Instruction>(
    builder.CreateBitCast(gep, int8ptr)
  );
  gep->insertBefore(i);
  bitcast->insertBefore(i);

  //
  // Create the fsparameter call and insert it before the given instruction.
  // Also store it for later use if necessary (if the same parameter is
  // registered for the same instruction).
  //
  Value *castedParameter = ptr;
  if (castedParameter->getType() != int8ptr)
  {
    castedParameter = builder.CreateBitCast(ptr, int8ptr);
    if (isa<Instruction>(castedParameter))
      cast<Instruction>(castedParameter)->insertBefore(i);
  }
  vector<Value *> FSArgs(4);
  FSArgs[0] = ConstantPointerNull::get(cast<PointerType>(int8ptr));
  FSArgs[1] = castedParameter;
  FSArgs[2] = bitcast;
  FSArgs[3] = ConstantInt::get(int8, 0);
  CallInst *FSCall = builder.CreateCall(FSParameter, FSArgs);
  FSCall->insertBefore(i);
  FSParameterCalls[arg] = FSCall;

  return FSCall;
}

//
// Builds a call to callinfo which registers information about the given
// call to a format string function.
//
// Inputs:
//  i       - the instruction associated with the call to the format string
//            function
//  vargc   - the number of variable arguments in the call to register
//  PVArguments - every variable pointer argument to the call of the format
//                string function that should be whitelisted
//                 
// This function returns a Value suitable as the first parameter to a
// transformed format string function like pool_printf.
//
Value *
FormatStringTransform::addCallInfo(Instruction *i,
                                   uint32_t vargc,
                                   const set<Value *> &PVArguments)
{
  LLVMContext &ctx = i->getContext();
  IRBuilder<> builder(ctx);
  const unsigned pargc = PVArguments.size();
  Type *int8ptr  = Type::getInt8PtrTy(ctx);

  Function *f = i->getParent()->getParent();
  //
  // Allocate the CallInfo structure at the entry point of the function if
  // necessary. The allocated structure will be a placeholder.
  //
  if (!CallInfoStructures.count(f))
  {
    Value *zero = ConstantInt::get(Type::getInt32Ty(ctx), 0);
    Type *CInfoType = makeCallInfoType(ctx, 0);
    AllocaInst *allocation = builder.CreateAlloca(CInfoType, zero);

    //
    // Place this allocation at the function entry.
    //
    BasicBlock::InstListType &instList =
      i->getParent()->getParent()->getEntryBlock().getInstList();
    instList.insert(instList.begin(), allocation);

    //
    // Bitcast it into (i8 *) because that is the type for which it is used as
    // a parameter to sc.fscallinfo.
    //
    Instruction *bitcast = cast<Instruction>(
      builder.CreateBitCast(allocation, int8ptr)
    );
    bitcast->insertAfter(allocation);

    CallInfoStructures[f]  = bitcast;
    CallInfoWhitelistSizes[f] = 0;
  }

  //
  // Update the per-function count of the max size of the whitelist in the
  // call_info structure. Later fillArraySizes() will allocate a structure
  // with enough space to hold a whitelist for each registered
  // call in the function.
  //
  CallInfoWhitelistSizes[f] = max(CallInfoWhitelistSizes[f], pargc);

  Value *cInfo = CallInfoStructures[f];
  Value *null = ConstantPointerNull::get(cast<PointerType>(int8ptr));
  vector<Value *> Params;

  //
  // Build the parameters to the callinfo call.
  //
  Params.push_back(cInfo);
  Params.push_back(
    ConstantInt::get(Type::getInt32Ty(ctx), vargc)
  );
  Params.insert(Params.end(), PVArguments.begin(), PVArguments.end());
  //
  // Append NULL to terminate the variable argument list and finally build the
  // completed call instruction.
  //
  Params.push_back(null);
  CallInst *c = builder.CreateCall(FSCallInfo, Params);
  c->insertBefore(i);

  //
  // Add to the new call instruction any debugging metadata that the old call
  // had. This will be passed over to the call to the transformed function.
  //
  if (MDNode *DebugMetaData = i->getMetadata("dbg"))
    c->setMetadata("dbg", DebugMetaData);

  return c;
}

//
// Builds a call instruction to newFunc out of the existing call instruction.
// The new call uses the same arguments as the old call, except that pointer
// arguments to the old call are first wrapped using sc.fsparameter before
// being passed into the new call.
//
// Inputs:
//   newFunc - the function to which a call will be built
//   oldCall - a reference to the CallSite to transform
//
// Returns:
//   This function returns a CallInst that replaces the old instruction.
//
CallInst *
FormatStringTransform::buildSecuredCall(Value *newFunc, CallSite &oldCall)
{
  set<Value *> pointerVArgs;
  const unsigned fargc = \
    oldCall.getCalledFunction()->getFunctionType()->getNumParams();
  const unsigned argc  = oldCall.arg_size();
  const unsigned vargc = argc - fargc;
  vector<Value *> NewArgs(1 + argc);
  Instruction *cInst = oldCall.getInstruction();
  //
  // Build the parameters to the new call, creating wrappers with
  // sc.fsparameter when necessary.
  //
  for (unsigned i = 0; i < argc; ++i)
  {
    Value *arg = oldCall.getArgument(i);
    if (!isa<PointerType>(arg->getType()))
      NewArgs[i + 1] = arg;
    else
    {
      Value *wrapped = wrapPointerArgument(PointerArgument(cInst, arg));
      NewArgs[i + 1] = wrapped;
      //
      // If this is a variable pointer argument, it should be registered with
      // the callinfo intrinsic.
      //
      if (i >= fargc)
        pointerVArgs.insert(wrapped);
    }
  }
  //
  // Build the CallInfo structure for the new call.
  //
  NewArgs[0] = addCallInfo(cInst, vargc, pointerVArgs);
  //
  // Construct the new call instruction.
  //
  return CallInst::Create(newFunc, NewArgs);
}

//
// Creates the type of the PointerInfo structure.
// This is defined in FormatStringRuntime.h as
//
//   typedef struct
//   {
//      void *ptr;
//      void *pool;
//      void *bounds[2];
//      uint8_t flags;
//   } pointer_info;
//
// The fields are used as follows:
//  - ptr holds the pointer parameter that was passed.
//  - pool holds the pool that ptr belongs to.
//  - bounds are intended to be filled at runtime with the memory object
//    boundaries of ptr.
//  - flags holds various information about the pointer, regarding completeness
//    etc.
//
Type *
FormatStringTransform::makePointerInfoType(LLVMContext &ctx) const
{
  Type *int8         = Type::getInt8Ty(ctx);
  Type *int8ptr      = Type::getInt8PtrTy(ctx);
  Type *int8ptr_arr2 = ArrayType::get(int8ptr, 2);
  vector<Type *> PointerInfoFields =
    args<Type *>::list(int8ptr, int8ptr, int8ptr_arr2, int8);
  return StructType::get(ctx, PointerInfoFields);
}

//
// Creates the type of the CallInfo structure, with a varying whitelist field
// size.
//
// This type is defined in FormatStringRuntime.h as
//
//   typedef struct
//   {
//      uint32_t vargc;
//      uint32_t tag;
//      uint32_t line_no;
//      const char *source_info;
//      void  *whitelist[1];
//   } call_info;
//
// The fields are used as follows:
//  - vargc is the total number of variable arguments passed in the call.
//  - tag, line_no, source_info hold debug-related information.
//  - whitelist is a variable-sized array of pointers, with the last element
//    in the array being NULL. These pointers are the only values which the
//    wrapper callee will treat as vararg pointer arguments.
//
Type *
FormatStringTransform::makeCallInfoType(LLVMContext &ctx, unsigned argc) const
{
  Type *int32       = Type::getInt32Ty(ctx);
  Type *int8ptr     = Type::getInt8PtrTy(ctx);
  Type *int8ptr_arr = ArrayType::get(int8ptr, 1 + argc);
  vector<Type *> CallInfoFields =
    args<Type *>::list(int32, int32, int32, int8ptr, int8ptr_arr);
  return StructType::get(ctx, CallInfoFields);
}

}
