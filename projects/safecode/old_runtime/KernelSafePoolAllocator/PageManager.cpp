//===- PageManager.cpp - Implementation of the page allocator -------------===//
// 
//                       The LLVM Compiler Infrastructure
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file implements the PageManager.h interface.
//
//===----------------------------------------------------------------------===//

#include "PageManager.h"
#ifndef _POSIX_MAPPED_FILES
#define _POSIX_MAPPED_FILES
#endif
#include <unistd.h>
#include <stdio.h>
#include "poolalloc/Support/MallocAllocator.h"
#include "poolalloc/MMAPSupport.h"
#include <iostream>
#include <vector>
#include <cassert>
// Define this if we want to use memalign instead of mmap to get pages.
// Empirically, this slows down the pool allocator a LOT.
#define DEBUG(x) 
#define USE_MEMALIGN 0
extern "C" {
unsigned PageSize = 0;
}
FILE *fd1; 
void InitializePageManager() {
  if (!PageSize) {
    //    fd1 =  fopen("/tmp/ftpdlog", "a");
    //    sleep(10);
    PageSize = sysconf(_SC_PAGESIZE) ;
  }
}

#if !USE_MEMALIGN
static void *GetPages(unsigned NumPages) {
#if defined(i386) || defined(__i386__) || defined(__x86__)
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
  //                 MAP_SHARED|MAP_ANONYMOUS, fdq, 0);
  //  void *pa = malloc(NumPages * PageSize);
  //  assert(Addr != MAP_FAILED && "MMAP FAILED!");
#if POSIX_MEMALIGN
  if (posix_memalign(&Addr, PageSize, NumPages*PageSize) != 0){
    assert(0 && "memalign failed \n");
  }
#else
  if ((Addr = valloc (NumPages*PageSize)) == 0){
    assert(0 && "valloc failed \n");
  }
#endif
  DEBUG(printf("memalign returns %x for %d\n", Addr, NumPages);)
  //  fprintf(fd1, "memory usage statistic :%d  %d\n", getpid(), poolmemusage);
  //  fflush(fd1);
  return Addr;
}
#endif

// Explicitly use the malloc allocator here, to avoid depending on the C++
// runtime library.
typedef std::vector<void*, llvm::MallocAllocator<void*> > FreePagesListType;

static FreePagesListType &getFreePageList() {
  static FreePagesListType *FreePages = 0;

  if (!FreePages) {
    // Avoid using operator new!
    FreePages = (FreePagesListType*)malloc(sizeof(FreePagesListType));
    // Use placement new now.
    //    new (FreePages) std::vector<void*, llvm::MallocAllocator<void*> >();
  }
  return *FreePages;
}

/// AllocatePage - This function returns a chunk of memory with size and
/// alignment specified by PageSize.
void *AllocatePage() {
  //  return GetPages(1);
  FreePagesListType &FPL = getFreePageList();

  if (!FPL.empty()) {
    void *Result = FPL.back();
      FPL.pop_back();
      return Result;
  }

  // Allocate several pages, and put the extras on the freelist...
  unsigned NumToAllocate = 1;
  char *Ptr = (char*)GetPages(NumToAllocate);
  /*
  for (unsigned i = 1; i != NumToAllocate; ++i)
    FPL.push_back(Ptr+i*PageSize);
  */
  return Ptr;
}

void *AllocateNPages(unsigned Num) {
  if (Num <= 1) return AllocatePage();
  return GetPages(Num);
}

/// FreePage - This function returns the specified page to the pagemanager for
/// future allocation.
#define THRESHOLD 5
void FreePage(void *Page) {
  FreePagesListType &FPL = getFreePageList();
  FPL.push_back(Page);
  //munmap(Page, 1);
  /*
  if (FPL.size() >  THRESHOLD) {
    //    printf( "pool allocator : reached a threshold \n");
    //    exit(-1); 
    munmap(Page, PageSize);
    poolmemusage -= PageSize;
  }
  */
}
