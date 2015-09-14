//===- PageManager.cpp - Implementation of the page allocator -------------===//
// 
//                          The SAFECode Compiler
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file implements the PageManager.h interface.
//
//===----------------------------------------------------------------------===//

#include "ConfigData.h"
#include "PageManager.h"
#ifndef _POSIX_MAPPED_FILES
#define _POSIX_MAPPED_FILES
#endif

#include "poolalloc/Support/MallocAllocator.h"
#include "poolalloc/MMAPSupport.h"
#include "safecode/ADT/HashExtras.h"
#include "safecode/Runtime/BitmapAllocator.h"

#include <unistd.h>

#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <vector>
#include <utility>

#include "llvm/ADT/DenseMap.h"

// this is for dangling pointer detection in Mac OS X
#if defined(__APPLE__)
#include <mach/mach_vm.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#endif

NAMESPACE_SC_BEGIN

//
// Structure: ShadowInfo
//
// Description:
//  This structure provides information on a pre-created shadow page.
//
struct ShadowInfo {
  // Start address of the shadow page
  void * ShadowStart;

  // Flag bits indicating which physical pages within the shadow are in use
  unsigned short InUse;
};

// Map canonical pages to their shadow pages
hash_map<void *,std::vector<struct ShadowInfo> > ShadowPages;

// If not compiling on Mac OS X, define types and values to make the same code
// work on multiple platforms.
#if !defined(__APPLE__)
typedef int kern_return_t;
static const int KERN_SUCCESS=0;
#endif

