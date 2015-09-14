//===- MSCInfo.h - Memory safety check info ---------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the generic MSCInfo interface, which is used as the common
// interface for identifying memory safety checks.
//
//===----------------------------------------------------------------------===//

#ifndef MEMORY_SAFETY_CHECK_INFO_H_
#define MEMORY_SAFETY_CHECK_INFO_H_

#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

#include <vector>

namespace llvm {

struct CheckInfo {
  enum CheckType {
    MemoryCheck = 0,
    GEPCheck = 1,
    FuncCheck = 2,
    FreeCheck = 3,
    GlobalRegistration = 4,
    StackRegistration = 5,
    StackUnregistration = 6,
    HeapRegistration = 7,
    HeapUnregistration = 8
  };

  StringRef Name;
  const CheckInfo *FastVersionInfo;
  CheckType Type;
  int PtrArgNo, SizeArgNo;
  int ObjArgNo, ObjSizeArgNo;
  int DestPtrArgNo;
  bool IsStoreCheck, IsFastCheck;
  StringRef FailureName;

  CheckInfo(StringRef Name, const CheckInfo *FastVersionInfo, CheckType Type,
            int PtrArgNo, int SizeArgNo, int ObjArgNo, int ObjSizeArgNo,
            int DestPtrArgNo, bool IsStoreCheck, bool IsFastCheck,
            StringRef FailureName):
            Name(Name), FastVersionInfo(FastVersionInfo), Type(Type),
            PtrArgNo(PtrArgNo), SizeArgNo(SizeArgNo), ObjArgNo(ObjArgNo),
            ObjSizeArgNo(ObjSizeArgNo), DestPtrArgNo(DestPtrArgNo),
            IsStoreCheck(IsStoreCheck), IsFastCheck(IsFastCheck),
            FailureName(FailureName) {}

  Function* getFunction(const Module &M) const {
    return M.getFunction(Name);
  }

  inline bool isMemoryCheck() const {
    return Type == MemoryCheck;
  }

  inline bool isFastMemoryCheck() const {
    return isMemoryCheck() && IsFastCheck;
  }

  inline bool isGEPCheck() const {
    return Type == GEPCheck;
  }

  inline bool isGlobalRegistration() const {
    return Type == GlobalRegistration;
  }

  inline bool isStackRegistration() const {
    return Type == StackRegistration;
  }

  inline bool isVariableRegistration() const {
    return Type == GlobalRegistration || Type == StackRegistration ||
           Type == HeapRegistration;
  }

  inline bool isVariableUnregistration() const {
    return Type == StackUnregistration || Type == HeapUnregistration;
  }

  inline StringRef getFailureFunctionName() const {
    return FailureName;
  }
};

typedef const CheckInfo CheckInfoType;
typedef std::vector <CheckInfoType*> CheckInfoListType;

class MSCInfo {
  MSCInfo *MSCI; // Previous memory safety checks pass to chain to.

protected:
  /// InitializeMSCInfo - Subclasses must call this method to initialize the
  /// MSCInfo interface before any other methods are called. This is typically
  /// called by the run* methods of these subclasses.
  /// This may be called multiple times.
  ///
  void InitializeMSCInfo(Pass *P);

  virtual void addCheckInfo(const CheckInfo *CI);

  /// getAnalysisUsage - All implementations should invoke this directly
  /// (using MSCInfo::getAnalysisUsage(AU)).
  virtual void getAnalysisUsage(AnalysisUsage &AU) const;

public:
  static char ID;
  MSCInfo(): MSCI(0) {}
  virtual ~MSCInfo() = 0;

  /// getCheckInfoList - Return a list of all known 
  virtual CheckInfoListType getCheckInfoList() const;
  virtual CheckInfoType* getCheckInfo(Function *F) const;
};

} // End llvm namespace

#endif
