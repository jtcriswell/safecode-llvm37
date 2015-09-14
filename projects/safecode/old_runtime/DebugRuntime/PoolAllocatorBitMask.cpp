//===- PoolAllocatorBitMask.cpp - Implementation of poolallocator runtime -===//
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
// This uses the 'Ptr1' field to maintain a linked list of slabs that are either
// empty or are partially allocated from.  The 'Ptr2' field of the PoolTy is
// used to track a linked list of slabs which are full, ie, all elements have
// been allocated from them.
//
//===----------------------------------------------------------------------===//
// NOTES:
//  1) Some of the bounds checking code may appear strange.  The reason is that
//     it is manually inlined to squeeze out some more performance.  Please
//     don't change it.
//
//  2) This run-time performs MMU re-mapping of pages to perform dangling
//     pointer detection.  A "shadow" address is the address of a memory block
//     that has been remapped to a new virtal address; the shadow address is
//     returned to the caller on allocation and is unmapped on deallocation.
//     A "canonical" address is the virtual address of memory as it is mapped
//     in the pool slabs; the canonical address is remapped to different shadow
//     addresses each time that particular piece of memory is allocated.
//
//     In normal operation, the shadow address and canonical address are
//     identical.
//
//===----------------------------------------------------------------------===//

#include "ConfigData.h"
#include "PoolAllocator.h"
#include "PageManager.h"
#include "DebugReport.h"
#include "RewritePtr.h"

#include "safecode/Runtime/DebugRuntime.h"

#include <cstring>

// This must be defined for Snow Leopard to get the ucontext definitions
#if defined(__APPLE__)
#define _XOPEN_SOURCE 1
#endif

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <signal.h>
#include <ucontext.h>
#include <sys/mman.h>
#if 0
#include <sys/ucontext.h>
#endif

#include <pthread.h>

#define TAG unsigned tag

#define DEBUG(x)

NAMESPACE_SC_BEGIN

// Dummy pool for holding global memory object information
DebugPoolTy dummyPool;

// Structure defining configuration data
struct ConfigData ConfigData = {false, true, false};

// Invalid address range
#if !defined(__linux__)
uintptr_t InvalidUpper = 0x00000000;
uintptr_t InvalidLower = 0x00000003;
#endif

// Splay tree for mapping shadow pointers to canonical pointers
static RangeSplayMap<void *> ShadowMap;

NAMESPACE_SC_END

using namespace NAMESPACE_SC;

// Map between call site tags and allocation sequence numbers
std::map<unsigned,unsigned> allocSeqMap;
std::map<unsigned,unsigned> freeSeqMap;

/// UNUSED in production version
FILE * ReportLog = 0;

// Configuration for C code; flags that we should stop on the first error
unsigned StopOnError = 0;

// signal handler
static void bus_error_handler(int, siginfo_t *, void *);

// creates a new PtrMetaData structure to record pointer information
static void * getCanonicalPtr (void * ShadowPtr);
static inline void updatePtrMetaData(PDebugMetaData, unsigned, void *,
                                     void *,
                                     unsigned);
static PDebugMetaData createPtrMetaData (unsigned,
                                         unsigned,
                                         allocType,
                                         void *,
                                         void *,
                                         void *,
                                         char * SourceFile = "<unknown>",
                                         unsigned lineno = 0);

//===----------------------------------------------------------------------===//
//
//  Pool allocator library implementation
//
//===----------------------------------------------------------------------===//


//
// Function: pool_init_runtime()
//
// Description:
//  This function is called to initialize the entire SAFECode run-time.  It
//  configures the various run-time options for SAFECode and performs other
//  initialization tasks.
//
// Inputs:
//  Dangling   - Set to non-zero to enable dangling pointer detection.
//  RewriteOOB - Set to non-zero to enable Out-Of-Bounds pointer rewriting.
//  Termiante  - Set to non-zero to have SAFECode terminate when an error
//               occurs.
//

extern "C" void __poolalloc_init();
void
pool_init_runtime (unsigned Dangling, unsigned RewriteOOB, unsigned Terminate) {
  // logregs=1;
  //
  // Initialize the signal handlers for catching errors.
  //
  ConfigData.RemapObjects = Dangling;
  ConfigData.StrictIndexing = !(RewriteOOB);
  StopOnError = Terminate;

  //
  // Allocate a range of memory for rewrite pointers.
  //
#if !defined(__linux__)
  const unsigned invalidsize = 1 * 1024 * 1024 * 1024;
  void * Addr = mmap (0, invalidsize, 0, MAP_SHARED | MAP_ANON, -1, 0);
  if (Addr == MAP_FAILED) {
     perror ("mmap:");
     fflush (stdout);
     fflush (stderr);
     assert(0 && "valloc failed\n");
  }
  //memset (Addr, 0x00, invalidsize);
  madvise (Addr, invalidsize, MADV_FREE);
  InvalidLower = (uintptr_t) Addr;
  InvalidUpper = (uintptr_t) Addr + invalidsize;

  if (logregs) {
    fprintf (stderr, "OOB Area: %p - %p\n", (void *) InvalidLower,
                                            (void *) InvalidUpper);
    fflush (stderr);
  }
#endif

  //
  // Leave initialization of the Report logfile to the reporting routines.
  // The libc stdio functions may have not been initialized by this point, so
  // we cannot rely upon them working.
  //
  ReportLog = stderr;

  //
  // Install hooks for catching allocations outside the scope of SAFECode.
  //
  if (ConfigData.TrackExternalMallocs) {
    installAllocHooks();
  }

  //
  // Initialize the dummy pool.
  //
  __sc_dbg_poolinit(&dummyPool, 1, 0);

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

  // Initialize all global pools
  __poolalloc_init();
  return;
}

//
// Function: __sc_dbg_newpool()
//
// Description:
//  Retuen a pool descriptor for a new pool.
//
void *
__sc_dbg_newpool(unsigned NodeSize) {
  DebugPoolTy * Pool = new DebugPoolTy();
  __pa_bitmap_poolinit(static_cast<BitmapPoolTy*>(Pool), NodeSize);
  return Pool;
}

