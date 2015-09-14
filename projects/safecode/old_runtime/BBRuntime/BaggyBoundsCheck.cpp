//===- BaggyBoundsCheck.cpp - Implementation of poolallocator runtime -===//
//
//                          The SAFECode Compiler
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file is one possible implementation of the LLVM pool allocator runtime
// library.
//
// Uses Baggy Bounds Checking 
//
//===----------------------------------------------------------------------===//
// NOTES:
//  1) Some of the bounds checking code may appear strange.  The reason is that
//     it is manually inlined to squeeze out some more performance.  Please
//     don't change it.
//
//===----------------------------------------------------------------------===//

#include "ConfigData.h"
#include "DebugReport.h"
#include "safecode/Runtime/BBRuntime.h"

#include <cstring>
#include <cassert>
#include <cstdio>
#include <cstdarg>

// This must be defined for Snow Leopard to get the ucontext definitions
#if defined(__APPLE__)
#define _XOPEN_SOURCE 1
#endif

#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <ucontext.h>
#include <sys/mman.h>

#define TAG unsigned tag

#define DEBUG(x)

NAMESPACE_SC_BEGIN

struct ConfigData ConfigData;

NAMESPACE_SC_END

using namespace NAMESPACE_SC;

/// UNUSED in production version
FILE * ReportLog;

// Configuration for C code; flags that we should stop on the first error
unsigned StopOnError;

// signal handler
static void bus_error_handler(int, siginfo_t *, void *);

unsigned SLOT_SIZE = 4;
unsigned WORD_SIZE = 64;
unsigned char * __baggybounds_size_table_begin; 
const size_t table_size = 1L << 43;


//===----------------------------------------------------------------------===//
//
//  Baggy Bounds Pool allocator library implementation
//
//===----------------------------------------------------------------------===//

void *
__sc_bb_poolinit(DebugPoolTy *Pool, unsigned NodeSize, unsigned) {
  return Pool;
}

void 
__sc_bb_pooldestroy(DebugPoolTy *Pool) {
  return;
}

//
// Function: pool_init_runtime
//
// Description: 
//   This function is called to initialize the entire SAFECode run-time. It 
//   configures the various run-time options for SAFECode and performs other
//   initialization tasks.
//
// Inputs:
//  Dangling   - Set to non-zero to enable dangling pointer detection
//  RewriteOOB - Set to non-zero to enable Out-Of-Bounds pointer rewriting.
//  Terminate  - Set to non-zero to have SAFECode terminate when an error 
//               occurs. 
//

void 
pool_init_runtime(unsigned Dangling, unsigned RewriteOOB, unsigned Terminate) {
  //
  // Initialize the signal handlers for catching errors.
  //
  ConfigData.RemapObjects = Dangling;
  ConfigData.StrictIndexing = !(RewriteOOB);
  StopOnError = Terminate;

  //
  // Allocate a range of memory for rewrite pointers.
  //

  //
  // Leave initialization of the Report logfile to the reporting routines.
  // The libc stdio functions may have not been initialized by this point, so
  // we cannot rely upon them working.
  //
  ReportLog = stderr;

  //
  // TODO:Install hooks for catching allocations outside the scope of SAFECode.
  //
  /*if (ConfigData.TrackExternalMallocs) {
    installAllocHooks();
    } */ 
  //
  // Initialize the signal handlers for catching errors.
  //
  struct sigaction sa;
  bzero (&sa, sizeof (struct sigaction));
  sa.sa_sigaction = bus_error_handler;
  sa.sa_flags = SA_SIGINFO;
  if (sigaction(SIGBUS, &sa, NULL) == -1) {
    fprintf (stderr, "sigaction installer failed!");
    fflush (stderr);
  }
  if (sigaction(SIGSEGV, &sa, NULL) == -1) {
    fprintf (stderr, "sigaction installer failed!");
    fflush (stderr);
  }


  // Initialize the baggy bounds table
  __baggybounds_size_table_begin = NULL;
  __baggybounds_size_table_begin = 
    (unsigned char*) mmap(0, table_size, 
                          PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON|MAP_NORESERVE, 
                          -1, 0);

  if (__baggybounds_size_table_begin == MAP_FAILED) {
    fprintf (stderr, "Baggy Bounds Table initialization failed!");
    fflush (stderr);
    assert(0 && "Table Init Failed");
  }

  return;
}

