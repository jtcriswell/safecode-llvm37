#include <unistd.h>
#include "safecode/Runtime/BBRuntime.h"

#define TAG unsigned tag
using namespace NAMESPACE_SC;

extern "C" void
pool_register (DebugPoolTy *Pool, void * allocaptr, unsigned NumBytes) {
  __sc_bb_src_poolregister(Pool, allocaptr, NumBytes, 0, NULL, 0);
  return;
}

extern "C" void
pool_register_debug (DebugPoolTy *Pool,
                     void * allocaptr,
                     unsigned int NumBytes, TAG,
                     const char* SourceFilep,
                     unsigned int lineno) {
  __sc_bb_src_poolregister(Pool, allocaptr, NumBytes, tag, SourceFilep, lineno);
}

extern "C" void
pool_register_stack_debug(DebugPoolTy *pool,
                          void * allocaptr,
                          unsigned NumBytes, TAG,
                          const char* SourceFilep,
                          unsigned lineno) {
  __sc_bb_src_poolregister_stack(pool, allocaptr, NumBytes, tag, SourceFilep, lineno);
}

extern "C" void
pool_register_global (DebugPoolTy *Pool,
                      void *allocaptr,
                      unsigned NumBytes) {
  __sc_bb_poolregister_global(Pool, allocaptr, NumBytes);
}

extern "C" void
pool_register_global_debug (DebugPoolTy *Pool,
                      void *allocaptr,
                      unsigned NumBytes, TAG,
                      const char* SourceFilep,
                      unsigned lineno) {
  __sc_bb_src_poolregister_global_debug(Pool, allocaptr, NumBytes, tag, SourceFilep, lineno);
}

extern "C" void
pool_unregister (DebugPoolTy *Pool, void * allocaptr) {
  __sc_bb_poolunregister(Pool, allocaptr);
}

extern "C" void
pool_unregister_debug (DebugPoolTy *Pool,
                       void *allocaptr,
                       TAG,
                       const char* SourceFilep,
                       unsigned lineno) {
  __sc_bb_poolunregister_debug(Pool, allocaptr, tag, SourceFilep, lineno);
}

extern "C" void
pool_unregister_stack_debug (DebugPoolTy *Pool,
                             void *allocaptr,
                             TAG,
                             const char* SourceFilep,
                             unsigned lineno) {
  __sc_bb_poolunregister_stack_debug(Pool, allocaptr, tag, SourceFilep, lineno);
}

//
// Function: pool_reregister()
//
// Description:
//  This is pool_register() for realloc() style allocators.  It will unregister
//  the previously existing object (if necessary) and register the newly
//  allocated object.
//
extern "C" void
pool_reregister (DebugPoolTy *Pool,
                 void * newptr,
                 void * oldptr,
                 unsigned NumBytes) {
  if (oldptr == NULL) {
    //
    // If the old pointer is NULL, then we know that this is essentially a
    // regular heap allocation; treat it as such.
    //
    pool_register (Pool, newptr, NumBytes);
  } else if (NumBytes == 0) {
    //
    // Allocating a buffer of zero bytes is essentially a deallocation of the
    // memory; treat it as such.
    //
    pool_unregister (Pool, oldptr);
  } else {
    //
    // Otherwise, this is a true reallocation.  Unregister the old memory and
    // register the new memory.
    pool_unregister (Pool, oldptr);
    pool_register(Pool, newptr, NumBytes);
  }

  return;
}

extern "C" void
pool_reregister_debug (DebugPoolTy *Pool,
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
    pool_register_debug (Pool, newptr, NumBytes, tag, SourceFilep, lineno);
  } else if (NumBytes == 0) {
    //
    // Allocating a buffer of zero bytes is essentially a deallocation of the
    // memory; treat it as such.
    //
    pool_unregister_debug (Pool, oldptr, tag, SourceFilep, lineno);
  } else {
    //
    // Otherwise, this is a true reallocation.  Unregister the old memory and
    // register the new memory.
    pool_unregister_debug (Pool, oldptr, tag, SourceFilep, lineno);
    pool_register_debug   (Pool, newptr, NumBytes, tag, SourceFilep, lineno);
  }

  return;
}

extern "C" void* __sc_fsparameter(void *pool, void *ptr, void *dest, unsigned char complete) {
  return dest;
}

extern "C" void poolargvregister(int argc, char ** argv) {
  __sc_bb_poolargvregister(argc, argv);
}
