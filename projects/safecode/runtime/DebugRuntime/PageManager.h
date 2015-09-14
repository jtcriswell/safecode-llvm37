//===- PageManager.h - Allocates memory on page boundaries ------*- C++ -*-===//
// 
//                          The SAFECode Compiler 
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file defines the interface used by the pool allocator to allocate memory
// on large alignment boundaries.
//
//===----------------------------------------------------------------------===//

#ifndef _SC_DEBUG_PAGEMANAGER_H_
#define _SC_DEBUG_PAGEMANAGER_H_

#include "../include/PageManager.h"

namespace llvm {

/// Special implemetation for dangling pointer detection

// RemapObject - This function is used by the dangling pool allocator in order
//               to remap canonical pages to shadow pages.
void * RemapObject(void* va, unsigned NumByte);

// MProtectPage - Protects Page passed in by argument, raising an exception
//                or traps at future access to Page
void MProtectPage(void * Page, unsigned NumPages);

// ProtectShadowPage - Protects shadow page that begins at beginAddr, spanning
//                     over NumPages
void ProtectShadowPage(void * beginPage, unsigned NumPPages);

// UnprotectShadowPage - Unprotects the shadow page in the event of fault when
//                       accessing protected shadow page in order to
//                       resume execution
void UnprotectShadowPage(void * beginPage, unsigned NumPPage);

}
#endif
