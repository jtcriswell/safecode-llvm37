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

#include "safecode/SAFECode.h"
#include "safecode/Runtime/BitmapAllocator.h"
#include "poolalloc_runtime/Support/SplayTree.h"

#include <iosfwd>
#include <stdint.h>

NAMESPACE_SC_BEGIN

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
  void * SourceFile;

  // Source filename for deallocation
  void * FreeSourceFile;

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

void * rewrite_ptr (DebugPoolTy * Pool, const void * p, const void * ObjStart,
const void * ObjEnd, const char * SourceFile, unsigned lineno);
void installAllocHooks (void);

NAMESPACE_SC_END

// Use macros so that I won't pollute the namespace

#define PPOOL NAMESPACE_SC::DebugPoolTy*
#define TAG unsigned
#define SRC_INFO const char *, unsigned int

extern "C" {
  void pool_init_runtime(unsigned Dangling,
                         unsigned RewriteOOB,
                         unsigned Terminate);
  void * __sc_dbg_newpool(unsigned NodeSize);
  void __sc_dbg_pooldestroy(PPOOL);

  void * __sc_dbg_poolinit(PPOOL, unsigned NodeSize, unsigned);
  void * __sc_dbg_poolalloc(PPOOL, unsigned NumBytes);
  void * __sc_dbg_src_poolalloc (PPOOL, unsigned Size, TAG, SRC_INFO);

  void * __sc_dbg_poolargvregister (int argc, char ** argv);

  void __sc_dbg_poolregister(PPOOL, void *allocaptr, unsigned NumBytes);
  void __sc_dbg_src_poolregister (PPOOL, void * p, unsigned size, TAG, SRC_INFO);
  void __sc_dbg_poolregister_stack (PPOOL, void * p, unsigned size);
  void __sc_dbg_src_poolregister_stack (PPOOL, void * p, unsigned size, TAG, SRC_INFO);
  void __sc_dbg_poolregister_global (PPOOL, void * p, unsigned size);
  void __sc_dbg_src_poolregister_global_debug (PPOOL, void * p, unsigned size, TAG, SRC_INFO);

  void __sc_dbg_poolreregister (PPOOL, void * p, void * q, unsigned size);
  void __sc_dbg_src_poolreregister (PPOOL, void * p, void * q, unsigned size, TAG, SRC_INFO);

  void __sc_dbg_poolunregister(PPOOL, void *allocaptr);
  void __sc_dbg_poolunregister_stack(PPOOL, void *allocaptr);
  void __sc_dbg_poolunregister_debug(PPOOL, void *allocaptr, TAG, SRC_INFO);
  void __sc_dbg_poolunregister_stack_debug(PPOOL, void *allocaptr, TAG, SRC_INFO);
  void __sc_dbg_poolfree(PPOOL, void *Node);
  void __sc_dbg_src_poolfree (PPOOL, void *, TAG, SRC_INFO);

  void * __sc_dbg_poolcalloc (PPOOL, unsigned Number, unsigned NumBytes);
  void * __sc_dbg_src_poolcalloc (PPOOL,
                                unsigned Number, unsigned NumBytes,
                                  TAG, SRC_INFO);

  void * __sc_dbg_poolrealloc(PPOOL, void *Node, unsigned NumBytes);
  void * __sc_dbg_poolrealloc_debug(PPOOL, void *Node, unsigned NumBytes, TAG, SRC_INFO);
  void * __sc_dbg_poolstrdup (PPOOL, const char * Node);
  void * __sc_dbg_poolstrdup_debug (PPOOL, const char * Node, TAG, SRC_INFO);
  void * __sc_dbg_poolmemalign(PPOOL, unsigned Alignment, unsigned NumBytes);

  void poolcheck(PPOOL, void *Node);
  void poolcheckui(PPOOL, void *Node);
  void poolcheck_debug (PPOOL, void * Node, TAG, SRC_INFO);
  void poolcheckui_debug (PPOOL, void * Node, TAG, SRC_INFO);

  void poolcheckalign(PPOOL, void *Node, unsigned Offset);
  void poolcheckalign_debug (PPOOL, void *Node, unsigned Offset, TAG, SRC_INFO);

  void * boundscheck   (PPOOL, void * Source, void * Dest);
  void * boundscheckui (PPOOL, void * Source, void * Dest);
  void * boundscheckui_debug (PPOOL, void * S, void * D, TAG, SRC_INFO);
  void * boundscheck_debug (PPOOL, void * S, void * D, TAG, SRC_INFO);

  // CStdLib
  void * pool_memcpy(PPOOL dstPool, PPOOL srcPool, void *dst, const void *src, size_t n, const uint8_t complete);
  void * pool_memcpy_debug(PPOOL dstPool, PPOOL srcPool, void *dst, const void *src, size_t n, const uint8_t complete, TAG, SRC_INFO);
  void * pool_mempcpy(PPOOL dstPool, PPOOL srcPool, void *dst, const void *src, size_t n, const uint8_t complete);
  void * pool_mempcpy_debug(PPOOL dstPool, PPOOL srcPool, void *dst, const void *src, size_t n, const uint8_t complete, TAG, SRC_INFO);
  void * pool_memmove(PPOOL dstPool, PPOOL srcPool, void *dst, const void *src, size_t n, const uint8_t complete);
  void * pool_memmove_debug(PPOOL dstPool, PPOOL srcPool, void *dst, const void *src, size_t n, const uint8_t complete, TAG, SRC_INFO);
  void * pool_memset(PPOOL sPool, void *s, int c, size_t n, const uint8_t complete);
  void * pool_memset_debug(PPOOL sPool, void *s, int c, size_t n, const uint8_t complete, TAG, SRC_INFO);

  char * pool_strcpy(PPOOL dstPool, PPOOL srcPool, char *dst, const char *src, const uint8_t complete);
  char * pool_strcpy_debug(PPOOL dstPool, PPOOL srcPool, char *dst, const char *src, const uint8_t complete, TAG, SRC_INFO);
  char * pool_stpcpy(PPOOL dstPool, PPOOL srcPool, char *dst, const char *src, const uint8_t complete);
  char * pool_stpcpy_debug(PPOOL dstPool, PPOOL srcPool, char *dst, const char *src, const uint8_t complete, TAG, SRC_INFO);
  size_t pool_strlen(PPOOL stringPool, const char *string, const uint8_t complete);
  size_t pool_strlen_debug(PPOOL stringPool, const char *string, const uint8_t complete, TAG, SRC_INFO);
  char * pool_strncpy(PPOOL dstPool, PPOOL srcPool, char *dst, const char *src, size_t n,const uint8_t complete);
  char * pool_strncpy_debug(PPOOL dstPool, PPOOL srcPool, char *dst, const char *src, size_t n, const uint8_t complete, TAG, SRC_INFO);
  size_t pool_strnlen(PPOOL stringPool, const char *string, size_t maxlen,const uint8_t complete);
  size_t pool_strnlen_debug(PPOOL stringPool, const char *string, size_t maxlen, const uint8_t complete, TAG, SRC_INFO);


  char * pool_strchr(PPOOL sPool, const char *s, int c, const uint8_t complete);
  char * pool_strchr_debug(PPOOL sPool, const char *s, int c, const uint8_t complete, TAG, SRC_INFO);
  char * pool_strrchr(PPOOL sPool, const char *s, int c, const uint8_t complete);
  char * pool_strrchr_debug(PPOOL sPool, const char *s, int c, const uint8_t complete, TAG, SRC_INFO);
  char * pool_strstr(PPOOL s1Pool, PPOOL s2Pool, const char *s1, const char *s2, const uint8_t complete);
  char * pool_strstr_debug(PPOOL s1Pool, PPOOL s2Pool, const char *s1, const char *s2,
                           const uint8_t complete, TAG, SRC_INFO);
  char * pool_strcat(PPOOL dstPool, PPOOL srcPool, char *d, const char *s, const uint8_t complete);
  char * pool_strcat_debug(PPOOL dstPool, PPOOL srcPool, char *d, const char *s, const uint8_t complete, TAG, SRC_INFO);
  char * pool_strncat(PPOOL dstPool, PPOOL srcPool, char *d, const char *s, size_t n, const uint8_t complete);
  char * pool_strncat_debug(PPOOL dstPool, PPOOL srcPool, char *d, const char *s, size_t n,
                            const uint8_t complete, TAG, SRC_INFO);
  char * pool_strpbrk(PPOOL sPool, PPOOL aPool, const char *s, const char *a, const uint8_t complete);
  char * pool_strpbrk_debug(PPOOL sPool, PPOOL aPool, const char *s, const char *a,
                            const uint8_t complete, TAG, SRC_INFO);
  
  int    pool_strcmp(PPOOL str1Pool, PPOOL str2Pool, const char *str1, const char *str2, const uint8_t complete);
  int    pool_strcmp_debug(PPOOL str1Pool, PPOOL str2Pool, const char *str1, const char *str2, const uint8_t complete, TAG, SRC_INFO);
  int    pool_strncmp(PPOOL s1p,PPOOL s2p,const char *s1, const char *s2,size_t num,const uint8_t complete);
  int    pool_strncmp_debug(PPOOL s1p,PPOOL s2p,const char *s1, const char *s2,size_t num,const uint8_t complete, TAG, SRC_INFO);
  int    pool_strcasecmp(PPOOL str1Pool, PPOOL str2Pool, const char *str1, const char *str2, const uint8_t complete);
  int    pool_strcasecmp_debug(PPOOL str1Pool, PPOOL str2Pool, const char *str1, const char *str2, const uint8_t complete, TAG, SRC_INFO);
  int    pool_strncasecmp(PPOOL s1p,PPOOL s2p,const char *s1, const char *s2,size_t num,const uint8_t complete);
  int    pool_strncasecmp_debug(PPOOL s1p,PPOOL s2p,const char *s1, const char *s2,size_t num,const uint8_t complete, TAG, SRC_INFO);
  int    pool_memcmp(PPOOL s1p,PPOOL s2p,const void *s1, const void *s2,size_t num,const uint8_t complete);
  int    pool_memcmp_debug(PPOOL s1p,PPOOL s2p,const void *s1, const void *s2,size_t num,const uint8_t complete, TAG, SRC_INFO);
  int    pool_strspn(PPOOL s1p,PPOOL s2p,const char *s1, const char *s2,const uint8_t complete);
  int    pool_strspn_debug(PPOOL s1p,PPOOL s2p,const char *s1, const char *s2,const uint8_t complete, TAG, SRC_INFO);
  int    pool_strcspn(PPOOL s1p,PPOOL s2p,const char *s1, const char *s2,const uint8_t complete);
  int    pool_strcspn_debug(PPOOL s1p,PPOOL s2p,const char *s1, const char *s2,const uint8_t complete, TAG, SRC_INFO);

  void * pool_memccpy(PPOOL dstPool, PPOOL srcPool, void *dst, const void *src, char c, size_t n, const uint8_t complete);
  void * pool_memccpy_debug(PPOOL dstPool, PPOOL srcPool, void *dst, const void *src, char c, size_t n, const uint8_t complete, TAG, SRC_INFO);
  void * pool_memchr(PPOOL sPool, void *s, int c, size_t n, const uint8_t complete);
  void * pool_memchr_debug(PPOOL sPool, void *s, int c, size_t n, const uint8_t complete, TAG, SRC_INFO);
  int    pool_bcmp(PPOOL aPool, PPOOL bPool, const void *a, const void *b, size_t n, const uint8_t complete);
  int    pool_bcmp_debug(PPOOL aPool, PPOOL bPool, const void *a, const void *b, size_t n, const uint8_t complete, TAG, SRC_INFO);
  void   pool_bcopy(PPOOL aPool, PPOOL bPool, const void *a, void *b, size_t n, const uint8_t complete);
  void   pool_bcopy_debug(PPOOL aPool, PPOOL bPool, const void *a, void *b, size_t n, const uint8_t complete, TAG, SRC_INFO);
  void   pool_bzero(PPOOL sPool, void *s, size_t n, const uint8_t complete);
  void   pool_bzero_debug(PPOOL sPool, void *s, size_t n, const uint8_t complete, TAG, SRC_INFO);
  char * pool_index(PPOOL sPool, const char *s, int c, const uint8_t complete);
  char * pool_index_debug(PPOOL sPool, const char *s, int c, const uint8_t complete, TAG, SRC_INFO);
  char * pool_rindex(PPOOL sPool, const char *s, int c, const uint8_t complete);
  char * pool_rindex_debug(PPOOL sPool, const char *s, int c, const uint8_t complete, TAG, SRC_INFO);
  char * pool_strcasestr(PPOOL s1Pool, PPOOL s2Pool, const char *s1, const char *s2, const uint8_t complete);
  char * pool_strcasestr_debug(PPOOL s1Pool, PPOOL s2Pool, const char *s1, const char *s2, const uint8_t complete, TAG, SRC_INFO);

#ifdef _GNU_SOURCE
  void * pool_mempcpy(PPOOL dstPool, PPOOL srcPool, void *dst, const void *src, size_t n, const uint8_t complete);
#endif

  // Format string runtime
  void *__sc_fsparameter(void *pool, void *ptr, void *dest, uint8_t complete);
  void *__sc_fscallinfo(void *ci, uint32_t vargc, ...);
  void *__sc_fscallinfo_debug(void *ci, uint32_t vargc, ...);
  int   pool_printf(void *info, void *fmt, ...);
  int   pool_fprintf(void *info, void *dest, void *fmt, ...);
  int   pool_sprintf(void *info, void *dest, void *fmt, ...);
  int   pool_snprintf(void *info, void *dest, size_t n, void *fmt, ...);
  void  pool_err(void *info, int eval, void *fmt, ...);
  void  pool_errx(void *info, int eval, void *fmt, ...);
  void  pool_warn(void *info, void *fmt, ...);
  void  pool_warnx(void *info, void *fmt, ...);
  void  pool_syslog(void *info, int priority, void *fmt, ...);
  int   pool_scanf(void *info, void *fmt, ...);
  int   pool_fscanf(void *info, void *src, void *fmt, ...);
  int   pool_sscanf(void *info, void *str, void *fmt, ...);
  int   pool___printf_chk(void *info, int flag, void *fmt, ...);
  int   pool___fprintf_chk(void *info, void *dest, int flag, void *fmt, ...);
  int   pool___sprintf_chk(void *info, void *dest, int flag, size_t slen, void *fmt, ...);
  int   pool___snprintf_chk(void *info, void *dest, size_t n, int flag, size_t slen, void *fmt, ...);
 
  // Exact checks
  void * exactcheck2 (const char *source, const char *base, 
                      const char *result, unsigned size);
  void * exactcheck2_debug (const char * source, const char *base, 
                            const char *result, unsigned size,
                            TAG, SRC_INFO);

  void __sc_dbg_funccheck (unsigned num, void *f, void *g, ...);
  void * pchk_getActualValue (PPOOL, void * src);

  // Change memory protections to detect dangling pointers
  void * pool_shadow (void * Node, unsigned NumBytes);
  void * pool_unshadow (void * Node);
}

#undef PPOOL
#undef TAG
#undef SRC_INFO
#endif
