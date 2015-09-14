//===- RewritePtr.h - Header file for Rewrite Pointers ----------*- C++ -*-===//
// 
//                         The SAFECode Compiler
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file defines functions for use with rewrite pointers.
//
//===----------------------------------------------------------------------===//

#ifndef _SC_REWRITEPTR_H
#define _SC_REWRITEPTR_H

NAMESPACE_SC_BEGIN

//
// The lower and upper bound of an unmapped memory region.  This range is used
// for rewriting pointers that go one beyond the edge of an object so that they
// can be used for comparisons but will generate a fault if used for loads or
// stores.
//
// There are a few restrictions:
//  1) I *think* InvalidUpper must be on a page boundary.
//  2) None of the values can be reserved pointer values.  Such values include:
//      0 - This is the NULL pointer.
//      1 - This is a reserved pointer in the Linux kernel.
//      2 - This is another reserved pointer in the Linux kernel.
//
// Here's the breakdown of how it works on various operating systems:
//  o) Linux           - We use the kernel's reserved address space (which is
//                       inaccessible from applications).
//  o) Other platforms - We allocate a range of memory and disable read and
//                       write permissions for the pages contained within it.
//
extern uintptr_t InvalidUpper;
extern uintptr_t InvalidLower;

// Map between rewrite pointer and source file information
extern llvm::DenseMap<void *, const char*>  RewriteSourcefile;
extern llvm::DenseMap<void *, unsigned>     RewriteLineno;
extern std::map<const void *, const void *> RewrittenPointers;
extern llvm::DenseMap<void *,
                      std::pair<void *, void * > > RewrittenObjs;

//
// Function: isRewritePtr()
//
// Description:
//  Determines whether the specified pointer value is a rewritten value for an
//  Out-of-Bounds pointer value.
//
// Return value:
//  true  - The pointer value is an OOB pointer rewrite value.
//  false - The pointer value is the actual value of the pointer.
//
static inline bool
isRewritePtr (void * p) {
  uintptr_t ptr = (uintptr_t) p;

  if ((InvalidLower < ptr ) && (ptr < InvalidUpper))
    return true;
  return false;
}

//
// Function: getOOBObject()
//
// Description:
//  Given a pointer, determine if it is an OOB pointer.  If it is, determine
//  the bounds of the object from whence it came and return them to the caller.
//
// Inputs:
//  p - The pointer for which bounds information is requested.
//
// Outputs:
//  start - The first address of the memory object.
//  end   - The last valid address of the memory object.
//
// Return value:
//  true  - The pointer was an OOB pointer.
//  false - The pointer was not an OOB pointer.
//
static inline bool
getOOBObject (void * p, void * & start, void * & end) {
  // Record from which object an OOB pointer originates
  extern llvm::DenseMap<void *, std::pair<void *, void * > >
  RewrittenObjs;

  if (isRewritePtr (p)) {
    // FIXME: the casts are hacks to deal with the C++ type system
    start = const_cast<void*>(RewrittenObjs[p].first);
    end   = const_cast<void*>(RewrittenObjs[p].second);
    return true;
  }

  return false;
}

NAMESPACE_SC_END

#endif