//
// Function: pooldestroy()
//
// Description:
//  Release all memory allocated for a pool.  The compiler inserts a call to
//  this function when it knows that all objects within the specified pool are
//  unreachable and can be safely deallocated.
//
// FIXME: determine how to adjust debug logs when 
//        pooldestroy is called
void
__sc_dbg_pooldestroy(DebugPoolTy * Pool) {
  assert(Pool && "Null pool pointer passed in to pooldestroy!\n");

  //
  // Deallocate all object meta-data stored in the pool.
  //
  Pool->Objects.clear();
  Pool->OOB.clear();
  Pool->DPTree.clear();

  //
  // Let the pool allocator run-time free all objects allocated within the
  // pool.
  //
  __pa_bitmap_pooldestroy(Pool);
}

//
// Function: poolargvregister()
//
// Description:
//  Register all of the argv strings in the external object pool.
//
void *
__sc_dbg_poolargvregister (int argc, char ** argv) {
  if (logregs) {
    fprintf (stderr, "poolargvregister: %p - %p\n", (void *) argv,
             (void *) (((unsigned char *)(&(argv[argc+1]))) - 1));
    fflush (stderr);
  }
  for (int index=0; index < argc; ++index) {
    if (logregs) {
      fprintf (stderr, "poolargvregister: %p %u: %s\n", argv[index],
               (unsigned)strlen(argv[index]), argv[index]);
      fflush (stderr);
    }
    ExternalObjects.insert(argv[index], argv[index] + strlen (argv[index]));
  }

  //
  // Register the actual argv array as well.  Note that the transform can
  // do this, but it's easier to implement it here, and I doubt accessing argv
  // strings is performance critical.
  //
  // Note that the argv array is supposed to end with a NULL pointer element.
  //
  ExternalObjects.insert(argv, ((unsigned char *)(&(argv[argc+1]))) - 1);

  //
  // Register errno for kicks and gibbles.
  //
  unsigned char * errnoAdd = (unsigned char *) &errno;
  ExternalObjects.insert(errnoAdd, errnoAdd + sizeof (errno) - 1);

  return argv;
}

//
// Function: poolregister_debug()
//
// Description:
//  Register the memory starting at the specified pointer of the specified size
//  with the given Pool.  This version will also record debug information about
//  the object being registered.
//
//  This function is internal to the code and handles the different types of
//  object registrations.
//
static inline void
_internal_poolregister (DebugPoolTy *Pool,
                        void * allocaptr,
                        unsigned NumBytes, TAG,
                        const char * SourceFilep,
                        unsigned lineno,
                        allocType allocationType) {
  // Do some initial casting for type goodness
  char * SourceFile = (char *)(SourceFilep);

  //
  // Print debug information about what object the caller is trying to
  // register.
  //
  if (logregs) {
    const char * p = 0;
    switch (allocationType) {
      case Heap:
        p = "Heap";
        break;
      case Stack:
        p = "Stack";
        break;
      case Global:
        p = "Global";
        break;
      default:
        break;
    }

    fprintf (ReportLog, "poolreg_debug(%d): %p: %p-%p: %d %d %s %d: %s\n", tag,
             (void*) Pool, (void*)allocaptr,
             ((char*)(allocaptr)) + NumBytes - 1, NumBytes, tag, SourceFile, lineno, p);
    fflush (ReportLog);
  }

  //
  // If the pool is NULL or the object has zero length, don't do anything.
  //
  assert (NumBytes && "NumBytes must be more than zero!\n");
  if ((!Pool) || (NumBytes == 0)) return;

  //
  // Add the object to the pool's splay of valid objects.
  //
  if (!(Pool->Objects.insert(allocaptr, (char*) allocaptr + NumBytes - 1))) {
    assert (0 && "poolregister failed: Object already registered!\n");
    abort();
  }
}

//
// Function: poolregister()
//
// Description:
//  Register the memory starting at the specified pointer of the specified size
//  with the given Pool.  This version will also record debug information about
//  the object being registered.
//
void
__sc_dbg_poolregister (DebugPoolTy *Pool, void * allocaptr,
                       unsigned NumBytes) {
#if 0
  //
  // If this is a singleton object within a type-known pool, don't add it to
  // the splay tree.
  //
  // FIXME: disabling because the code that does poolcheck for singleton
  // objects, uses SearchForContainingSlab, which assumes it is being called
  // from a poolfree, and does not handle pointers to the middle of an object.
  if (Pool && (NumBytes == Pool->NodeSize))
    return;
#endif

  //
  // Heap allocations of zero size should just be ignored.
  //
  if (!NumBytes)
    return;

  //
  // Use the common registration function.  Mark the allocation as a heap
  // allocation.
  //
  _internal_poolregister (Pool,
                          allocaptr,
                          NumBytes, 0,
                          "<unknown",
                          0,
                          Heap);
  return;
}

//
// Function: __sc_dbg_src_poolregister()
//
// Description:
//  This function is externally visible and is called by code to register
//  a heap allocation.
//
void
__sc_dbg_src_poolregister (DebugPoolTy *Pool,
                           void * allocaptr,
                           unsigned NumBytes, TAG,
                           const char * SourceFilep,
                           unsigned lineno) {
  //
  // Use the common registration function.  Mark the allocation as a heap
  // allocation.  However, only do this if the object is not a singleton object
  // within a type-known pool.
  //

  // FIXME: disabling because the code that does poolcheck for singleton
  // objects, uses SearchForContainingSlab, which assumes it is being called
  // from a poolfree, and does not handle pointers to the middle of an object.
#if 0
  if (Pool && (NumBytes == Pool->NodeSize)) {
    return
  }
#endif

  //
  // Heap allocations of zero size should just be ignored.
  //
  if (!NumBytes)
    return;

  _internal_poolregister (Pool,
                          allocaptr,
                          NumBytes, tag,
                          SourceFilep,
                          lineno,
                          Heap);

  //
  // Generate a generation number for this object registration.  We only do
  // this for heap allocations.
  //
  unsigned allocID = (allocSeqMap[tag] += 1);

  //
  // Create the meta data object containing the debug information for this
  // pointer.
  //
  PDebugMetaData debugmetadataPtr;
  debugmetadataPtr = createPtrMetaData (allocID,
                                        0,
                                        Heap,
                                        __builtin_return_address(0),
                                        0,
                                        getCanonicalPtr(allocaptr),
                                        (char *) SourceFilep, lineno);
  dummyPool.DPTree.insert (allocaptr,
                           (char*) allocaptr + NumBytes - 1,
                           debugmetadataPtr);
}