#if defined(__APPLE__)
//
// Function: RemapPages()
//
// Description:
//  This function takes a virtual, page aligned address and a length and remaps
//  the memory so that the underlying physical pages appear in multiple
//  locations within the virtual memory.
//
// Inputs:
//  va     - Virtual address of the first page to double map.
//  length - The length, in bytes of the memory to be remapped.
//
static void *
RemapPages (void * va, unsigned length) {
  kern_return_t      kr;
  mach_vm_address_t  target_addr = 0;
  mach_vm_address_t  source_addr;
  vm_prot_t          prot_cur = VM_PROT_READ | VM_PROT_WRITE;
  vm_prot_t          prot_max = VM_PROT_READ | VM_PROT_WRITE;
  vm_map_t           self     = mach_task_self();

  source_addr = (mach_vm_address_t) ((uintptr_t)va & ~(PPageSize - 1));
  uintptr_t offset = (uintptr_t)va & (PPageSize - 1);
  unsigned NumPPage = 0;

  NumPPage = (length / PPageSize) + 1;

  //if((unsigned)va > 0x2f000000) {
  //  logregs = 1;
  //}

  if (logregs) {
    fprintf (stderr, " RemapPage:117: source_addr = %p, offset = %p, NumPPage = %d\n", (void*)source_addr, (void *) offset, NumPPage);
    fflush(stderr);
  }

#if 0
  // FIX ME!! when there's time, check out why this doesn't work
  if ( (length - (NumPPage-1) * PPageSize) > (PPageSize - offset) ) {
    NumPPage++;
    length = NumPPage * PPageSize;
  }
#endif

  uintptr_t byteToMap = length + offset;

  if (logregs) {
    fprintf(stderr, " RemapPage127: remapping page of size %p covering %d page with offset %p and byteToMap = %p",
    (void *) length, NumPPage, (void *) offset, (void *) byteToMap);
    fflush(stderr);
  }
  kr = mach_vm_remap (self,
                      &target_addr,
                      byteToMap,
                      0,
                      TRUE,
                      self,
                      source_addr,
                      FALSE,
                      &prot_cur,
                      &prot_max,
                      VM_INHERIT_SHARE); 
 
  if (kr != KERN_SUCCESS) {
    fprintf(stderr, " mach_vm_remap error: %d \n", kr);
    fprintf(stderr, " failed to remap %p of memory from source_addr = %p\n", (void *) byteToMap, (void *)source_addr);
    //printf(" no of pages used %d %d  %d\n", AddressSpaceUsage1, AddressSpaceUsage2, AddressSpaceUsage2+AddressSpaceUsage1);
    fprintf(stderr, "%s\n", mach_error_string(kr));
    // Mach don't modify and don't mark the string as constants..
    // So I use a cast to get rid of compiler warnings.
    mach_error(const_cast<char*>("mach_vm_remap:"), kr); // just to make sure I've got this error right
    fflush(stderr);
    //goto repeat;
    //abort();
  }

  if (logregs) {
    fprintf(stderr, " RemapPage:160: remap succeeded to addr 0x%08x\n", (unsigned)target_addr);
    fflush(stderr);
  }
  va = (void *) target_addr;
  return va;
 
/* 
#ifdef STATISTIC
   AddressSpaceUsage2++;
#endif
*/
}
#else
void *
RemapPages (void * va, unsigned length) {
  void *  target_addr = 0;
  void *  source_addr;
  void *  finish_addr;

  //
  // Find the beginning and end of the physical pages for this memory object.
  //
  source_addr = (void *) ((unsigned long)va & ~(PPageSize - 1));
  finish_addr = (void *) (((unsigned long)va + length) & ~(PPageSize - 1));

  unsigned int NumPages = ((uintptr_t)finish_addr - (uintptr_t)source_addr) / PPageSize;
  if (!NumPages) NumPages = 1;

  //
  // Find the length in bytes of the memory we want to remap.
  //
  ptrdiff_t map_length = ((intptr_t) finish_addr - (intptr_t) source_addr) + PPageSize - 1;

  //
  // The below code seems to double map physical memory correctly.  However,
  // it does not remap the memory pages that the caller has requested.  I
  // suspect the code to calculate the length and address is somehow incorrect
  // and needs to be fixed.
  //
  // target_addr = mremap (va, 0, PPageSize, MREMAP_MAYMOVE);

#if 0
fprintf (stderr, "remap: %p %x -> %p %x\n", va, map_length, source_addr, map_length);
fflush (stderr);
#endif
  target_addr = mremap (source_addr, 0, map_length, MREMAP_MAYMOVE);
  if (target_addr == MAP_FAILED) {
    perror ("RemapPage: Failed to create shadow page: ");
  }

#if 0
  volatile unsigned int * p = (unsigned int *) source_addr;
  volatile unsigned int * q = (unsigned int *) target_addr;

  p[0] = 0xbeefbeef;
fprintf (stderr, "value: %p=%x, %p=%x\n", (void *) p, p[0], (void *) q, q[0]);
  p[0] = 0xdeeddeed;
fprintf (stderr, "value: %p=%x, %p=%x\n", (void *) p, p[0], (void *) q, q[0]);
fflush (stderr);
#endif
  return target_addr;
}
#endif