void
__internal_register(void *allocaptr, unsigned NumBytes) {
  uintptr_t Source = (uintptr_t)allocaptr;
  unsigned char size= 0;
  while((unsigned)(1<<size) < NumBytes) {
    size++;
  }
  size = (size < SLOT_SIZE) ? SLOT_SIZE : size;
  uintptr_t Source1 = Source & ~((1<<size)-1);
  if(Source1 != Source) {
    printf("%p, %p, %u Not aligned\n", (void*)Source, (void*)Source1, NumBytes);
    assert(0 && "Memory objects not aligned");
  }
  Source = Source & ~((1<<size)-1);
  unsigned long index = Source >> SLOT_SIZE;
  unsigned range = 1 << (size - SLOT_SIZE);

  memset(__baggybounds_size_table_begin + index, size, range);
  return;
}

//
// Function: sc_bb_poolargvregister()
//
// Description:
//  Register all of the argv strings in the external object pool.
//
void * 
__sc_bb_poolargvregister(int argc, char **argv) {
  char ** argv_temp = 
    (char **)__sc_bb_src_poolalloc(NULL,(sizeof(char*)*(argc+1)),0,"main\n", 0);

  for (int index=0; index < argc; ++index) {
    char *argv_index_temp = 
      (char *)__sc_bb_src_poolalloc(NULL,(strlen(argv[index])+ 1)*sizeof(char),0,"main\n", 0);
    argv_index_temp = strcpy(argv_index_temp,  argv[index]);

    __internal_register(argv_index_temp,(strlen (argv[index]) + 1)*sizeof(char));
    argv_temp[index] = argv_index_temp;
  }
  argv_temp[argc] = NULL;

  //
  // Register the actual argv array as well.  Note that the transform can
  // do this, but it's easier to implement it here, and I doubt accessing argv
  // strings is performance critical.
  //
  // Note that the argv array is supposed to end with a NULL pointer element.
  //
  __internal_register(argv_temp, sizeof(char*)*(argc+1) );

  return (void*)argv_temp;
}

//
// Function: __sc_bb_src_poolregister()
//
// Description:
//  This function is externally visible and is called by code to register
//  a heap allocation.
//
void 
__sc_bb_src_poolregister (DebugPoolTy *Pool,
                          void * allocaptr, 
                          unsigned NumBytes, TAG,
                          const char* SourceFilep, 
                          unsigned lineno) {

  __internal_register(allocaptr, NumBytes);
  return;
}

//
// Function: __sc_bb_src_poolregister_stack()
//
// Description:
//  This function is externally visible and is called by code to register
//  a stack allocation.
//
void 
__sc_bb_src_poolregister_stack (DebugPoolTy *Pool,
                                void * allocaptr, 
                                unsigned NumBytes, TAG,
                                const char* SourceFilep, 
                                unsigned lineno) {
  __internal_register(allocaptr, NumBytes);
  return;
}

//
// Function: __sc_bb_poolregister_stack()
//
// Description:
//  This function is externally visible and is called by code to register
//  a stack allocation without debug information.
//
void 
__sc_bb_poolregister_stack (DebugPoolTy *Pool,
                            void * allocaptr, 
                            unsigned NumBytes) {
  __sc_bb_src_poolregister_stack(Pool, allocaptr, NumBytes, 0, "<unknown>", 0);
  return;
}

//
// Function: __sc_bb_src_poolregister_global()
//
// Description:
//  This function is externally visible and is called by code to register
//  a global variable.
//
void 
__sc_bb_poolregister_global (DebugPoolTy *Pool,
                             void *allocaptr, 
                             unsigned NumBytes) {
  __sc_bb_src_poolregister_global_debug(Pool, 
                                        allocaptr, NumBytes, 0 , "<unknown>", 0);
  return;
}

//
// Function: __sc_bb_src_poolregister_global_debug()
//
// Description:
//  This function is externally visible and is called by code to register
//  a global variable with debugging information attached.
//
void 
__sc_bb_src_poolregister_global_debug (DebugPoolTy *Pool,
                                       void *allocaptr, 
                                       unsigned NumBytes,TAG, 
                                       const char *SourceFilep,
                                       unsigned lineno) {
  __internal_register(allocaptr, NumBytes);
}

//
// Function: __sc_bb_poolregister()
//
// Description:
//  Register the memory starting at the specified pointer of the specified size
//  with the given Pool.  This version will also record debug information about
//  the object being registered.
//
void 
__sc_bb_poolregister(DebugPoolTy *Pool, 
                     void *allocaptr, 
                     unsigned NumBytes) {
  __sc_bb_src_poolregister(Pool, allocaptr, NumBytes, 0, "<unknown>", 0);
}

