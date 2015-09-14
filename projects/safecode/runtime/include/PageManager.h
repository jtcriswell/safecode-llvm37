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

#ifndef PAGEMANAGER_H
#define PAGEMANAGER_H

#include <stdint.h>

#include <vector>

namespace llvm {

//
// Value used to initialize memory.  We use zero because, when repeated, it
// maps to an unmapped virtual address on nearly any operating system.
//
static const unsigned initvalue = 0x00;

/// PageMultipler - This variable holds the ratio between physical pages and
/// the number of pages returned by AllocatePage.
///
/// NOTE:
///   The size of a page returned from AllocatePage *must* be under 64K.  This
///   is because the pool slab using 16 bit integers to index into the slab.
#define PAGEMULT 16
static const unsigned int PageMultiplier=PAGEMULT;

/// NumToAllocate - This variable specifies the number of pages of size
///                 PageMultipler to allocate at a time.
static const unsigned NumToAllocate = 8;

/// NumShadows - This variable specifies the number of shadows that should be
/// created automatically for every piece of memory created by AllocatePage().
static const unsigned int NumShadows=4;

/// InitializePageManager - This function must be called before any other page
/// manager accesses are performed.  It may be called multiple times.
/// 
void InitializePageManager();

void *GetPages(unsigned NumPages);

/// PageSize - Contains the size of the unit of memory allocated by
/// AllocatePage.  This is a value that is typically several kilobytes in size,
/// and is guaranteed to be a power of two.
///
extern "C" uintptr_t PageSize;

/// PPageSize - Contains the size of a single physical page.  This is the
/// smallest granularity at which virtual memory operations can be performed.
extern uintptr_t PPageSize;

/// AllocatePage - This function returns a chunk of memory with size and
/// alignment specified by getPageSize().
void * AllocatePage();

// RemapObject - This function is used by the dangling pool allocator in order
//               to remap canonical pages to shadow pages.
void * RemapObject(void* va, unsigned NumByte);

/// AllocateNPages - Allocates Num number of pages
void *AllocateNPages(unsigned Num);

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

/// FreePage - This function returns the specified page to the pagemanager for
/// future allocation.
void FreePage(void *Page);

// The set of free memory pages we retrieved from the OS.
typedef std::vector<void*> FreePagesListType;
extern FreePagesListType FreePages;

}

#endif