//
// Function: RemapObject()
//
// Description:
//  Create another mapping of the memory object so that it appears in multiple
//  locations of the virtual address space.
//
// Inputs:
//  va     - Virtual address of the memory object to remap.  It does not need
//           to be page aligned.
//
//  length - The length of the memory object in bytes.
//
// Return value:
//  Returns a pointer *to the page* that was remapped.
//
// Notes:
//  This function must generally determine the set of pages occupied by the
//  memory object and remap those pages.  This is because most operating
//  systems can only remap memory at page granularity.
//
// TODO:
//  The constant 12 used to compute StartPage only works when the physical page
//  size is 4096.  We need to make that configurable.
//
void *
RemapObject (void * va, unsigned length) {
  // Start of the page in which the object lives
  unsigned char * page_start;

  // Start of the physical page in which the object lives
  unsigned char * phy_page_start;

  // The offset within the physical page in which the object lives
  uintptr_t phy_offset = (unsigned long)va & (PPageSize - 1);
  //  unsigned offset     = (unsigned long)va & (PageSize - 1);

  //
  // Compute the location of the object relative to the page and physical page.
  //
  page_start     = (unsigned char *)((uintptr_t)va & ~(PageSize - 1));
  phy_page_start = (unsigned char *)((uintptr_t)va & ~(PPageSize - 1));

  uintptr_t StartPage = ((uintptr_t)phy_page_start >> 12) -
                        ((uintptr_t)page_start     >> 12);
  //unsigned EndPage   = ((phy_page_start + length - page_start) / PPageSize) + 1;

  //
  // If we're not remapping objects, don't do anything.
  //
  if (ConfigData.RemapObjects == false)
    return (void *)(phy_page_start);

  // Create a mask to easily tell if the needed pages are available
  uintptr_t mask = 0;
  for (uintptr_t i = StartPage; i < PageMultiplier; ++i) {
    if (((unsigned char *)(page_start) + i*PPageSize) <= ((unsigned char *)(va)+length) )
      mask |= (1u << i);
    else
      break;
  }

  //
  // First, look to see if a pre-existing shadow page is available.
  //
  if (ShadowPages.find(page_start) != ShadowPages.end()) {
    uintptr_t numfull = 0;
    for (uintptr_t i = 0; i < NumShadows; ++i) {
      struct ShadowInfo Shadow = ShadowPages[page_start][i];
      if ((Shadow.ShadowStart) && ((Shadow.InUse & mask) == 0)) {
        // Set the shadow pages as being used
        ShadowPages[page_start][i].InUse |= mask;

        // Return the pre-created shadow page
        return ((unsigned char *)(Shadow.ShadowStart) + (phy_page_start - page_start));
      }

      // Keep track of how many shadows are full
      if (Shadow.InUse == 0xffff) ++numfull;
    }

    //
    // If all of the shadow pages are full, remove this entry from the set of
    // ShadowPages.
    //
    if (numfull == NumShadows) {
      ShadowPages.erase(page_start);
    }
  }

  //
  // We could not find a pre-existing shadow page.  Create a new one.
  //
  void * p = (RemapPages (phy_page_start, length + phy_offset));
  assert (p && "New remap failed!\n");
  return p;
}

/// AllocatePage - This function returns a chunk of memory with size and
/// alignment specified by PageSize.
void *AllocatePage() {
  FreePagesListType &FPL = FreePages;

  if (!FPL.empty()) {
    void *Result = FPL.back();
      FPL.pop_back();
      return Result;
  }

  // Allocate several pages, and put the extras on the freelist...
  char *Ptr = (char*)GetPages(NumToAllocate);

  // Place all but the first page into the page cache
  for (unsigned i = 1; i != NumToAllocate; ++i) {
    FPL.push_back (Ptr+i*PageSize);
  }

  // Create several shadow mappings of all the pages
  if (ConfigData.RemapObjects) {
    char * NewShadows[NumShadows];
    for (unsigned i=0; i < NumShadows; ++i) {
      NewShadows[i] = (char *) RemapPages (Ptr, NumToAllocate * PageSize);
    }

    // Place the shadow pages into the shadow cache
    for (unsigned i = 0; i != NumToAllocate; ++i) {
      char * PagePtr = Ptr+i*PageSize;
      std::vector<struct ShadowInfo> & Shadows = ShadowPages[(void*)PagePtr];
      Shadows.reserve(NumShadows);
      for (unsigned j=0; j < NumShadows; ++j) {
        Shadows[j].ShadowStart = NewShadows[j]+(i*PageSize);
        Shadows[j].InUse       = 0;
      }
    }
  }

  return Ptr;
}


// ProtectShadowPage - Protects shadow page that begins at beginAddr, spanning
//                     over PageNum
void
ProtectShadowPage (void * beginPage, unsigned NumPPages)
{
  kern_return_t kr;
  if (ConfigData.RemapObjects) {
    kr = mprotect(beginPage, NumPPages * PPageSize, PROT_NONE);
    if (kr != KERN_SUCCESS)
      perror(" mprotect error: Failed to protect shadow page\n");
  }
  return;
}

// UnprotectShadowPage - Unprotects the shadow page in the event of fault when
//                       accessing protected shadow page in order to
//                       resume execution
void
UnprotectShadowPage (void * beginPage, unsigned NumPPages)
{
  kern_return_t kr;
  kr = mprotect(beginPage, NumPPages * PPageSize, PROT_READ | PROT_WRITE);
  if (kr != KERN_SUCCESS)
    perror(" unprotect error: Failed to make shadow page accessible \n");
  return;
}

NAMESPACE_SC_END