//
// Function: pool_reregister()
//
// Description:
//  This is pool_register() for realloc() style allocators.  It will unregister
//  the previously existing object (if necessary) and register the newly
//  allocated object.
//
void
__sc_dbg_poolreregister (DebugPoolTy *Pool,
                         void * newptr,
                         void * oldptr,
                         unsigned NumBytes) {
  if (oldptr == NULL) {
    //
    // If the old pointer is NULL, then we know that this is essentially a
    // regular heap allocation; treat it as such.
    //
    __sc_dbg_poolregister (Pool, newptr, NumBytes);
  } else if (NumBytes == 0) {
    //
    // Allocating a buffer of zero bytes is essentially a deallocation of the
    // memory; treat it as such.
    //
    __sc_dbg_poolunregister (Pool, oldptr);
  } else {
    //
    // Otherwise, this is a true reallocation.  Unregister the old memory and
    // register the new memory.
    __sc_dbg_poolunregister (Pool, oldptr);
    __sc_dbg_poolregister(Pool, newptr, NumBytes);
  }

  return;
}

void
__sc_dbg_src_poolreregister (DebugPoolTy *Pool,
                             void * newptr,
                             void * oldptr,
                             unsigned NumBytes,
                             TAG,
                             const char * SourceFilep,
                             unsigned lineno) {
  if (oldptr == NULL) {
    //
    // If the old pointer is NULL, then we know that this is essentially a
    // regular heap allocation; treat it as such.
    //
    __sc_dbg_src_poolregister(Pool, newptr, NumBytes, tag, SourceFilep, lineno);
  } else if (NumBytes == 0) {
    //
    // Allocating a buffer of zero bytes is essentially a deallocation of the
    // memory; treat it as such.
    //
    __sc_dbg_poolunregister_debug (Pool, oldptr, tag, SourceFilep, lineno);
  } else {
    //
    // Otherwise, this is a true reallocation.  Unregister the old memory and
    // register the new memory.
    __sc_dbg_poolunregister_debug (Pool, oldptr, tag, SourceFilep, lineno);
    __sc_dbg_src_poolregister(Pool, newptr, NumBytes, tag, SourceFilep, lineno);
  }

  return;
}

//
// Function: __sc_dbg_src_poolregister_stack()
//
// Description:
//  This function is externally visible and is called by code to register
//  a stack allocation.
//
void
__sc_dbg_src_poolregister_stack (DebugPoolTy *Pool,
                                 void * allocaptr,
                                 unsigned NumBytes, TAG,
                                 const char * SourceFilep,
                                 unsigned lineno) {
  //
  // Use the common registration function.  Mark the allocation as a stack
  // allocation.
  //
  _internal_poolregister (Pool,
                          allocaptr,
                          NumBytes, tag,
                          SourceFilep,
                          lineno,
                          Stack);

  //
  // Create the meta data object containing the debug information for this
  // pointer.
  //
  PDebugMetaData debugmetadataPtr;
  debugmetadataPtr = createPtrMetaData (0,
                                        0,
                                        Stack,
                                        __builtin_return_address(0),
                                        0,
                                        getCanonicalPtr(allocaptr),
                                        (char *) SourceFilep, lineno);
  dummyPool.DPTree.insert (allocaptr,
                           (char*) allocaptr + NumBytes - 1,
                           debugmetadataPtr);

}

//
// Function: __sc_dbg_poolregister_stack()
//
// Description:
//  This function is externally visible and is called by code to register
//  a stack allocation without debug information.
//
void
__sc_dbg_poolregister_stack (DebugPoolTy *Pool,
                             void * allocaptr,
                             unsigned NumBytes) {
  //
  // Use the common registration function.  Mark the allocation as a stack
  // allocation.
  //
  _internal_poolregister (Pool,
                          allocaptr,
                          NumBytes, 0,
                          "<Unknown>",
                          0,
                          Stack);
}

//
// Function: __sc_dbg_src_poolregister_global()
//
// Description:
//  This function is externally visible and is called by code to register
//  a global variable.
//
void
__sc_dbg_poolregister_global (DebugPoolTy *Pool,
                              void * allocaptr,
                              unsigned NumBytes) {
  //
  // Use the common registration function.  Mark the allocation as a stack
  // allocation.
  //
  _internal_poolregister (Pool,
                          allocaptr,
                          NumBytes,
                          0,
                          "<unknown>",
                          0,
                          Global);
}

//
// Function: __sc_dbg_src_poolregister_global_debug()
//
// Description:
//  This function is externally visible and is called by code to register
//  a global variable with debugging information attached.
//
void
__sc_dbg_src_poolregister_global_debug (DebugPoolTy *Pool,
                                        void * allocaptr,
                                        unsigned NumBytes, TAG,
                                        const char * SourceFilep,
                                        unsigned lineno) {
  //
  // Use the common registration function.  Mark the "allocation" as a global
  // object.
  //
  _internal_poolregister (Pool,
                          allocaptr,
                          NumBytes, tag,
                          SourceFilep,
                          lineno,
                          Global);

  //
  // Create the meta data object containing the debug information for this
  // pointer.
  //
  PDebugMetaData debugmetadataPtr;
  debugmetadataPtr = createPtrMetaData (0,
                                        0,
                                        Global,
                                        __builtin_return_address(0),
                                        0,
                                        getCanonicalPtr(allocaptr),
                                        (char *) SourceFilep, lineno);
  dummyPool.DPTree.insert (allocaptr,
                           (char*) allocaptr + NumBytes - 1,
                           debugmetadataPtr);

}

