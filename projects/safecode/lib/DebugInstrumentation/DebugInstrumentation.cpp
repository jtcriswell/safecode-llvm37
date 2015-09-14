//===- DebugInstrumentation.cpp - Modify run-time checks to track debug info -//
// 
//                     The LLVM Compiler Infrastructure
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This pass modifies calls to the pool allocator and SAFECode run-times to
// track source level debugging information.
//
// Notes:
//  Some of this code is based off of code from the getLocationInfo() method in
//  LLVM.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "debug-instrumentation"


#include "llvm/ADT/Statistic.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/GetElementPtrTypeIterator.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"

#include "safecode/DebugInstrumentation.h"
#include "safecode/Utility.h"

#include <cstdlib>
#include <vector>

using namespace llvm;

namespace llvm {

char DebugInstrument::ID = 0;

// Register the pass
static
RegisterPass<DebugInstrument> X ("debuginstrument",
                                 "Add Debug Data to SAFECode Run-Time Checks");

static int tagCounter = 0;

//
// Basic LLVM Types
//
static Type * VoidType  = 0;
static Type * Int8Type  = 0;
static Type * Int32Type = 0;

///////////////////////////////////////////////////////////////////////////
// Command line options
///////////////////////////////////////////////////////////////////////////

namespace {
  ///////////////////////////////////////////////////////////////////////////
  // Pass Statistics
  ///////////////////////////////////////////////////////////////////////////
  STATISTIC (FoundSrcInfo,   "Number of Source Information Locations Found");
  STATISTIC (QueriedSrcInfo, "Number of Source Information Locations Queried");
}

///////////////////////////////////////////////////////////////////////////
// Static Functions
///////////////////////////////////////////////////////////////////////////

//
// Function: copyToDefaultSection()
//
// Description:
//  This function examines the specified LLVM value and determines if it is
//  a GEP into a global value in a special section.  If it is, it makes a copy
//  of the global in the default section and returns a pointer to it.
//
// Inputs:
//  V - The value to process.
//
// Return value:
//  Either V is returned or a pointer to a new GlobalVariable in the default
//  section is returned.
//
static inline Value *
copyToDefaultSection (Value * V) {
  if (ConstantExpr * GEP = dyn_cast<ConstantExpr>(V)) {
    if (GlobalVariable * GV = dyn_cast<GlobalVariable>(GEP->getOperand(0))) {
      if (GV->hasSection()) {
        //
        // Get the module in which this value belongs.
        //
        Module * M = GV->getParent();

        //
        // Get the element type of the global variable.
        //
        Type * Ty = GV->getType()->getElementType();
        GlobalVariable * SrcGV = new GlobalVariable (*M,
                                                     Ty,
                                                     GV->isConstant(),
                                                     GV->getLinkage(),
                                                     GV->getInitializer(),
                                                     GV->getName(),
                                                     0,
                                                     GV->getThreadLocalMode(),
                                                     0);
        SrcGV->copyAttributesFrom (GV);
        SrcGV->setSection ("");
        return SrcGV;
      }
    }
  }

  return V;
}

///////////////////////////////////////////////////////////////////////////
// Class Methods
///////////////////////////////////////////////////////////////////////////

GetSourceInfo::~GetSourceInfo() {}

//
// Method: operator()
//
// Description:
//  Return the source information associated with the call instruction by
//  finding the location within the source code in which the call is made.
//
// Inputs:
//  CI - The call instruction
//
// Return value:
//  A pair of LLVM values.  The first is the source file name; the second is
//  the line number.  Default values are given if no source line information
//  can be found.
//
std::pair<Value *, Value *>
LocationSourceInfo::operator() (CallInst * CI) {
  static int count=0;

  //
  // Update the number of source locations queried.
  //
  ++QueriedSrcInfo;

  //
  // Create default debugging values in case we don't find any debug
  // information.  The filename becomes the function name (if the function
  // has a name) and the line number becomes a unique identifier.
  //
  std::string filename = "<unknown>";
  unsigned int lineno  = ++count;
  if (CI->getParent()->getParent()->hasName())
    filename = CI->getParent()->getParent()->getName();

  //
  // Get the line number and source file information for the call if it exists.
  //
  if (MDNode *Dbg = CI->getMetadata(dbgKind)) {
    DILocation *Loc = (DILocation*) Dbg;
    filename = Loc->getDirectory().str() + "/" + Loc->getFilename().str();
    lineno   = Loc->getLine();
    ++FoundSrcInfo;
  }

  //
  // Convert the source filename and line number information into LLVM values.
  //
  Value * LineNumber = ConstantInt::get (Int32Type, lineno);
  Value * SourceFile;
  if (SourceFileMap.find (filename) != SourceFileMap.end()) {
    SourceFile = SourceFileMap[filename];
  } else {
    Constant * FInit = ConstantDataArray::getString (CI->getContext(),
                                                     filename);
    Module * M = CI->getParent()->getParent()->getParent();
    SourceFile = new GlobalVariable (*M,
                                     FInit->getType(),
                                     true,
                                     GlobalValue::InternalLinkage,
                                     FInit,
                                     "sourcefile");
    SourceFileMap[filename] = SourceFile;
  }

  return std::make_pair (SourceFile, LineNumber);
}

//
// Method: operator()
//
// Description:
//  Return the source information associated with a value within the call
//  instruction.  This is mainly intended to provide better source file
//  information to poolregister() calls.
//
// Inputs:
//  CI - The call instruction
//
// Return value:
//  A pair of LLVM values.  The first is the source file name; the second is
//  the line number.  Default values are given if no source line information
//  can be found.
//
std::pair<Value *, Value *>
VariableSourceInfo::operator() (CallInst * CI) {
  assert (((CI->getNumOperands()) > 2) &&
          "Not enough information to get debug info!\n");

  Value * LineNumber;
  Value * SourceFile;

  //
  // Create a default line number and source file information for the call.
  //
  LineNumber = ConstantInt::get (Int32Type, 0);
  std::string filename = "<unknown>";
  Module * M = CI->getParent()->getParent()->getParent();

  //
  // Get the value for which we want debug information.
  //
  CallSite CS(CI);
  Value * V = CS.getArgument(1)->stripPointerCasts();
  
  //
  // Try to get information about where in the program the value was allocated.
  //
  if (GlobalVariable * GV = dyn_cast<GlobalVariable>(V)) {
    NamedMDNode *NMD = M->getNamedMetadata("llvm.dbg.gv");
    if (NMD) {
      for (unsigned i = 0, e = NMD->getNumOperands(); i != e; ++i) {
        MDNode* MD = NMD->getOperand(i);
	if (!isa<DIGlobalVariable>(*MD))
          continue;
        if (cast_or_null<GlobalVariable, Constant>((cast_or_null<DIGlobalVariable, MDNode>(NMD->getOperand(i)))->getVariable()) == GV) {
          DIGlobalVariable *Var = cast_or_null<DIGlobalVariable>(NMD->getOperand(i));
          LineNumber = ConstantInt::get (Int32Type, Var->getLine());
          filename = (Var->getDirectory() + "/" + Var->getFilename()).str();
        }
      }
    }
  } else {
    if (Instruction *I = dyn_cast<Instruction>(V))
      if (MDNode *Dbg = I->getMetadata(dbgKind)) {
        DILocation *Loc = (DILocation*) Dbg;
        filename = Loc->getDirectory().str() + "/" + Loc->getFilename().str();
        LineNumber = ConstantInt::get (Int32Type, Loc->getLine());
      }
  }
  if (SourceFileMap.find (filename) != SourceFileMap.end()) {
    SourceFile = SourceFileMap[filename];
  } else {
    Constant * FInit = ConstantDataArray::getString (CI->getContext(),
                                                     filename);
    Module * M = CI->getParent()->getParent()->getParent();
    SourceFile = new GlobalVariable (*M,
                                     FInit->getType(),
                                     true,
                                     GlobalValue::InternalLinkage,
                                     FInit,
                                     "sourcefile");
    SourceFileMap[filename] = SourceFile;
  }
  return std::make_pair (SourceFile, LineNumber);
}


//
// Method: processFunction()
//
// Description:
//  Process each function in the module.
//
// Inputs:
//  F - The function to transform into a debug version.  This *can be NULL.
//
void
DebugInstrument::transformFunction (Function * F, GetSourceInfo & SI) {
  // If the function does not exist within the module, it does not need to
  // be transformed.
  if (!F) return;

  //
  // Create the function prototype for the debug version of the function.  This
  // function will have an identical type to the original *except* that it will
  // have additional debug parameters at the end.
  //
  const FunctionType * FuncType = F->getFunctionType();
  std::vector<Type *> ParamTypes (FuncType->param_begin(),
                                  FuncType->param_end());
  //
  // See note on vararg functions below.
  //
  if (!F->isVarArg())
  {
    ParamTypes.push_back (Int32Type);
    ParamTypes.push_back (VoidPtrTy);
    ParamTypes.push_back (Int32Type);
  }

  //
  // Check to see if the debug version of the function already exists.
  //
  bool hadToCreateFunction = true;
  if (F->getParent()->getFunction(F->getName().str() + "_debug"))
    hadToCreateFunction = false;

  //
  // Create the expected type of the debug version. Note: For functions that
  // take a variable number of arguments, this is set up so that the debugging
  // information will be pushed back at the end of the variable argument list.
  //
  FunctionType * DebugFuncType = FunctionType::get (FuncType->getReturnType(),
                                                    ParamTypes,
                                                    F->isVarArg());
  std::string funcdebugname = F->getName().str() + "_debug";
  Constant * FDebug = F->getParent()->getOrInsertFunction (funcdebugname,
                                                           DebugFuncType);

#if 0
  //
  // Give the function a body.  This is used for ensuring that SAFECode plays
  // nicely with LLVM's bugpoint tool.  By having a body, the program will link
  // correctly even when the intrinsic renaming pass is removed by bugpoint.
  //
  if (hadToCreateFunction) {
    Function * DebugFunc = dyn_cast<Function>(FDebug);
    assert (DebugFunc);

    LLVMContext & Context = F->getContext();
    BasicBlock * entryBB=BasicBlock::Create (Context, "entry", DebugFunc);
    Type * VoidTy = Type::getVoidTy(Context);
    if (DebugFunc->getReturnType() == VoidTy) {
      ReturnInst::Create (Context, entryBB);
    } else {
      Value * retValue = UndefValue::get (DebugFunc->getReturnType());
      ReturnInst::Create (Context, retValue, entryBB);
    }
  }
#endif

  //
  // Create a set of call instructions that must be modified.
  //
  std::vector<CallInst *> Worklist;
  Function::use_iterator i, e;
  for (i = F->use_begin(), e = F->use_end(); i != e; ++i) {
    if (CallInst * CI = dyn_cast<CallInst>(*i)) {
      Worklist.push_back (CI);
    }
  }

  //
  // Process all call instructions in the worklist.
  //
  for (unsigned index = 0; index < Worklist.size(); ++index) {
    //
    // Get a call instruction off of the work list.
    //
    CallInst * CI = Worklist[index];
    CallSite CS (CI);

    //
    // Get the line number and source file information for the call.
    //
    Value * LineNumber;
    Value * SourceFile;
    std::pair<Value *, Value *> Info = SI (CI);
    SourceFile = Info.first;
    LineNumber = Info.second;

    //
    // If the source filename is in the meta-data section, make a copy of it in
    // the default section.  This ensures that it gets code generated.
    //
    SourceFile = copyToDefaultSection (SourceFile);

    //
    // Transform the function call.
    //
    std::vector<Value *> args;
    args.insert (args.end(), CS.arg_begin(), CS.arg_end());
    args.push_back (ConstantInt::get(Int32Type, tagCounter++));
    args.push_back (castTo (SourceFile, VoidPtrTy, "", CI));
    args.push_back (LineNumber);
    CallInst * NewCall = CallInst::Create (FDebug,
                                           args,
                                           CI->getName(),
                                           CI);
    CI->replaceAllUsesWith (NewCall);
    CI->eraseFromParent();
  }

  return;
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
DebugInstrument::runOnModule (Module &M) {
  // Create the void pointer type
  VoidPtrTy = getVoidPtrType(M);

  //
  // Create needed LLVM types.
  //
  VoidType  = Type::getVoidTy(M.getContext());
  Int8Type  = IntegerType::getInt8Ty(M.getContext());
  Int32Type = IntegerType::getInt32Ty(M.getContext());

  //
  // Get the ID number for debug metadata.
  //
  unsigned dbgKind = M.getContext().getMDKindID("dbg");

  //
  // Transform allocations, load/store checks, and bounds checks.
  //
  LocationSourceInfo LInfo (dbgKind);
  VariableSourceInfo VInfo (dbgKind);

  // Check and registration functions
  transformFunction (M.getFunction ("poolfree"), LInfo);
  transformFunction (M.getFunction ("poolcheck"), LInfo);
  transformFunction (M.getFunction ("poolcheckui"), LInfo);
  transformFunction (M.getFunction ("poolcheckstr"), LInfo);
  transformFunction (M.getFunction ("poolcheckstrui"), LInfo);
  transformFunction (M.getFunction ("poolcheckalign"), LInfo);
  transformFunction (M.getFunction ("poolcheckalignui"), LInfo);
  transformFunction (M.getFunction ("poolcheck_free"), LInfo);
  transformFunction (M.getFunction ("poolcheck_freeui"), LInfo);
  transformFunction (M.getFunction ("boundscheck"), LInfo);
  transformFunction (M.getFunction ("boundscheckui"), LInfo);
  transformFunction (M.getFunction ("exactcheck2"), LInfo);
  transformFunction (M.getFunction ("fastlscheck"), LInfo);
  transformFunction (M.getFunction ("funccheck"), LInfo);
  transformFunction (M.getFunction ("funccheckui"), LInfo);
  transformFunction (M.getFunction ("pool_register"), LInfo);
#if 0
  transformFunction (M.getFunction ("pool_register_global"), LInfo);
#endif
  transformFunction (M.getFunction ("pool_register_stack"), LInfo);
  transformFunction (M.getFunction ("pool_unregister"), LInfo);
  transformFunction (M.getFunction ("pool_unregister_stack"), LInfo);
  transformFunction (M.getFunction ("pool_reregister"), LInfo);

  // Format string function intrinsic
  transformFunction (M.getFunction ("__sc_fscallinfo"), LInfo);

  // Standard C library wrappers
  transformFunction (M.getFunction ("pool_memccpy"), LInfo);
  transformFunction (M.getFunction ("pool_memchr"), LInfo);
  transformFunction (M.getFunction ("pool_memcmp"), LInfo);
  transformFunction (M.getFunction ("pool_memcpy"), LInfo);
  transformFunction (M.getFunction ("pool_memmove"), LInfo);
  transformFunction (M.getFunction ("pool_memset"), LInfo);
  transformFunction (M.getFunction ("pool_strcat"), LInfo);
  transformFunction (M.getFunction ("pool_strchr"), LInfo);
  transformFunction (M.getFunction ("pool_strcmp"), LInfo);
  transformFunction (M.getFunction ("pool_strcoll"), LInfo);
  transformFunction (M.getFunction ("pool_strcpy"), LInfo);
  transformFunction (M.getFunction ("pool_strcspn"), LInfo);
  transformFunction (M.getFunction ("pool_strlen"), LInfo);
  transformFunction (M.getFunction ("pool_strncat"), LInfo);
  transformFunction (M.getFunction ("pool_strncmp"), LInfo);
  transformFunction (M.getFunction ("pool_strncpy"), LInfo);
  transformFunction (M.getFunction ("pool_strpbrk"), LInfo);
  transformFunction (M.getFunction ("pool_strrchr"), LInfo);
  transformFunction (M.getFunction ("pool_strspn"), LInfo);
  transformFunction (M.getFunction ("pool_strstr"), LInfo);
  transformFunction (M.getFunction ("pool_strxfrm"), LInfo);
  transformFunction (M.getFunction ("pool_mempcpy"), LInfo);
  transformFunction (M.getFunction ("pool_strcasestr"), LInfo);
  transformFunction (M.getFunction ("pool_stpcpy"), LInfo);
  transformFunction (M.getFunction ("pool_strnlen"), LInfo);
  transformFunction (M.getFunction ("pool_bcmp"), LInfo);
  transformFunction (M.getFunction ("pool_bcopy"), LInfo);
  transformFunction (M.getFunction ("pool_bzero"), LInfo);
  transformFunction (M.getFunction ("pool_index"), LInfo);
  transformFunction (M.getFunction ("pool_rindex"), LInfo);
  transformFunction (M.getFunction ("pool_strcasestr"), LInfo);
  transformFunction (M.getFunction ("pool_strcasecmp"), LInfo);
  transformFunction (M.getFunction ("pool_strncasecmp"), LInfo);
  transformFunction (M.getFunction ("pool_vprintf"), LInfo);
  transformFunction (M.getFunction ("pool_vfprintf"), LInfo);
  transformFunction (M.getFunction ("pool_vsprintf"), LInfo);
  transformFunction (M.getFunction ("pool_vsnprintf"), LInfo);
  transformFunction (M.getFunction ("pool_vscanf"), LInfo);
  transformFunction (M.getFunction ("pool_vfscanf"), LInfo);
  transformFunction (M.getFunction ("pool_vsscanf"), LInfo);
  transformFunction (M.getFunction ("pool_vsyslog"), LInfo);
  transformFunction (M.getFunction ("pool_fgets"), LInfo);
  transformFunction (M.getFunction ("pool_fputs"), LInfo);
  transformFunction (M.getFunction ("pool_puts"), LInfo);
  transformFunction (M.getFunction ("pool_gets"), LInfo);
  transformFunction (M.getFunction ("pool_tmpnam"), LInfo);
  transformFunction (M.getFunction ("pool_fread"), LInfo);
  transformFunction (M.getFunction ("pool_fwrite"), LInfo);
  transformFunction (M.getFunction ("pool_read"), LInfo);
  transformFunction (M.getFunction ("pool_recv"), LInfo);
  transformFunction (M.getFunction ("pool_recvfrom"), LInfo);
  transformFunction (M.getFunction ("pool_write"), LInfo);
  transformFunction (M.getFunction ("pool_send"), LInfo);
  transformFunction (M.getFunction ("pool_sendto"), LInfo);
  transformFunction (M.getFunction ("pool_readdir_r"), LInfo);
  transformFunction (M.getFunction ("pool_readlink"), LInfo);
  transformFunction (M.getFunction ("pool_realpath"), LInfo);
  transformFunction (M.getFunction ("pool_getcwd"), LInfo);

  return true;
}

}