void
__sc_bb_poolunregister(DebugPoolTy *Pool, void *allocaptr) {
  __sc_bb_poolunregister_debug(Pool, allocaptr, 0, "<unknown>", 0); 
}

void
__sc_bb_poolunregister_debug (DebugPoolTy *Pool, 
                              void *allocaptr,
                              TAG,
                              const char* SourceFilep,
                              unsigned lineno) {
  uintptr_t Source = (uintptr_t)allocaptr;
  unsigned  e;
  e = __baggybounds_size_table_begin[Source >> SLOT_SIZE];
  if(e == 0 ) {
    return;
  }
  uintptr_t size = 1 << e;
  uintptr_t base = Source & ~(size -1);
  unsigned long index = base >> SLOT_SIZE;
  unsigned int slots = 1<<(e - SLOT_SIZE);

  memset(__baggybounds_size_table_begin + index, 0, slots);
}

void
__sc_bb_poolunregister_stack(DebugPoolTy *Pool, 
                             void *allocaptr) {
  __sc_bb_poolunregister_stack_debug(Pool, allocaptr, 0, "<unknown>", 0); 
}

void
__sc_bb_poolunregister_stack_debug (DebugPoolTy *Pool, 
                                    void *allocaptr,
                                    TAG,
                                    const char* SourceFilep,
                                    unsigned lineno) {
  uintptr_t Source = (uintptr_t)allocaptr;

  unsigned  e;
  e = __baggybounds_size_table_begin[Source >> SLOT_SIZE];
  if(e == 0 ) {
    return;
  }
  uintptr_t size = 1 << e;
  uintptr_t base = Source & ~(size -1);
  unsigned long index = base >> SLOT_SIZE;
  unsigned int slots = 1<<(e - SLOT_SIZE);
  memset(__baggybounds_size_table_begin + index, 0, slots);
}

void *
__sc_bb_src_poolalloc(DebugPoolTy *Pool,
                      unsigned NumBytes, TAG, 
                      const char * SourceFilep,
                      unsigned lineno) {
  unsigned char size= 0;
  while((unsigned)(1<<size) < NumBytes) {
    size++;
  }
  if (size < SLOT_SIZE)
    size = SLOT_SIZE;
  unsigned int alloc = 1 << size;
  void *p;
  assert(!posix_memalign(&p, alloc, alloc) && "Memory allocation failed");

  return p;
}

void*
__sc_bb_poolmemalign(DebugPoolTy *Pool, 
                     unsigned Alignment, 
                     unsigned NumBytes) {

  unsigned char size= 0;
  while((unsigned)(1<<size) < NumBytes) {
    size++;
  }
  if (size < SLOT_SIZE)
    size = SLOT_SIZE;
  if (size < Alignment)
    size = Alignment;
  unsigned int alloc = 1 << size;
  void *p;

  assert(!posix_memalign(&p, alloc, alloc) && "Memory allocation failed");
  __sc_bb_poolregister(Pool, p, NumBytes);
  return p;
}

void * 
__sc_bb_src_poolcalloc(DebugPoolTy *Pool, 
                       unsigned Number, 
                       unsigned NumBytes, TAG, 
                       const char* SourceFilep, 
                       unsigned lineno) {

  unsigned char size= 0;
  while((unsigned)(1<<size) < (NumBytes*Number)) {
    size++;
  }
  if (size < SLOT_SIZE) size = SLOT_SIZE;
  unsigned int alloc = 1<< size;
  void *p;
  assert(!posix_memalign(&p, alloc, alloc) && "Memory allocation failed");
  __sc_bb_src_poolregister(Pool, p, (Number*NumBytes), tag, SourceFilep, lineno);
  if (p) {
    bzero(p, Number*NumBytes);
  }
  return p;
}

void *
__sc_bb_poolcalloc(DebugPoolTy *Pool,
                   unsigned Number, 
                   unsigned NumBytes, TAG) {
  return __sc_bb_src_poolcalloc(Pool,Number,  NumBytes, 0, "<unknown>",0); 
}

