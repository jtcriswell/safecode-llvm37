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
#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "poolalloc/MMAPSupport.h"
#include "poolalloc/Support/MallocAllocator.h"
#include <vector>
#include <iostream>

// this is for Mac OS X
#include <mach/mach_vm.h>
#include <mach/mach.h>

// Define this if we want to use memalign instead of mmap to get pages.
// Empirically, this slows down the pool allocator a LOT.
#define USE_MEMALIGN 0
//#define STATISTIC 
unsigned PageSize = 0;
unsigned AddressSpaceUsage1 = 0;
unsigned AddressSpaceUsage2 = 0;
unsigned AddressSpaceUsage3 = 0;
unsigned mmapstart = 0x20000000;
unsigned mmapstart2 = 0x40000000;
unsigned numPages = 8;
int fd;
FILE *f;
void InitializePageManager() {
  if (!PageSize) {
    PageSize = sysconf(_SC_PAGESIZE);
#ifdef FILESTATISTIC    
    f = fopen("/tmp/ftpdl", "a");
#endif    
  }
}

// Explicitly use the malloc allocator here, to avoid depending on the C++
// runtime library.
typedef std::vector<void*, llvm::MallocAllocator<void*> > RemappablePagesListType;

static RemappablePagesListType &getRemappablePageList() {
  static RemappablePagesListType *RemappablePages = 0;

  if (!RemappablePages) {
    // Avoid using operator new!
    RemappablePages = (RemappablePagesListType*)malloc(sizeof(RemappablePagesListType));
    // Use placement new now.
    new (RemappablePages) std::vector<void*, llvm::MallocAllocator<void*> >();
  }
  return *RemappablePages;
}

void logusage() {
  pid_t pid = getpid();
  char sbuf[150];
  bzero(sbuf,150);
  sprintf(sbuf,"/tmp/ftpdlog.%d", pid);
  //    int fd = open(sbuf, O_CREAT|O_APPEND|O_SYNC, 00777);
  // sprintf(sbuf, 
  // write(fd, sbuf, strlen(sbuf));
  //  close(fd);
  if (fprintf(f, "pid %d, Address space usage2 %d, Address space usage1 %d AddressSpaceUsage3 %d\n",pid, AddressSpaceUsage2, AddressSpaceUsage1, AddressSpaceUsage3) < 0) abort();
  fflush(f);
  //    fclose(f);
}

/*
void *RemapPage(void *va) {
  //This is for creating an alias
  //  return va;
  void *nva = mremap(va, 0, PageSize, MREMAP_MAYMOVE);
  return va;
  if (nva == (void *)-1) {
    perror(" mremap error \n");
    printf(" no of pages used %d %d  %d\n", AddressSpaceUsage1, AddressSpaceUsage2, AddressSpaceUsage2+AddressSpaceUsage1);
    abort();
    }
#ifdef FILESTATISTIC
  logusage();
#endif    
#ifdef STATISTIC
  AddressSpaceUsage2++;
#endif    
  return nva;
}
*/

void *RemapPage(void * va){
	kern_return_t		kr;
	mach_vm_address_t 	target_addr=0;
	mach_vm_address_t	source_addr;
	vm_prot_t			prot_cur = VM_PROT_READ | VM_PROT_WRITE;
	vm_prot_t			prot_max = VM_PROT_READ | VM_PROT_WRITE;
	
	source_addr = (mach_vm_address_t) va;
	
	kr = mach_vm_remap (mach_task_self(),
                      &target_addr,
                      PageSize,
                      0,
                      TRUE,
                      mach_task_self(),
                      source_addr,
                      FALSE,
                      &prot_cur,
                      &prot_max,
                      VM_INHERIT_SHARE); 

	if (kr != KERN_SUCCESS) {
		printf(" mremap error: %d \n", kr);
		printf(" no of pages used %d %d  %d\n", AddressSpaceUsage1, AddressSpaceUsage2, AddressSpaceUsage2+AddressSpaceUsage1);
		abort();
	}
	
	va = (void *) target_addr;
	return va;

#ifdef FILESTATISTIC
	logusage();
#endif
#ifdef STATISTIC
	AddressSpaceUsage2++;
#endif
}


// AllocatePage - This function returns a chunk of memory with size and
// alignment specified by PageSize.
void *AllocatePage() {
	RemappablePagesListType &FPL = getRemappablePageList();
	if (!FPL.empty()) {
		void *Result = FPL.back();
		FPL.pop_back();
		void *Ptr = mmap(Result, PageSize, PROT_READ|PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
		AddressSpaceUsage3++;
    
		return Ptr;
	}

	// Allocate a page using mmap shared
	//  int sd = shmget(IPC_PRIVATE, PageSize, 0x1c0|0600);
	// void *Ptr = shmat(sd, 0,0);
	//   shmctl(sd, IPC_RMID, 0);
	void *Ptr = mmap(0, PageSize, PROT_READ|PROT_WRITE, MAP_SHARED |MAP_ANONYMOUS, -1, 0);
	//  void *Ptr = mmap((void *)mmapstart, PageSize, PROT_READ|PROT_WRITE, MAP_SHARED |MAP_ANONYMOUS|MAP_FIXED, -1, 0);
	if (Ptr == (void *)-1 ) {
		perror("couldn't mmap\n");
		printf(" no of pages used %d %d  %d\n", AddressSpaceUsage1, AddressSpaceUsage2, AddressSpaceUsage2+AddressSpaceUsage1);
		abort();
	}
	
#ifdef STATISTIC 
	mmapstart += PageSize;
	AddressSpaceUsage1++;
#endif    
	return Ptr;
}

void *AllocateNPages(unsigned Num) {
  if (Num <= 1) return AllocatePage();
  void *Ptr = mmap(0, PageSize * Num, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0); 
#ifdef STATISTIC
  AddressSpaceUsage1++;
#endif    
  return Ptr;
  
}


// MprotectPage - This function changes the protection status of the page to become
//                 none-accessible
void MprotectPage(void *pa, unsigned numPages) {
	kern_return_t kr;
	kr = mprotect(pa, numPages * PageSize, PROT_NONE);
	if (kr != KERN_SUCCESS)
		perror(" mprotect error \n");
	return;
}

// FreePage - This function returns the specified page to the pagemanager for
// future allocation.
void FreePage(void *Page) {
	munmap(Page, PageSize);
	RemappablePagesListType &FPL = getRemappablePageList();
	FPL.push_back(Page);
}
