//===- SAFECodeRuntime.h -- Runtime interface of SAFECode ------*- C++ -*-===//
// 
//                     The LLVM Compiler Infrast`ructure
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file defines the interface of SAFECode runtime library.
//
//===----------------------------------------------------------------------===//

#ifndef _SAFECODE_RUNTIME_H_
#define _SAFECODE_RUNTIME_H_

#include "BitmapAllocator.h"
#include "SplayTree.h"

#include <iosfwd>
#include <stdint.h>

namespace llvm {

//
// Enumerated Type: allocType
//
// Description:
//  This enumerated type lists the different types of allocations that can be
//  made.
//
enum allocType {
  Global,   // Global object
  Stack,    // Stack-allocated object
  Heap      // Heap-allocated object
};

//
// Structure: DebugMetaData
//
// Description:
//  This structure contains information on the error to be reported.
//
// Fields:
//  allocID    : The ID number of the allocation of the object.
//  freeID     : The ID number of the deallocation of the object.
//  allocPC    : The program counter at which the object was last allocated.
//  freePC     : The program counter at which the object was last deallocated.
//  canonAddr  : The canonical address of the memory reference.
//  SourceFile : A string containing the source file to which the erring
//               instruction is found.
//  lineno     : The line number in the source file to which the erring
//               instruction is found.
//
typedef struct DebugMetaData {
  unsigned allocID;
  unsigned freeID;
  void * allocPC;
  void * freePC;
  void * canonAddr;

  // Allocation type (global, stack, or heap object)
  allocType allocationType;

  // Source filename
  const void * SourceFile;

  // Source filename for deallocation
  const void * FreeSourceFile;

  // Line number
  unsigned lineno;

  // Line number for deallocation
  unsigned Freelineno;

  void print(std::ostream & OS) const;
} DebugMetaData;
typedef DebugMetaData * PDebugMetaData;

struct DebugPoolTy : public BitmapPoolTy {
  // Splay tree used for object registration
  RangeSplaySet<> Objects;

  // Splay tree used for out of bound objects
  RangeSplayMap<void *> OOB;

  // Splay tree used by dangling pointer runtime
  RangeSplayMap<PDebugMetaData> DPTree;

  // Cache of recently found memory objects
  struct {
    void * lower;
    void * upper;
  } objectCache[2];

  unsigned char cacheIndex;
};

void * rewrite_ptr (DebugPoolTy * Pool, const void * p, void * ObjStart,
void * ObjEnd, const char * SourceFile, unsigned lineno);
void installAllocHooks (void);

}

// Use macros so that I won't polluate the namespace

#define PPOOL llvm::DebugPoolTy*
#define TAG unsigned
#define SRC_INFO const char *, unsigned int

extern "C" {
  void pool_init_runtime(unsigned Dangling,
                         unsigned RewriteOOB,
                         unsigned Terminate);
  void pool_init_logfile (const char * name);
  void * __sc_dbg_newpool(unsigned NodeSize);
  void __sc_dbg_pooldestroy(PPOOL);

  void * __sc_dbg_poolinit(PPOOL, unsigned NodeSize, unsigned);
  void * __sc_dbg_poolalloc(PPOOL, unsigned NumBytes);
  void * __sc_dbg_src_poolalloc (PPOOL, unsigned Size, TAG, SRC_INFO);

  void * poolargvregister (int argc, char ** argv);

  void pool_register       (PPOOL, void *allocaptr, unsigned NumBytes);
  void pool_register_debug (PPOOL, void * p, unsigned size, TAG, SRC_INFO);
  void pool_register_stack      (PPOOL, void * p, unsigned size);
  void pool_register_stack_debug(PPOOL, void * p, unsigned size, TAG, SRC_INFO);
  void pool_register_global (PPOOL, void * p, unsigned size);
  void pool_register_global_debug(PPOOL, void * p, unsigned size, TAG, SRC_INFO);

  void pool_reregister (PPOOL, void * p, void * q, unsigned size);
  void pool_reregister_debug (PPOOL, void * p, void * q, unsigned size, TAG, SRC_INFO);

  void pool_unregister(PPOOL, void *allocaptr);
  void pool_unregister_debug(PPOOL, void *allocaptr, TAG, SRC_INFO);
  void pool_unregister_stack(PPOOL, void *allocaptr);
  void pool_unregister_stack_debug(PPOOL, void *allocaptr, TAG, SRC_INFO);
  void __sc_dbg_poolfree(PPOOL, void *Node);
  void __sc_dbg_src_poolfree (PPOOL, void *, TAG, SRC_INFO);

  void * __sc_dbg_poolcalloc (PPOOL, unsigned Number, unsigned NumBytes);
  void * __sc_dbg_src_poolcalloc (PPOOL,
                                unsigned Number, unsigned NumBytes,
                                  TAG, SRC_INFO);

  void * poolrealloc(PPOOL, void *Node, unsigned NumBytes);
  void * __sc_dbg_poolrealloc_debug(PPOOL, void *Node, unsigned NumBytes, TAG, SRC_INFO);
  void * __sc_dbg_poolstrdup (PPOOL, const char * Node);
  void * __sc_dbg_poolstrdup_debug (PPOOL, const char * Node, TAG, SRC_INFO);
  void * __sc_dbg_poolmemalign(PPOOL, unsigned Alignment, unsigned NumBytes);

  void poolcheck(PPOOL, void *Node, unsigned length);
  void poolcheckui(PPOOL, void *Node, unsigned length);
  void poolcheck_debug (PPOOL, void * Node, unsigned length, TAG, SRC_INFO);
  void poolcheckui_debug (PPOOL, void * Node, unsigned length, TAG, SRC_INFO);

  void poolcheckalign(PPOOL, void *Node, unsigned Offset);
  void poolcheckalign_debug (PPOOL, void *Node, unsigned Offset, TAG, SRC_INFO);

  void * boundscheck   (PPOOL, void * Source, void * Dest);
  void * boundscheckui (PPOOL, void * Source, void * Dest);
  void * boundscheckui_debug (PPOOL, void * S, void * D, TAG, SRC_INFO);
  void * boundscheck_debug (PPOOL, void * S, void * D, TAG, SRC_INFO);

  // Exact checks
  void * exactcheck2 (char *source, char *base, char *result, unsigned size);
  void * exactcheck2_debug (char *source, char *base, char *result, 
                            unsigned size, TAG, SRC_INFO);
  void fastlscheck (const char *base, const char *result, unsigned size,
                    unsigned lsLen);
  void fastlscheck_debug (const char *base, const char *result, unsigned size,
                          unsigned lsLen,
                          unsigned tag,
                          const char * SourceFile,
                          unsigned lineno);

  void * pchk_getActualValue (PPOOL, void * src);

  // Indirect function call checks
  void funccheck   (void *f, void * targets[]);
  void funccheckui (void *f, void * targets[]);
  void funccheck_debug   (void *f, void * targets[], TAG, SRC_INFO);
  void funccheckui_debug (void *f, void * targets[], TAG, SRC_INFO);

  // Change memory protections to detect dangling pointers
  void * pool_shadow (void * Node, unsigned NumBytes);
  void * pool_unshadow (void * Node);

  // Check for invalid frees for non-resistent allocators
  void poolcheck_free   (PPOOL, void * ptr);
  void poolcheck_freeui (PPOOL, void * ptr);
  void poolcheck_free_debug   (PPOOL, void * ptr, TAG, SRC_INFO);
  void poolcheck_freeui_debug (PPOOL, void * ptr, TAG, SRC_INFO);
}

#undef PPOOL
#undef TAG
#undef SRC_INFO
#endif
