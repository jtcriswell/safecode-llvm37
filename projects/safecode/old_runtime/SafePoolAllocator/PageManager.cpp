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

#include <unistd.h>

#include <cassert>
#include <iostream>
#include <vector>
#include <map>
#include <utility>
#include <string.h>

// this is for dangling pointer detection in Mac OS X
#if defined(__APPLE__)
#include <mach/mach_vm.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#endif

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
std::map<void *,std::vector<struct ShadowInfo> > ShadowPages;

// Structure defining configuration data
struct ConfigData ConfigData = {false, true, false};

// Define this if we want to use memalign instead of mmap to get pages.
// Empirically, this slows down the pool allocator a LOT.
#define USE_MEMALIGN 0
extern "C" {
unsigned PageSize = 0;
}

extern unsigned poolmemusage;

// If not compiling on Mac OS X, define types and values to make the same code
// work on multiple platforms.
#if !defined(__APPLE__)
typedef int kern_return_t;
static const unsigned int KERN_SUCCESS=0;
#endif

// Physical page size
unsigned PPageSize;

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

static unsigned logregs = 0;

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

// The set of free memory pages we retrieved from the OS.
typedef std::vector<void*, llvm::MallocAllocator<void*> > FreePagesListType;
static FreePagesListType FreePages;

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

  source_addr = (mach_vm_address_t) ((unsigned long)va & ~(PPageSize - 1));
  unsigned offset = (unsigned long)va & (PPageSize - 1);
  unsigned NumPPage = 0;

  NumPPage = (length / PPageSize) + 1;

  //if((unsigned)va > 0x2f000000) {
  //  logregs = 1;
  //}

  if (logregs) {
    fprintf (stderr, " RemapPage:117: source_addr = 0x%016p, offset = 0x%016x, NumPPage = %d\n", (void*)source_addr, offset, NumPPage);
    fflush(stderr);
  }

#if 0
  // FIX ME!! when there's time, check out why this doesn't work
  if ( (length - (NumPPage-1) * PPageSize) > (PPageSize - offset) ) {
    NumPPage++;
    length = NumPPage * PPageSize;
  }
#endif

  unsigned byteToMap = length + offset;

  if (logregs) {
    fprintf(stderr, " RemapPage127: remapping page of size %d covering %d page with offset %d and byteToMap = %d",
    length, NumPPage, offset, byteToMap);
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
    fprintf(stderr, " failed to remap %dB of memory from source_addr = 0x%08x\n", byteToMap, (unsigned)source_addr);
    //printf(" no of pages used %d %d  %d\n", AddressSpaceUsage1, AddressSpaceUsage2, AddressSpaceUsage2+AddressSpaceUsage1);
    fprintf(stderr, "%s\n", mach_error_string(kr));
    mach_error("mach_vm_remap:",kr); // just to make sure I've got this error right
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
static void *
RemapPages (void * va, unsigned length) {
  void *  target_addr = 0;
  void *  source_addr;
  void *  finish_addr;

  //
  // Find the beginning and end of the physical pages for this memory object.
  //
  source_addr = (void *) ((unsigned long)va & ~(PageSize - 1));
  finish_addr = (void *) (((unsigned long)va + length) & ~(PPageSize - 1));

  unsigned int NumPages = ((uintptr_t)finish_addr - (uintptr_t)source_addr) / PPageSize;
  if (!NumPages) NumPages = 1;

  //
  // Find the length in bytes of the memory we want to remap.
  //
  unsigned map_length = PageSize;

fprintf (stderr, "remap: %x %x -> %x %x\n", va, map_length, source_addr, map_length);
fflush (stderr);
  target_addr = mremap (source_addr, 0, PageSize, MREMAP_MAYMOVE);
  if (target_addr == MAP_FAILED) {
    perror ("RemapPage: Failed to create shadow page: ");
  }

#if 0
  volatile unsigned int * p = (unsigned int *) source_addr;
  volatile unsigned int * q = (unsigned int *) target_addr;

  p[0] = 0xbeefbeef;
fprintf (stderr, "value: %x=%x, %x=%x\n", p, p[0], q, q[0]);
  p[0] = 0xdeeddeed;
fprintf (stderr, "value: %x=%x, %x=%x\n", p, p[0], q, q[0]);
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
  unsigned phy_offset = (unsigned long)va & (PPageSize - 1);
  unsigned offset     = (unsigned long)va & (PageSize - 1);

  //
  // Compute the location of the object relative to the page and physical page.
  //
  page_start     = (unsigned char *)((unsigned long)va & ~(PageSize - 1));
  phy_page_start = (unsigned char *)((unsigned long)va & ~(PPageSize - 1));

  unsigned StartPage = ((uintptr_t)phy_page_start >> 12) - ((uintptr_t)page_start >> 12);
  //unsigned EndPage   = ((phy_page_start + length - page_start) / PPageSize) + 1;

  //
  // If we're not remapping objects, don't do anything.
  //
  if (ConfigData.RemapObjects == false)
    return (void *)(phy_page_start);

  // Create a mask to easily tell if the needed pages are available
  unsigned mask = 0;
  for (unsigned i = StartPage; i < PageMultiplier; ++i) {
    if (((unsigned char *)(page_start) + i*PPageSize) <= ((unsigned char *)(va)+length) )
      mask |= (1u << i);
    else
      break;
  }

  //
  // First, look to see if a pre-existing shadow page is available.
  //
  if (ShadowPages.find(page_start) != ShadowPages.end()) {
    unsigned int numfull = 0;
    for (unsigned i = 0; i < NumShadows; ++i) {
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
    std::vector<struct ShadowInfo> Shadows(NumShadows);
    for (unsigned i = 0; i != NumToAllocate; ++i) {
      char * PagePtr = Ptr+i*PageSize;
      for (unsigned j=0; j < NumShadows; ++j) {
        Shadows[j].ShadowStart = NewShadows[j]+(i*PageSize);
        Shadows[j].InUse       = 0;
      }
      ShadowPages.insert(std::make_pair((void *)PagePtr,Shadows));
    }
  }

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
  FreePagesListType &FPL = FreePages;
  FPL.push_back(Page);
  munmap(Page, 1);
  /*
  if (FPL.size() >  THRESHOLD) {
    //    printf( "pool allocator : reached a threshold \n");
    //    exit(-1); 
    munmap(Page, PageSize);
    poolmemusage -= PageSize;
  }
  */
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

