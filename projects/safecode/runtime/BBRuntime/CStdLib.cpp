//===------- CStdLib.cpp - CStdLib transform pass runtime functions -------===//
// 
//                          The SAFECode Compiler
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file provides all external functions included by the CStdLib pass.
//
//===----------------------------------------------------------------------===//

#include "DebugReport.h"
#include "PoolAllocator.h"
#include "RewritePtr.h"

#include "safecode/Runtime/BBRuntime.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream> // Debug
#include <stdint.h>

#define TAG unsigned tag
#define SRC_INFO const char *SourceFile, unsigned lineNo

extern unsigned char* __baggybounds_size_table_begin; 
extern unsigned SLOT_SIZE;
extern unsigned WORD_SIZE;
extern const unsigned int logregs;

// Default versions of arguments to debug functions
#define DEFAULT_TAG 0
#define DEFAULT_SRC_INFO "<Unknown>", 0
#define SRC_INFO_ARGS SourceFile, lineNo

// Various violation types
#define OOB_VIOLATION(fault_ptr, handle, start, len) \
    OutOfBoundsViolation v;    \
    v.type        = ViolationInfo::FAULT_OUT_OF_BOUNDS; \
    v.faultPC     = __builtin_return_address(0); \
    v.faultPtr    = fault_ptr;  \
    v.SourceFile  = SourceFile; \
    v.lineNo      = lineNo;     \
    v.PoolHandle  = handle;     \
    v.objStart    = start;      \
    v.objLen      = len;        \
    v.dbgMetaData = NULL;       \
    ReportMemoryViolation(&v);

#define WRITE_VIOLATION(fault_ptr, handle, dst_sz, src_sz) \
    WriteOOBViolation v; \
    v.type = ViolationInfo::FAULT_WRITE_OUT_OF_BOUNDS,\
    v.faultPC = __builtin_return_address(0), \
    v.faultPtr = fault_ptr, \
    v.SourceFile = SourceFile, \
    v.lineNo =     lineNo, \
    v.PoolHandle = handle, \
    v.dstSize =    dst_sz, \
    v.srcSize =    src_sz, \
    v.dbgMetaData = NULL; \
    ReportMemoryViolation(&v);

#define LOAD_STORE_VIOLATION(fault_ptr, handle) \
    DebugViolationInfo v; \
    v.faultPC = __builtin_return_address(0), \
    v.faultPtr = fault_ptr, \
    v.dbgMetaData = NULL, \
    v.PoolHandle = handle, \
    v.SourceFile = SourceFile, \
    v.lineNo = lineNo; \
    ReportMemoryViolation(&v);

using namespace NAMESPACE_SC;

extern "C" {
  size_t strnlen(const char *s, size_t maxlen) {
    size_t i;
    for (i = 0; i < maxlen && s[i]; ++i)
      ;
    return i;
  }

  size_t strnlen_opt(const char *s, size_t maxlen) {
    const char *end = (const char *)memchr(s, '\0', maxlen);
    return (end ? ((size_t) (end - s)) : maxlen);
  }
}


char *bb_pool_strcpy_debug(DebugPoolTy *dstPool, DebugPoolTy *srcPool, char *dst, const char *src, const unsigned char complete, TAG, SRC_INFO) {
  void *dstBegin = dst, *dstEnd = NULL, *srcBegin = (void *)src, *srcEnd = NULL;

  // Ensure all valid pointers.
  assert(dst && src && "Null parameters!");

  // Check the destination buffer is not OOB.
  if (isRewritePtr((void *)dst)) {
    std::cout << "Destination buffer is OOB!\n";

    DebugViolationInfo v;

    v.type = ViolationInfo::FAULT_LOAD_STORE,
    v.faultPC = __builtin_return_address(0),
    v.faultPtr = dstBegin,
    v.dbgMetaData = NULL,
    v.PoolHandle = dstPool,
    v.SourceFile = SourceFile,
    v.lineNo = lineNo;

    ReportMemoryViolation(&v);
  }

  // Check the source buffer's bounds is not OOB.
  if (isRewritePtr((void*)src)) {
    std::cout << "Source string is OOB!\n";

    DebugViolationInfo v;

    v.type = ViolationInfo::FAULT_LOAD_STORE,
    v.faultPC = __builtin_return_address(0),
    v.faultPtr = srcBegin,
    v.dbgMetaData = NULL,
    v.PoolHandle = srcPool,
    v.SourceFile = SourceFile,
    v.lineNo = lineNo;

    ReportMemoryViolation(&v);
  }

  // Check that both the destination and source pointers fall within their respective bounds.
  unsigned char e = __baggybounds_size_table_begin[(uintptr_t)dst >> SLOT_SIZE];
  if (e) {
    std::cout << "Destination pointer out of bounds!\n";

    OutOfBoundsViolation v;

    v.type = ViolationInfo::FAULT_OUT_OF_BOUNDS,
    v.faultPC = __builtin_return_address(0),
    v.faultPtr = dstBegin,
    v.dbgMetaData = NULL,
    v.PoolHandle = dstPool,
    v.SourceFile = SourceFile,
    v.lineNo = lineNo;
    v.objStart = dstBegin,
    v.objLen = (unsigned)((char *)dstEnd - (char *)dstBegin) + 1;

    ReportMemoryViolation(&v);
  }
  e = __baggybounds_size_table_begin[(uintptr_t)src >> SLOT_SIZE];

  if (e) {
    std::cout << "Source pointer out of bounds!\n";

    OutOfBoundsViolation v;

    v.type = ViolationInfo::FAULT_OUT_OF_BOUNDS,
    v.faultPC = __builtin_return_address(0),
    v.faultPtr = srcBegin,
    v.dbgMetaData = NULL,
    v.PoolHandle = srcPool,
    v.SourceFile = SourceFile,
    v.lineNo = lineNo;
    v.objStart = srcBegin,
    v.objLen = (unsigned)((char *)srcEnd - (char *)srcBegin) + 1;

    ReportMemoryViolation(&v);
  }

  return strcpy(dst, src);
}

/**
 * Secure runtime wrapper function to replace strcpy()
 *
 * @param   dstPool  Pool handle for destination buffer
 * @param   srcPool  Pool handle for source string
 * @param   dst      Destination string pointer
 * @param   src      Source string pointer
 * @return  Destination string pointer
 */
char *bb_pool_strcpy(DebugPoolTy *dstPool, DebugPoolTy *srcPool, char *dst, const char *src, const unsigned char complete) {
  return bb_pool_strcpy_debug(dstPool, srcPool, dst, src, complete, 0, "<Unknown>", 0);
}