void *
__sc_bb_poolrealloc_debug (DebugPoolTy *Pool,
                           void *Node,
                           unsigned NumBytes, TAG,
                           const char * SourceFilep,
                           unsigned lineno) {
  return __sc_bb_poolrealloc(Pool, Node, NumBytes);
}
void *
__sc_bb_poolrealloc(DebugPoolTy *Pool,
                    void *Node, 
                    unsigned NumBytes) {
  if (Node == 0) {
    void *New = __sc_bb_poolalloc(Pool, NumBytes);
    __sc_bb_poolregister(Pool, New, NumBytes);
    return New;
  }

  if (NumBytes == 0) {
    __sc_bb_poolunregister(Pool, Node);
    __sc_bb_poolfree(Pool, Node);
    return 0;
  }

  uintptr_t Source = (uintptr_t)Node; 
  if (Source & SET_MASK) {
    return 0;
  }

  void *New = __sc_bb_poolalloc(Pool, NumBytes);
  if(New == 0)
    return 0;
  __sc_bb_poolregister(Pool, New, NumBytes);

  unsigned  char e = __baggybounds_size_table_begin[Source >> SLOT_SIZE];
  unsigned int size_old = 1 << e;
  uintptr_t Source_new = (uintptr_t)New; 
  unsigned  char e_new = __baggybounds_size_table_begin[Source_new >> SLOT_SIZE];
  unsigned int size_new = 1 << e_new;

  if(size_new > size_old)
    memcpy(New, Node, size_old);
  else 
    memcpy(New, Node, size_new);

  __sc_bb_poolunregister(Pool, Node);
  __sc_bb_poolfree(Pool, Node);
  return New;
}

void *
__sc_bb_poolalloc(DebugPoolTy *Pool,
                  unsigned NumBytes) {
  return __sc_bb_src_poolalloc(Pool, NumBytes, 0, "<unknown>",0); 
}

void
__sc_bb_src_poolfree (DebugPoolTy *Pool,
                      void *Node,TAG,
                      const char* SourceFile, 
                      unsigned lineno) {
  free(Node);
}	

void
__sc_bb_poolfree (DebugPoolTy *Pool,
                  void *Node) {
  __sc_bb_src_poolfree(Pool, Node, 0, "<unknown>", 0); 
}


//
// Function: getProgramCounter()
//
// Description:
//  This function determines the program counter at which a fault was taken.
//
// Inputs:
//  context - A pointer to the context in which the fault occurred.  This is
//            a paramter that is passed into signal handlers.
//
// Return value:
//  0  - The program counter could not be determined on this platform.
//  ~0 - Otherwise, the program counter at which the fault occurred is
//       returned.
//
static unsigned
  getProgramCounter (void * context) {
#if defined(__APPLE__)
#if defined(i386) || defined(__i386__) || defined(__x86__)
    // Cast parameters to the desired type
    ucontext_t * mycontext = (ucontext_t *) context;
    return (mycontext->uc_mcontext->__ss.__eip);
#endif
#endif

#if defined(__linux__)
    // Cast parameters to the desired type
    ucontext_t * mycontext = (ucontext_t *) context;
    return (mycontext->uc_mcontext.gregs[14]);
#endif

    return 0;
  }

//
//
// Function: bus_error_handler()
//
// Description:
//  This is the signal handler that catches bad memory references.
//

static void
bus_error_handler (int sig, siginfo_t * info, void * context) {

  //
  // Disable the signal handler for now.  If this function does something
  // wrong, we want the bus error to terminate the program.
  //
  signal(SIGBUS, NULL);

  //
  // Get the program counter for where the fault occurred.
  //
  unsigned program_counter = getProgramCounter (context);

  //
  // Get the address causing the fault.
  //
  void * faultAddr = info->si_addr; 

  //
  // This is not a dangling pointer, uninitialized pointer, or a rewrite
  // pointer.  This is some load/store that has obviously gone wrong (even
  // if we consider the possibility of incompletenes).  Report it as a
  // load/store error.
  //
  DebugViolationInfo v;
  v.type = ViolationInfo::FAULT_LOAD_STORE,
    v.faultPC = (const void*)program_counter,
    v.faultPtr = faultAddr,
    v.SourceFile = 0,
    v.lineNo = 0;

  ReportMemoryViolation(&v);

  //
  // Reinstall the signal handler for subsequent faults
  //
  struct sigaction sa;
  sa.sa_sigaction = bus_error_handler;
  sa.sa_flags = SA_SIGINFO;
  if (sigaction(SIGBUS, &sa, NULL) == -1)
    printf("sigaction installer failed!");
  if (sigaction(SIGSEGV, &sa, NULL) == -1)
    printf("sigaction installer failed!");

  return;

}

