//===- AllocatorInfo.h ------------------------------------------*- C++ -*----//
// 
//                     The LLVM Compiler Infrastructure
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// Define the abstraction of a pair of allocator / deallocator, including:
//
//   * The size of the object being allocated. 
//   * Whether the size may be a constant, which can be used for exactcheck
// optimization.
//
//===----------------------------------------------------------------------===//

#ifndef SAFECODE_SUPPORT_ALLOCATORINFO_H
#define SAFECODE_SUPPORT_ALLOCATORINFO_H

#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Pass.h"

#include "safecode/Utility.h"

#include <stdint.h>
#include <string>

namespace llvm {

/// AllocatorInfo - define the abstraction of a pair of allocator / deallocator.
class AllocatorInfo {
  public:
    AllocatorInfo(const std::string & allocCallName, 
                  const std::string & freeCallName) : 
    allocCallName(allocCallName), freeCallName(freeCallName) {}

    virtual ~AllocatorInfo();
    /// Test whether the size of a particular allocation site may be a constant.
    /// This is used to determined whether SAFECode can perform an exactcheck
    /// optimization on the particular allocation site.
    /// 
    /// For simple allocators such as malloc() / poolalloc(), that is always
    /// true. However, allocators such as kmem_cache_alloc() put the size of
    /// allocation inside a struct, which needs extra instructions to get the
    /// size. We don't want to get into this complexity right now, even running
    /// ADCE right after exactcheck optimization might fix the problem.
    ///
    virtual bool isAllocSizeMayConstant(Value * AllocSite) const {
      return true;
    }

    /// Return the size of the object being allocated
    /// Assume the caller knows it is an allocation for this allocator
    /// Return NULL when something is wrong
    virtual Value * getAllocSize(Value * AllocSite) const = 0;

    /// Return the size of the object being allocated; insert code into the
    /// program to compute the size if necessary
    virtual Value * getOrCreateAllocSize (Value * AllocSite) const = 0;

    /// Return the pointer being freed
    /// Return NULL when something is wrong
    virtual Value * getFreedPointer(Value * FreeSite) const = 0;

    /// Return the function name of the allocator, say "malloc".
    const std::string & getAllocCallName() const { return allocCallName; }

    /// Return the function name of the deallocator, say "free".
    const std::string & getFreeCallName() const { return freeCallName; }

  protected:
    std::string allocCallName;
    std::string freeCallName;
};

/// SimpleAllocatorInfo - define the abstraction of simple allcators /
/// deallocators such as malloc / free
class SimpleAllocatorInfo : public AllocatorInfo {
  public:
    SimpleAllocatorInfo(const std::string & allocCallName, 
                        const std::string & freeCallName,
                        uint32_t allocSizeOperand,
                        uint32_t freePtrOperand) :
      AllocatorInfo(allocCallName, freeCallName),
      allocSizeOperand(allocSizeOperand), freePtrOperand(freePtrOperand) {}
    virtual Value * getAllocSize(Value * AllocSite) const;
    virtual Value * getOrCreateAllocSize(Value * AllocSite) const;
    virtual Value * getFreedPointer(Value * FreeSite) const;

  protected:
    uint32_t allocSizeOperand;
    uint32_t freePtrOperand;
};

/// ReAllocatorInfo - define the abstraction of simple reallcators /
/// deallocators such as realloc / free
class ReAllocatorInfo : public SimpleAllocatorInfo {
  public:
    ReAllocatorInfo(const std::string & allocCallName, 
                    const std::string & freeCallName,
                    uint32_t allocSizeOperand,
                    uint32_t allocPtrOperand,
                    uint32_t freePtrOperand) :
      SimpleAllocatorInfo(allocCallName,
                          freeCallName,
                          allocSizeOperand,
                          freePtrOperand),
      allocPtrOperand(allocPtrOperand) {}
    virtual Value * getAllocedPointer (Value * AllocSite) const;

  protected:
    uint32_t allocPtrOperand;
};

/// ArrayAllocatorInfo - define the abstraction of array allocators /
/// deallocators such as calloc / free
class ArrayAllocatorInfo : public SimpleAllocatorInfo {
  protected:
    // The index of the operand used for the number of elements to allocate
    uint32_t allocNumOperand;