//
// Function: checkForBadFrees()
//
// Description:
//  This function can be called by pool_unregister() functions to determine if
//  an invalid free is being performed.
//
static inline void
checkForBadFrees (DebugPoolTy *Pool,
                  void * allocaptr, allocType Type,
                  unsigned tag,
                  const char * SourceFilep,
                  unsigned lineno) {
  //
  // Increment the ID number for this deallocation.
  //
  unsigned freeID = (freeSeqMap[tag] += 1);

  //
  // Ignore frees of NULL pointers.  These are okay.
  //
  if (allocaptr == NULL)
    return;

  //
  // Retrieve the debug information about the node.  This will include a
  // pointer to the canonical page.
  //
  void * start;
  void * end;
  PDebugMetaData debugmetadataptr = 0;
  bool found = dummyPool.DPTree.find (allocaptr, start, end, debugmetadataptr);

  // Assert that we either didn't find the object or we found the object *and*
  // it has meta-data associated with it.
  assert ((!found || (found && debugmetadataptr)) &&
          "checkForBadFrees: No debugmetadataptr\n");

  //
  // If we cannot find the meta-data for this pointer, then the free is
  // invalid.  Report it as an error and then continue executing if possible.
  //
  if (!found) {
    DebugViolationInfo v;
    v.type = DebugViolationInfo::FAULT_INVALID_FREE,
      v.faultPC = __builtin_return_address(0),
      v.faultPtr = allocaptr;
      v.PoolHandle = Pool;
      v.dbgMetaData = debugmetadataptr;
      v.SourceFile = SourceFilep;
      v.lineNo = lineno;
    ReportMemoryViolation(&v);
    return;
  }

  //
  // Update the debugging metadata information for this object.
  //
  updatePtrMetaData (debugmetadataptr,
                     freeID,
                     __builtin_return_address(0),
                     (void *)SourceFilep,
                     lineno);

  //
  // Determine if we are doing something stupid like deallocating a global
  // or stack-allocated object when we're supposed to be freeing a heap
  // object.  If so, then report an error.
  //
  if (Type == Heap) {
    if (debugmetadataptr->allocationType != Heap) {
        DebugViolationInfo v;
        v.type = ViolationInfo::FAULT_NOTHEAP_FREE,
        v.faultPC = __builtin_return_address(0);
        v.PoolHandle = Pool;
        v.dbgMetaData = debugmetadataptr;
        v.SourceFile = SourceFilep;
        v.lineNo = lineno;
        v.faultPtr = allocaptr;
        ReportMemoryViolation(&v);
    }
  }

  //
  // Determine if we're freeing a pointer that doesn't point to the beginning
  // of an object.  If so, report an error.
  //
  if (allocaptr != start) {
    OutOfBoundsViolation v;
    v.type = ViolationInfo::FAULT_INVALID_FREE,
      v.faultPC = __builtin_return_address(0),
      v.faultPtr = allocaptr,
      v.dbgMetaData = debugmetadataptr,
      v.SourceFile = SourceFilep,
      v.lineNo = lineno,
      v.objStart = start;
      v.objLen =  (intptr_t)end - (intptr_t)start + 1;
    ReportMemoryViolation(&v);
    return;
  }

  //
  // If dangling pointer detection is not enabled, remove the object from the
  // dangling pointer splay tree.  The memory object's virtual address will be
  // reused, and we don't want to match it for subsequently allocated objects.
  //
  // Also, always remove stack objects.  Their virtual addresses are recycled,
  // and so we don't want to try to re-look up their old start and end values.
  //
  if ((Type == Stack) || (!(ConfigData.RemapObjects))) {
    free (debugmetadataptr);
    dummyPool.DPTree.remove (allocaptr);
  }

  return;
}

//
// Function: poolunregister()
//
// Description:
//  Remove the specified object from the set of valid objects in the Pool.
//
// Inputs:
//  Pool      - The pool in which the object should belong.
//  allocaptr - A pointer to the object to remove.  It can be NULL.
//
// Notes:
//  Note that this function currently deallocates debug information about the
//  allocation.  This is safe because this function is only called on stack
//  objects.  This is less-than-ideal because we lose debug information about
//  the allocation of the stack object if it is later dereferenced outside its
//  function (dangling pointer), but it is currently too expensive to keep that
//  much debug information around.
//
// TODO: The above note is no longer correct; this function is called for stack
//       and heap allocated objects.  A parameter flags whether the object is
//       a heap object or a stack/global object.
//
static inline void
_internal_poolunregister (DebugPoolTy *Pool,
                          void * allocaptr, allocType Type,
                          unsigned tag,
                          const char * SourceFilep,
                          unsigned lineno) {
  if (logregs) {
    fprintf (stderr, "pool_unregister: Start: %p: %s %d\n", allocaptr, SourceFilep, lineno);
    fflush (stderr);
  }

  //
  // If no pool was specified, then do nothing.
  //
  if (!Pool) return;

  //
  // For the NULL pointer, we take no action but flag no error.
  //
  if (!allocaptr) return;

  //
  // Remove the object from the pool's splay tree.
  //
  Pool->Objects.remove (allocaptr);

  //
  // Eject the pointer from the pool's cache if necessary.
  //
  if ((Pool->objectCache[0].lower <= allocaptr) &&
      (allocaptr <= Pool->objectCache[0].upper))
    Pool->objectCache[0].lower = Pool->objectCache[0].upper = 0;

  if ((Pool->objectCache[1].lower <= allocaptr) &&
      (allocaptr <= Pool->objectCache[1].upper))
    Pool->objectCache[1].lower = Pool->objectCache[1].upper = 0;

  //
  // Generate some debugging output.
  //
  if (logregs) {
    fprintf (stderr, "pool_unregister: Done: %p: %s %d\n", allocaptr, SourceFilep, lineno);
    fflush (stderr);
  }
}

void
__sc_dbg_poolunregister (DebugPoolTy *Pool, void * allocaptr) {
  _internal_poolunregister (Pool, allocaptr, Heap, 0, "Unknown", 0);
  return;
}

