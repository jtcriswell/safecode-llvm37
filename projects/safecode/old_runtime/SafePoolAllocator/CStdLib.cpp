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

#include "PoolAllocator.h"

#include <cassert>
#include <algorithm>

extern "C" {
  size_t strnlen(const char *s, size_t maxlen) {
    size_t i;
    for (i = 0; i < maxlen && s[i]; ++i)
      ;
    return i;
  }

  size_t strnlen_opt(const char *s, size_t maxlen) {
    const char *end = (const char *)memchr(s, '\0', maxlen);
    return (end ? end - s : maxlen);
  }
}

/**
 * Optimized inline assembly versions of strncpy that returns the number of
 * characters copied (including \0)
 *
 * @param   dst      Destination string pointer
 * @param   src      Source string pointer
 * @param   size     Number of characters to copy
 * @return  Number of characters copied
 */
size_t strncpy_asm(char *dst, const char *src, size_t size) {
  long copied;

#if defined(i386) || defined(__i386__) || defined(__x86__)
  __asm__ __volatile__(
    ".0: xorl      %%ecx, %%ecx      \n"
    "    cmpl      %%edi, %%ecx      \n"
    "    adcl      $0, %%ecx         \n"
    "    decl      %%edi             \n"
    "    testl     %%ecx, %%ecx      \n"
    "    je        .1                \n"
    "    movsbl    (%%edx), %%ecx    \n"
    "    movb      %%cl, (%%eax)     \n"
    "    incl      %%eax             \n"
    "    incl      %%edx             \n"
    "    testl     %%ecx, %%ecx      \n"
    "    jne       .0                \n"
    ".1: subl      %%esi, %%eax      \n"
    : "=a"(copied)
    : "a"(dst), "S"(dst), "d"(src), "D"(size)
    : "%ecx", "memory"
  );
#else
  strncpy(dst, src, size);
  copied = strnlen(dst, size);
#endif

  return copied;
}

/**
 * Secure runtime wrapper function to replace strcpy()
 *
 * @param   srcPool  Pool handle for source string
 * @param   dstPool  Pool handle for destination buffer
 * @param   src      Source string pointer
 * @param   dst      Destination string pointer
 * @return  Destination string pointer
 */
char *pool_strcpy(PoolTy *srcPool, PoolTy *dstPool, const char *src, char *dst) {
  size_t copied = 0, stop = 0;
  void *srcBegin = (char *)src, *srcEnd = NULL, *dstBegin = dst, *dstEnd = NULL;

  assert(srcPool && dstPool && src && dst && "CStdLib (pool_strcpy): Null pool parameters!\n");

  bool foundSrc = srcPool->Objects.find(srcBegin, srcBegin, srcEnd);
  assert(foundSrc && "CStdLib (pool_strcpy): Source string not found in pool!\n");

  bool foundDst = dstPool->Objects.find(dstBegin, dstBegin, dstEnd);
  assert(foundDst && "CStdLib (pool_strcpy): Destination buffer not found in pool!\n");

  assert((srcBegin <= srcEnd) && "CStdLib (pool_strcpy): Source pointer out of bounds!\n");
  assert((dstBegin <= dstEnd) && "CStdLib (pool_strcpy): Destination pointer out of bounds!\n");

  stop = std::min((char *)srcEnd - (char *)src, (char *)dstEnd - dst);
  copied = strncpy_asm(dst, src, stop);

  assert(!dst[copied] && "CStdLib (pool_strcpy): Copy violated destination bounds!\n");

  return dst;
}
