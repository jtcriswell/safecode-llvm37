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

#include "../include/PageManager.h"

#ifndef _POSIX_MAPPED_FILES
#define _POSIX_MAPPED_FILES
#endif
#include "../include/MallocAllocator.h"
#include "../include/MMAPSupport.h"

#include <unistd.h>

#include <cassert>
#include <cstdio>
#include <iostream>
#include <vector>
#include <map>
#include <utility>
#include <cstring>

#include <errno.h>

namespace llvm {

FreePagesListType FreePages;

// Define this if we want to use memalign instead of mmap to get pages.
// Empirically, this slows down the pool allocator a LOT.
#define USE_MEMALIGN 0
extern "C" {
uintptr_t PageSize = 0;
}

static unsigned poolmemusage = 0;

// Physical page size
uintptr_t PPageSize;

//
// Function: InitializePageManager()
//
// Description:
//  Perform nececessary initialization of the page manager code.  This must be
//  called before any other function in this file is called.
//
void
InitializePageManager() {
  //
  // Determine the physical page size.
  //
  if (!PPageSize) PPageSize = sysconf(_SC_PAGESIZE);

  //
  // Calculate the page size used by the run-time (which is a multiple of the
  // machine's physical page size).
  //
  if (!PageSize) PageSize =  PageMultiplier * PPageSize;
}

#if !USE_MEMALIGN
void *GetPages(unsigned NumPages) {
#if defined(i386) || defined(__i386__) || defined(__x86__) || defined(__x86_64__)
  /* Linux and *BSD tend to have these flags named differently. */
#if defined(MAP_ANON) && !defined(MAP_ANONYMOUS)
# define MAP_ANONYMOUS MAP_ANON
#endif /* defined(MAP_ANON) && !defined(MAP_ANONYMOUS) */
#elif defined(sparc) || defined(__sparc__) || defined(__sparcv9)
  /* nothing */
#elif defined(__APPLE__)
  /* On MacOS X, just use valloc */
#else
  std::cerr << "This architecture is not supported by the pool allocator!\n";
  abort();
#endif

#if defined(__linux__)
#define fd 0
#else
#define fd -1
#endif
  void *Addr;
  //MMAP DOESNOT WORK !!!!!!!!
  //  Addr = mmap(0, NumPages*PageSize, PROT_READ|PROT_WRITE,
  //                 MAP_SHARED|MAP_ANONYMOUS, fd, 0);
  //  void *pa = malloc(NumPages * PageSize);
  //  assert(Addr != MAP_FAILED && "MMAP FAILED!");
#if defined(__linux__)
  Addr = mmap(0, NumPages * PageSize, PROT_READ|PROT_WRITE,
                                      MAP_SHARED |MAP_ANONYMOUS, -1, 0);
  if (Addr == MAP_FAILED) {
     perror ("mmap:");
     fflush (stdout);
     fflush (stderr);
     assert(0 && "valloc failed\n");
  }
#else
#if POSIX_MEMALIGN
   if (posix_memalign(&Addr, PageSize, NumPages*PageSize) != 0){
     assert(0 && "memalign failed \n");
   }
#else
   if ((Addr = valloc (NumPages*PageSize)) == 0){
     perror ("valloc:");
     fflush (stdout);
     fflush (stderr);
     assert(0 && "valloc failed \n");
   } else {
#if 0
    fprintf (stderr, "valloc: Allocated %x\n", NumPages);
    fflush (stderr);
#endif
   }
#endif
#endif
  poolmemusage += NumPages * PageSize;

  // Initialize the page to contain safe inital values
  memset(Addr, initvalue, NumPages *PageSize);

  return Addr;
}
#endif


/// AllocatePage - This function returns a chunk of memory with size and
/// alignment specified by PageSize.
__attribute__((weak)) void * AllocatePage() {

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

  return Ptr;
}

void *AllocateNPages(unsigned Num) {
  if (Num <= 1) return AllocatePage();
  return GetPages(Num);
}

/// FreePage - This function returns the specified page to the pagemanager for
/// future allocation.
void FreePage(void *Page) {
  FreePagesListType &FPL = FreePages;
  FPL.push_back(Page);
}

}