void
__sc_dbg_poolunregister_debug (DebugPoolTy *Pool,
                               void * allocaptr,
                               TAG,
                               const char * SourceFilep,
                               unsigned lineno) {
  checkForBadFrees (Pool, allocaptr, Heap, tag, SourceFilep, lineno);
  _internal_poolunregister (Pool, allocaptr, Heap, tag, SourceFilep, lineno);
  return;
}

void
__sc_dbg_poolunregister_stack (DebugPoolTy *Pool, void * allocaptr) {
  _internal_poolunregister (Pool, allocaptr, Stack, 0, "Unknown", 0);
  return;
}

void
__sc_dbg_poolunregister_stack_debug (DebugPoolTy *Pool,
                                     void * allocaptr,
                                     TAG,
                                     const char * SourceFilep,
                                     unsigned lineno) {
  checkForBadFrees (Pool, allocaptr, Stack, tag, SourceFilep, lineno);
  _internal_poolunregister (Pool, allocaptr, Stack, tag, SourceFilep, lineno);
  return;
}

//
// Function: poolalloc_debug()
//
// Description:
//  This function is just like poolalloc() except that it associates a source
//  file and line number with the allocation.
//
void *
__sc_dbg_src_poolalloc (DebugPoolTy *Pool,
                        unsigned NumBytes, TAG,
                        const char * SourceFilep,
                        unsigned lineno) {
  //
  // Ensure that we're allocating at least one byte.
  //
  if (NumBytes == 0) NumBytes = 1;

  // Perform the allocation and determine its offset within the physical page.
  void * canonptr = __pa_bitmap_poolalloc(Pool, NumBytes);
  return canonptr;
}

//
// Function: poolfree_debug()
//
// Description:
//  This function is identical to poolfree() except that it relays source-level
//  debug information to the error reporting routines.
//
void
__sc_dbg_src_poolfree (DebugPoolTy *Pool,
                       void * Node, TAG,
                       const char * SourceFile,
                       unsigned int lineno) {
  //
  // Free the object within the pool; the poolunregister() function will
  // detect invalid frees.
  //
  __pa_bitmap_poolfree (Pool, Node);
}


//===----------------------------------------------------------------------===//
//
// Dangling pointer runtime functions
//
//===----------------------------------------------------------------------===//

//
// Function: createPtrMetaData()
//  Allocates memory for a DebugMetaData struct and fills up the appropriate
//  fields so to keep a record of the pointer's meta data
//
// Inputs:
//  AllocID        - A unique identifier for the allocation.
//  FreeID         - A unique identifier for the deallocation.
//  allocationType - The type of allocation.
//  AllocPC        - The program counter at which the object was allocated.
//  FreePC         - The program counter at which the object was freed.
//  Canon          - The canonical address of the memory object.
//
static PDebugMetaData
createPtrMetaData (unsigned AllocID,
                   unsigned FreeID,
                   allocType allocationType,
                   void * AllocPC,
                   void * FreePC,
                   void * Canon,
                   char * SourceFile,
                   unsigned lineno) {
  // FIXME:
  //  This will cause an allocation that is registered as an external
  //  allocation.  We need to use some internal allocation routine.
  //
  PDebugMetaData ret = (PDebugMetaData) malloc (sizeof(DebugMetaData));
  ret->allocID = AllocID;
  ret->freeID = FreeID;
  ret->allocPC = AllocPC;
  ret->freePC = FreePC;
  ret->canonAddr = Canon;
  ret->SourceFile = SourceFile;
  ret->lineno = lineno;
  ret->allocationType = allocationType;

  ret->FreeSourceFile = 0;
  ret->Freelineno = 0;
  return ret;
}

