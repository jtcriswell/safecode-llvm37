//===- MallocHooks.cpp - Implementation of hooks to malloc() functions ----===//
// 
//                            The SAFECode Compiler
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file implements functions that interrupt and record allocations created
// by the system's original memory allocators.  This allows the SAFECode
// compiler to work with external code.
//
//===----------------------------------------------------------------------===//

#include "safecode/SAFECode.h"
#include "poolalloc_runtime/Support/SplayTree.h"

#if defined(__APPLE__)
#include <malloc/malloc.h>
#endif

NAMESPACE_SC_BEGIN

// Splay tree for recording external allocations
RangeSplaySet<> ExternalObjects;

#if defined(__APPLE__)
// The real allocation functions
static void * (*real_malloc)  (malloc_zone_t *, size_t);
static void * (*real_calloc)  (malloc_zone_t *, size_t, size_t);
static void * (*real_valloc)  (malloc_zone_t *, size_t);
static void * (*real_realloc) (malloc_zone_t *, void *, size_t);
static void   (*real_free)    (malloc_zone_t *, void *);

// Prototypes for the tracking versions
static void * track_malloc  (malloc_zone_t *, size_t size);
static void * track_calloc  (malloc_zone_t *, size_t num_items, size_t size);
static void * track_valloc  (malloc_zone_t *, size_t size);
static void * track_realloc (malloc_zone_t *, void * p, size_t size);
static void   track_free    (malloc_zone_t *, void * p);

void
installAllocHooks (void) {
  // Pointer to the default malloc zone
  malloc_zone_t * default_zone;

  //
  // Get the default malloc zone and record the pointers to the real malloc
  // functions.
  //
  default_zone = malloc_default_zone();
  real_malloc  = default_zone->malloc;
  real_calloc  = default_zone->calloc;
  real_valloc  = default_zone->valloc;
  real_realloc = default_zone->realloc;
  real_free    = default_zone->free;

  //
  // Install intercept routines.
  //
  default_zone->malloc = track_malloc;
  default_zone->calloc  = track_calloc;
  default_zone->valloc  = track_valloc;
  default_zone->realloc = track_realloc;
  default_zone->free    = track_free;
}

static void *
track_malloc (malloc_zone_t * zone, size_t size) {
  // Pointer to the allocated object
  char * objp;

  //
  // Perform the allocation.
  //
  objp = (char*) real_malloc (zone, size);

  //
  // Record the allocation and return to the caller.
  //
  ExternalObjects.insert(objp, objp + size);
  return objp;
}

static void *
track_valloc (malloc_zone_t * zone, size_t size) {
  // Pointer to the allocated object
  char * objp;

  //
  // Perform the allocation.
  //
  objp = (char*) real_valloc (zone, size);

  //
  // Record the allocation and return to the caller.
  //
  ExternalObjects.insert(objp, objp + size);
  return objp;
}

static void *
track_calloc (malloc_zone_t * zone, size_t num, size_t size) {
  // Pointer to the allocated object
  char * objp;

  //
  // Perform the allocation.
  //
  objp = (char*) real_calloc (zone, num, size);

  //
  // Record the allocation and return to the caller.
  //
  ExternalObjects.insert(objp, objp + num * size);
  return objp;
}

static void *
track_realloc (malloc_zone_t * zone, void * oldp, size_t size) {
  // Pointer to the allocated object
  char * objp;

  //
  // Perform the allocation.
  //
  objp = (char*) real_realloc (zone, oldp, size);

  //
  // Record the allocation and return to the caller.
  //
  ExternalObjects.insert(objp, objp + size);
  return objp;
}

static void
track_free (malloc_zone_t * zone, void * p) {
  //
  // Perform the allocation.
  //
  real_free (zone, p);

  //
  // Record the allocation and return to the caller.
  //
  ExternalObjects.remove(p);
  return;
}
#else
void
installAllocHooks (void) {
  return;
}
#endif

NAMESPACE_SC_END