  public:
    ArrayAllocatorInfo(const std::string & allocCallName, 
                       const std::string & freeCallName,
                       uint32_t allocSizeOperand,
                       uint32_t allocNumOperand,
                       uint32_t freePtrOperand) :
      SimpleAllocatorInfo (allocCallName, freeCallName, allocSizeOperand, freePtrOperand), allocNumOperand (allocNumOperand) {}
    virtual Value * getAllocSize(Value * AllocSite) const {
      return 0;
    }
    virtual Value * getOrCreateAllocSize(Value * AllocSite) const;
};

/// StringAllocatorInfo - define the abstraction of string allocator functions
class StringAllocatorInfo : public SimpleAllocatorInfo {
  public:
    StringAllocatorInfo(const std::string & allocCallName, 
                        const std::string & freeCallName,
                        uint32_t freePtrOperand) :
      SimpleAllocatorInfo (allocCallName, freeCallName, 0, freePtrOperand) {}
    virtual Value * getAllocSize(Value * AllocSite) const {
      return 0;
    }
    virtual Value * getOrCreateAllocSize(Value * AllocSite) const;
};

//
// Pass: AllocatorInfoPass
//
// Description:
//  This is a pass that can be queried to find information about various
//  allocation functions.
//
class AllocatorInfoPass : public ImmutablePass {
  public:
    typedef std::vector<AllocatorInfo *> AllocatorInfoListTy;
    typedef AllocatorInfoListTy::iterator alloc_iterator;
    typedef std::vector<ReAllocatorInfo *> ReAllocatorInfoListTy;
    typedef ReAllocatorInfoListTy::iterator realloc_iterator;

  protected:
    // List of allocator/deallocator functions
    AllocatorInfoListTy allocators;

    // List of reallocator functions
    ReAllocatorInfoListTy reallocators;

  public:
    // Pass members and methods
    static char ID;
    AllocatorInfoPass() : ImmutablePass(ID) {
      //
      // Register the standard C library allocators.
      //
      static SimpleAllocatorInfo CPP1Allocator   ("_Znwm", "_ZdlPv", 1, 1);
      static SimpleAllocatorInfo CPP2Allocator   ("_Znam", "_ZdaPv", 1, 1);
      static SimpleAllocatorInfo CPP3Allocator   ("_Znwj", "", 1, 1);
      static SimpleAllocatorInfo CPP4Allocator   ("_Znaj", "", 1, 1);
      static SimpleAllocatorInfo MallocAllocator ("malloc", "free", 1, 1);
      static ArrayAllocatorInfo  CallocAllocator ("calloc", "", 1, 2, 1);
      static ReAllocatorInfo     ReAllocator     ("realloc", "", 2, 1, 1);
      static StringAllocatorInfo StrdupAllocator ("strdup", "", 1);
      static StringAllocatorInfo GetenvAllocator ("getenv", "", 0);

      // Add the standard C allocators
      addAllocator   (&MallocAllocator);
      addAllocator   (&CallocAllocator);
      addReAllocator (&ReAllocator);

      // Add the C++ allocators
      addAllocator   (&CPP1Allocator);
      addAllocator   (&CPP2Allocator);
      addAllocator   (&CPP3Allocator);
      addAllocator   (&CPP4Allocator);

      // Add the string allocator functions
      addAllocator   (&StrdupAllocator);
      addAllocator   (&GetenvAllocator);
      return;
    }

    virtual bool runOnModule (Module & M) {
      //
      // Ensure that a prototype for strlen() exists.
      //
      const DataLayout & TD = M.getDataLayout();
      M.getOrInsertFunction ("strlen",
                             TD.getIntPtrType(M.getContext(), 0),
                             getVoidPtrType(M.getContext()),
                             NULL);
      return true;
    }

    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.setPreservesAll();
      return;
    }

    // Iterator methods
    alloc_iterator alloc_begin() { return allocators.begin(); }
    alloc_iterator alloc_end() { return allocators.end(); }
    realloc_iterator realloc_begin() { return reallocators.begin(); }
    realloc_iterator realloc_end() { return reallocators.end(); }

    // Methods to add allocators
    void addAllocator (AllocatorInfo * Allocator) {
      allocators.push_back (Allocator);
    }

    void addReAllocator (ReAllocatorInfo * Allocator) {
      reallocators.push_back (Allocator);
    }

    Value * getObjectSize(Value * V);
};

}

#endif