static inline void
updatePtrMetaData (PDebugMetaData debugmetadataptr,
                   unsigned freeID,
                   void * paramFreePC,
                   void * SourceFile,
                   unsigned lineno) {
  debugmetadataptr->freeID = freeID;
  debugmetadataptr->freePC = paramFreePC;
  debugmetadataptr->FreeSourceFile = SourceFile;
  debugmetadataptr->Freelineno = lineno;
  return;
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
static uintptr_t
getProgramCounter (void * context) {
#if defined(__APPLE__)
#if defined(i386) || defined(__i386__) || defined(__x86__)
  // Cast parameters to the desired type
  ucontext_t * mycontext = (ucontext_t *) context;
  return (mycontext->uc_mcontext->__ss.__eip);
#endif

#if defined(__x86_64__)
  // Cast parameters to the desired type
  ucontext_t * mycontext = (ucontext_t *) context;
  return (mycontext->uc_mcontext->__ss.__rip);
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
// Function: bus_error_handler()
//
// Description:
//  This is the signal handler that catches bad memory references.
//
static void
bus_error_handler (int sig, siginfo_t * info, void * context) {
  if (1) {
    fprintf (stderr, "SAFECode: Fault!\n");
    fflush (stderr);
  }

  //
  // Disable the signal handler for now.  If this function does something
  // wrong, we want the bus error to terminate the program.
  //
  signal(SIGBUS, NULL);

  //
  // Get the program counter for where the fault occurred.
  //
  uintptr_t program_counter = getProgramCounter (context);

  //
  // Get the address causing the fault.
  //
  void * faultAddr = info->si_addr, *end;
  PDebugMetaData debugmetadataptr;
  int fs = 0;

  //
  // If the faulting pointer is within the zero page or the reserved memory
  // region for uninitialized variables, then report an error.
  //
#if defined(__linux__)
  const unsigned lowerUninit = 0xc0000000u;
  const unsigned upperUninit = 0xffffffffu;
#else
  unsigned lowerUninit = 0x00000000u;
  unsigned upperUninit = 0x00000fffu;
#endif
  if ((lowerUninit <= (uintptr_t)(faultAddr)) &&
      ((uintptr_t)(faultAddr) <= upperUninit)) {
    DebugViolationInfo v;
    v.type = ViolationInfo::FAULT_UNINIT,
      v.faultPC = (const void*) program_counter,
      v.faultPtr = faultAddr,
      v.dbgMetaData = 0;

    ReportMemoryViolation(&v);
    return;
  }

  //
  // Attempt to look up dangling pointer information for the faulting pointer.
  //
  fs = dummyPool.DPTree.find (info->si_addr, faultAddr, end, debugmetadataptr);

  //
  // If there is no dangling pointer information for the faulting pointer,
  // perhaps it is an Out of Bounds Rewrite Pointer.  Check for that now.
  //
  if (0 == fs) {
    void * start = faultAddr;
    void * tag = 0;
    void * end;
    if (OOBPool.OOB.find (faultAddr, start, end, tag)) {
      char * Filename = (char *)(RewriteSourcefile[faultAddr]);
      unsigned lineno = RewriteLineno[faultAddr];

      //
      // Get the bounds of the original object.
      //
      getOOBObject (faultAddr, start, end);
      OutOfBoundsViolation v;
      v.type = ViolationInfo::FAULT_LOAD_STORE,
        v.faultPC = (const void*)program_counter,
        v.faultPtr = tag,
        v.dbgMetaData = NULL,
        v.SourceFile = Filename,
        v.lineNo = lineno,
        v.objStart = start,
        // FIXME: Make sure there is no off by one error in the line below
        v.objLen = (char *)(end) - (char *)(start);

      ReportMemoryViolation(&v);
    } else {
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
    }

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
 
  // FIXME: Correct the semantics for calculating NumPPage 
  unsigned NumPPage;
  unsigned offset = (unsigned) ((long)info->si_addr & (PPageSize - 1) );
  unsigned int len = (unsigned char *)(end) - (unsigned char *)(faultAddr) + 1;
  NumPPage = (len / PPageSize) + 1;
  if ( (len - (NumPPage-1) * PPageSize) > (PPageSize - offset) )
    NumPPage++;
 
  // This is necessary so that the program continues execution,
  //  especially in debugging mode 
  UnprotectShadowPage((void *)((long)info->si_addr & ~(PPageSize - 1)), NumPPage);
  
  //void* S = info->si_addr;
  // printing reports
  void * address = info->si_addr;

  DebugViolationInfo v;
    v.type = ViolationInfo::FAULT_DANGLING_PTR,
      v.faultPC = (const void*) program_counter,
    v.faultPtr = address,
    v.dbgMetaData = debugmetadataptr;

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

static void *
getCanonicalPtr (void * ShadowPtr) {
  //
  // Look for the pointer in the dummy pool.  Assume that if it is not found,
  // we will return the original shadow pointer.
  //
  void * start, * end;
  void * CanonPtr = 0;
  bool found = ShadowMap.find (ShadowPtr, start, end, CanonPtr);
  return (found ? CanonPtr : ShadowPtr);
}

//
// Function: pool_shadow()
//
// Description:
//  Given the pointer to the beginning of an object, create a shadow object.
//  This means that the physical memory is mapped to a new virtual address
//  (i.e., the shadow address).  This shadow address is never re-used, so we
//  can use it for dangling pointer detection.
//
// Inputs:
//  CanonPtr - The pointer to remap.  This *must* be a pointer to the beginning
//             of a heap object allocated by poolalloc().
//  NumBytes - The size of the allocated object in bytes.
//
// Notes:
//  This function does not do any sanity checking on its input.  Calls to it
//  are added by the SAFECode transforms.  Therefore, there is no sanity
//  checking on the input.
//
void *
pool_shadow (void * CanonPtr, unsigned NumBytes) {
  //
  // Calculate the offset of the object from the beginning of the page.
  //
  uintptr_t offset = (((uintptr_t)(CanonPtr)) & (PPageSize-1));

  //
  // Remap the object, if necessary, and then calculate the pointer to the
  // shadow object (RemapObject() returns the beginning of the page).
  //
  void * shadowpage = RemapObject (CanonPtr, NumBytes);
  void * shadowptr = (unsigned char *)(shadowpage) + offset;

  //
  // Record the mapping from shadow pointer to canonical pointer.
  //
  ShadowMap.insert (shadowptr, 
                        (char*) shadowptr + NumBytes - 1,
                        CanonPtr);
  if (logregs) {
    fprintf (stderr, "pool_shadow: %p -> %p\n", CanonPtr, shadowptr);
    fflush (stderr);
  }
  return shadowptr;
}

//
// Function: pool_unshadow()
//
// Description:
//  This function modifies the page protections of an object so that it is no
//  longer writeable.
//
// Inputs:
//  Node - A pointer to the beginning of the object that should be marked as
//         read-only.
//
// Return value:
//  The canonical version of the pointer is returned.  This value can be safely
//  passed to poolfree().
//
// Notes:
//  This function should only be called when dangling pointer detection is
//  enabled.
//
void *
pool_unshadow (void * Node) {
  // The start and end of the object as registered in the dangling pointer
  // object metapool
  void * start = 0, * end = 0;

  //
  // Retrieve the debug information about the node.  This will include a
  // pointer to the canonical page.
  //
  PDebugMetaData debugmetadataptr = 0;
  bool found = dummyPool.DPTree.find (Node, start, end, debugmetadataptr);

  // Assert that we either didn't find the object or we found the object *and*
  // it has meta-data associated with it.
  assert ((!found || (found && debugmetadataptr)) &&
          "poolfree: No debugmetadataptr\n");

  //
  // If the object is not found, return.
  //
  if (!found) {
    return Node;
  }

  if (logregs) {
    fprintf (stderr, "pool_unshadow: Start: %p\n", Node);
    fflush (stderr);
  }

  //
  // Determine the number of pages that the object occupies.
  //
  ptrdiff_t len = (intptr_t)end - (intptr_t)start;
  unsigned offset = (unsigned)((long)Node & (PPageSize - 1));
  unsigned NumPPage = (len / PPageSize) + 1;
  if ( (len - (NumPPage-1) * PPageSize) > (PPageSize - offset) )
    NumPPage++;

  if (logregs) {
    fprintf (stderr, "pool_unshadow: Middle: %p\n", Node);
    fflush (stderr);
  }

  // Protect the shadow pages of the object
  ProtectShadowPage((void *)((long)Node & ~(PPageSize - 1)), NumPPage);
  if (logregs) {
    fprintf (stderr, "pool_unshadow: Done: %p\n", Node);
    fflush (stderr);
  }
  return debugmetadataptr->canonAddr;
}

//
// Function: poolcalloc_debug()
//
// Description:
//  This is the same as pool_calloc but with source level debugging
//  information.
//
// Inputs:
//  Pool        - The pool from which to allocate the elements.
//  Number      - The number of elements to allocate.
//  NumBytes    - The size of each element in bytes.
//  SourceFilep - A pointer to the source filename in which the caller is
//                located.
//  lineno      - The line number at which the call occurs in the source code.
//
// Return value:
//  NULL - The allocation did not succeed.
//  Otherwise, a fresh pointer to the allocated memory is returned.
//
// Notes:
//  Note that this function calls poolregister() directly because the SAFECode
//  transforms do not add explicit calls to poolregister().
//
void *
__sc_dbg_src_poolcalloc (DebugPoolTy *Pool,
                         unsigned Number,
                         unsigned NumBytes, TAG,
                         const char * SourceFilep,
                         unsigned lineno) {
  //
  // Allocate the desired amount of memory.
  //
  void * New = __sc_dbg_src_poolalloc (Pool, Number * NumBytes, tag, SourceFilep, lineno);

  //
  // If the allocation succeeded, zero out the memory and do needed SAFECode
  // operations.
  //
  if (New) {
    // Zero the memory
    bzero (New, Number * NumBytes);
  }

  //
  // Print out some debugging information.
  //
  if (logregs) {
    fprintf (ReportLog, "poolcalloc_debug: %p: %p %x: %s %d\n", (void*) Pool, (void*)New, Number * NumBytes, SourceFilep, lineno);
    fflush (ReportLog);
  }
  return New;
}

void *
__sc_dbg_poolcalloc (DebugPoolTy *Pool, unsigned Number, unsigned NumBytes) {
  return __sc_dbg_src_poolcalloc (Pool, Number, NumBytes, 0, "<unknown>", 0);
}

void *
__sc_dbg_poolrealloc (DebugPoolTy *Pool, void *Node, unsigned NumBytes) {
  //
  // If the object has never been allocated before, allocate it now, create a
  // shadow object (if necessary), and register the object as a heap object.
  //
  if (Node == 0) {
    void * New = __pa_bitmap_poolalloc(Pool, NumBytes);
    if (ConfigData.RemapObjects) New = pool_shadow (New, NumBytes);
    __sc_dbg_poolregister (Pool, New, NumBytes);
    return New;
  }

  //
  // Reallocate an object to 0 bytes means that we wish to free it.
  //
  if (NumBytes == 0) {
    _internal_poolunregister (Pool, Node, Heap, 0, "Unknown", 0);
    if (ConfigData.RemapObjects) Node = pool_unshadow (Node);
    __pa_bitmap_poolfree(Pool, Node);
    return 0;
  }

  //
  // Otherwise, we need to change the size of the allocated object.  For now,
  // we will simply allocate a new object and copy the data from the old object
  // into the new object.
  //

  //
  // Get the bounds of the old object.  If we cannot get the bounds, then
  // simply fail the allocation.
  //
  void * S, * end;
  if ((!(Pool->Objects.find (Node, S, end))) || (S != Node)) {
    return 0;
  }

  //
  // Allocate a new object.  If we fail, return NULL.
  //
  void *New;
  if ((New = __pa_bitmap_poolalloc(Pool, NumBytes)) == 0)
    return 0;

  //
  // Create a shadow of the new object (if necessary) and register it with the
  // pool.
  //
  if (ConfigData.RemapObjects) New = pool_shadow (New, NumBytes);
  __sc_dbg_poolregister (Pool, New, NumBytes);

  //
  // Determine the number of bytes to copy into the new object.
  //
  ptrdiff_t length = NumBytes;
  if ((((uintptr_t)(end)) - ((uintptr_t)(S)) + 1) < NumBytes) {
    length = ((intptr_t)(end)) - ((intptr_t)(S)) + 1;
  }

  //
  // Copy the contents of the old object into the new object.
  //
  memcpy(New, Node, length);

  //
  // Invalidate the old object and its bounds and return the pointer to the
  // new object.
  //
  _internal_poolunregister(Pool, Node, Heap, 0, "Unknown", 0);
  if (ConfigData.RemapObjects) Node = pool_unshadow (Node);
  __pa_bitmap_poolfree(Pool, Node);
  return New;
}

void *
__sc_dbg_poolrealloc_debug (DebugPoolTy *Pool,
                            void *Node,
                            unsigned NumBytes, TAG,
                            const char * SourceFilep,
                            unsigned lineno) {
  //
  // If the object has never been allocated before, allocate it now, create a
  // shadow object (if necessary), and register the object as a heap object.
  //
  if (Node == 0) {
    void * New = __pa_bitmap_poolalloc(Pool, NumBytes);
    if (ConfigData.RemapObjects) New = pool_shadow (New, NumBytes);
    __sc_dbg_src_poolregister (Pool, New, NumBytes, tag, SourceFilep, lineno);
    return New;
  }

  //
  // Reallocate an object to 0 bytes means that we wish to free it.
  //
  if (NumBytes == 0) {
    _internal_poolunregister (Pool, Node, Heap, tag, SourceFilep, lineno);
    if (ConfigData.RemapObjects) Node = pool_unshadow (Node);
    __pa_bitmap_poolfree(Pool, Node);
    return 0;
  }

  //
  // Otherwise, we need to change the size of the allocated object.  For now,
  // we will simply allocate a new object and copy the data from the old object
  // into the new object.
  //

  //
  // Get the bounds of the old object.  If we cannot get the bounds, then
  // simply fail the allocation.
  //
  void * S, * end;
  if ((!(Pool->Objects.find (Node, S, end))) || (S != Node)) {
    return 0;
  }

  //
  // Allocate a new object.  If we fail, return NULL.
  //
  void *New;
  if ((New = __pa_bitmap_poolalloc(Pool, NumBytes)) == 0)
    return 0;

  //
  // Create a shadow of the new object (if necessary) and register it with the
  // pool.
  //
  if (ConfigData.RemapObjects) New = pool_shadow (New, NumBytes);
  __sc_dbg_src_poolregister (Pool, New, NumBytes, tag, SourceFilep, lineno);

  //
  // Determine the number of bytes to copy into the new object.
  //
  ptrdiff_t length = NumBytes;
  if ((((uintptr_t)(end)) - ((uintptr_t)(S)) + 1) < NumBytes) {
    length = ((intptr_t)(end)) - ((intptr_t)(S)) + 1;
  }

  //
  // Copy the contents of the old object into the new object.
  //
  memcpy(New, Node, length);

  //
  // Invalidate the old object and its bounds and return the pointer to the
  // new object.
  //
  _internal_poolunregister(Pool, Node, Heap, tag, SourceFilep, lineno);
  if (ConfigData.RemapObjects) Node = pool_unshadow (Node);
  __pa_bitmap_poolfree(Pool, Node);
  return New;
}

//
// Function: internal_poolstrdup()
//
// Description:
//  This is a pool allocated version of the strdup() function call.  It ensures
//  that the object is properly registered in the correct pool.  This function
//  contains common code for both the production and debug versions of the
//  poolstrdup() function.
//
// Inputs:
//  Pool        - The pool in which the new string should reside.
//  Node        - The string which should be duplicated.
//
// Outputs:
//  length      - The length of the memory object (in bytes) allocated by this
//                function to hold the new string.
// Return value:
//  0 - The duplication failed.
//  Otherwise, a pointer to the duplicated string is returned.
//
static void *
internal_poolstrdup (DebugPoolTy * Pool,
                     const char * String,
                     unsigned & length) {
  //
  // First determine the size of the string.  We use pool_strlen() to ensure
  // that we do this safely.  Remember to increment the length by 1 to handle
  // the fact that there is space for the string terminator byte.
  //
  extern size_t pool_strlen(DebugPoolTy *stringPool, const char *string, const unsigned char complete);
  length = pool_strlen(Pool, String, 0) + 1;

  //
  // Now call the pool allocator's strdup() function.
  //
  const void * Node = String;
  void * NewNode = __pa_bitmap_poolstrdup (static_cast<BitmapPoolTy*>(Pool),
                                           (void *)(Node));

  if (NewNode) {
    //
    // Create a shadow copy of the object if dangling pointer detection is
    // enabled.
    //
    if (ConfigData.RemapObjects)
      NewNode = pool_shadow (NewNode, length);
  }

  return NewNode;
}

//
// Function: poolstrdup()
//
// Description:
//  This is a pool allocated version of the strdup() function call.  It ensures
//  that the object is properly registered in the correct pool.
//
// Inputs:
//  Pool        - The pool in which the new string should reside.
//  Node        - The string which should be duplicated.
//
// Return value:
//  0 - The duplication failed.
//  Otherwise, a pointer to the duplicated string is returned.
//
void *
__sc_dbg_poolstrdup (DebugPoolTy * Pool, const char * Node) {
  unsigned length = 0;
  void * NewNode = internal_poolstrdup (Pool, Node, length);
  if (NewNode) {
    __sc_dbg_poolregister (Pool, NewNode, length);
  }

  return NewNode;
}

//
// Function: poolstrdup_debug()
//
// Description:
//  This is a pool allocated version of the strdup() function call.  It ensures
//  that the object is properly registered in the correct pool.
//
// Inputs:
//  Pool        - The pool in which the new string should reside.
//  Node        - The string which should be duplicated.
//  tag         - The tag associated with the call site.
//  SourceFilep - The source file name in which the call site is located.
//  lineno      - The source line number at which the call site is located.
//
// Return value:
//  0 - The duplication failed.
//  Otherwise, a pointer to the duplicated string is returned.
//
void *
__sc_dbg_poolstrdup_debug (DebugPoolTy * Pool,
                           const char * Node,
                           TAG,
                           const char * SourceFilep,
                           unsigned lineno) {
  unsigned length = 0;
  void * NewNode = internal_poolstrdup (Pool, Node, length);
  if (NewNode) {
    __sc_dbg_src_poolregister (Pool,
                               NewNode,
                               length,
                               tag,
                               SourceFilep,
                               lineno);
  }

  return NewNode;
}

//
// Function: poolinit()
//
// Description:
//  Initialize a pool used in the debug run-time.
//
// Inputs:
//  Pool - A pointer to the pool to initialize.
//  NodeSize - The default size of an object allocated within the pool.
//
void *
__sc_dbg_poolinit(DebugPoolTy *Pool, unsigned NodeSize, unsigned) {
  //
  // Create a record if necessary.
  if (logregs) {
    fprintf (stderr, "poolinit: %p %u\n", (void *) Pool, NodeSize);
    fflush (stderr);
  }

  //
  // Call the underlying allocator's poolinit() function to initialze the pool.
  //
  __pa_bitmap_poolinit(Pool, NodeSize);

  //
  // Call the in-place new operator for the splay tree of objects and, if
  // applicable, the set of Out of Bound rewrite pointers and the splay tree
  // used for dangling pointer detection.  This causes their constructors to
  // be called on the already allocated memory.
  //
  // While this may appear odd, it is what we want.  The allocation of pools
  // are added by the pool allocation transform.  Pools are either global
  // variables (context insensitive) or stack allocated objects
  // (context-sensitive).  Either way, their memory is not allocated by this
  // run-time so in-place new operators must be used to initialize C++ classes
  // within the pool.
  //
  new (&(Pool->Objects)) RangeSplaySet<>();
  new (&(Pool->OOB)) RangeSplayMap<void *>();
  new (&(Pool->DPTree)) RangeSplayMap<PDebugMetaData>();

  //
  // Initialize the object cache.
  //
  Pool->objectCache[0].lower = 0;
  Pool->objectCache[0].upper = 0;
  Pool->objectCache[1].lower = 0;
  Pool->objectCache[1].upper = 0;
  Pool->cacheIndex = 0;

  return Pool;
}

