//===------- string.cpp - CStdLib Runtime functions for <string.h> --------===//
// 
//                          The SAFECode Compiler
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file provides CStdLib runtime wrapper versions of functions found in
// <string.h>.
//
//===----------------------------------------------------------------------===//

#include "safecode/Config/config.h"
#include "CStdLib.h"

#include <string.h>
#include <algorithm>

//
// Function: poolcheckstr()
//
// Description:
//  This function is a generic load check on a string.  It is intended to be
//  used for C library functions that take a string and read its contents.
//
// Inputs:
//   Pool - The pool handle for the pool in which the string should be.
//          Pool handles can be NULL.
//   str  - A pointer to the string to check.
//
// Notes:
//   We have versions of poolcheckstr() for incomplete/unknown pointers as
//   well as debug versions that pass along debugging information.
//
void
poolcheckstr (DebugPoolTy * Pool, const char * str) {
  if (str == NULL) return;
  validStringCheck (str, Pool, false, "Generic", DEFAULT_SRC_INFO);
}

void
poolcheckstr_debug (DebugPoolTy * Pool, char * str, TAG, SRC_INFO) {
  if (str == NULL) return;
  validStringCheck (str, Pool, false, "Generic", SRC_INFO_ARGS);
}

void
poolcheckstrui (DebugPoolTy * Pool, char * str) {
  if (str == NULL) return;
  validStringCheck (str, Pool, false, "Generic", DEFAULT_SRC_INFO);
}

void
poolcheckstrui_debug (DebugPoolTy * Pool, char * str, TAG, SRC_INFO) {
  if (str == NULL) return;
  validStringCheck (str, Pool, false, "Generic", SRC_INFO_ARGS);
}

//
// pool_memccpy()
//
// See pool_memccpy_debug().
//
void *
pool_memccpy(DebugPoolTy *dPool,
             DebugPoolTy *sPool, 
             void *d,
             void *s,
             char c,
             size_t n,
             const uint8_t complete) {
  return pool_memccpy_debug(dPool, sPool, d, s, c, n, complete, DEFAULTS);
}

//
// Secure runtime wrapper function to replace memccpy()
//
// Inputs:
//   dstPool  Pool handle for destination memory area
//   srcPool  Pool handle for source memory area
//   dst      Destination memory area
//   src      Source memory area
//   c        Stop copying when we see this byte
//   n        Maximum number of bytes to copy
//   complete Completeness bit vector
//   TAG      Tag information for debugging purposes
//   SRC_INFO Source file and line number information for debugging purposes
//
// Returns:
//   A pointer to the first byte after c in dst or, if c was not found in the
//   first n bytes of s2, it returns a null pointer.
//
void *
pool_memccpy_debug(DebugPoolTy *dstPool, 
                   DebugPoolTy *srcPool, 
                   void *dst, 
                   void *src, 
                   int c,
                   size_t n,
                   const uint8_t complete,
                   TAG,
                   SRC_INFO) {
  size_t dstSize = 0, srcSize = 0;
  void *dstBegin = dst, *dstEnd = NULL, *srcBegin = src, *srcEnd = NULL, *stop;
  const bool dstComplete = ARG1_COMPLETE(complete);
  const bool srcComplete = ARG2_COMPLETE(complete);
  bool dstFound, srcFound;
  // Retrieve both the destination and source buffer's bounds from the handles.
  if (!(dstFound = pool_find(dstPool, dst, dstBegin, dstEnd)) && dstComplete) {
    err << "Memory object not found in pool!\n";
    LOAD_STORE_VIOLATION(dst, dstPool, SRC_INFO_ARGS);
  }
  if (!(srcFound = pool_find(srcPool, src, srcBegin, srcEnd)) && srcComplete) {
    err << "Memory object not found in pool!\n";
    LOAD_STORE_VIOLATION(src, srcPool, SRC_INFO_ARGS);
  }
  if (srcFound) {
    // Calculate the maximum number of bytes to copy.
    srcSize = byte_range(src, srcEnd);
    // Get the position of the byte which terminates copying.
    stop = memchr(src, c, srcSize);
    // Get the number of bytes that will be copied over.
    size_t bytesToCopy = stop != NULL ? byte_range(src, stop) : n;
    if (bytesToCopy > srcSize) {
      err << "Cannot copy more bytes than the size of the source!\n";
      OOB_VIOLATION(src, srcPool, src, bytesToCopy, SRC_INFO_ARGS);
    }
    if (dstFound) {
      dstSize = byte_range(dst, dstEnd);
      if (bytesToCopy > dstSize) {
        err << "Cannot copy more bytes than the size of the destination!\n";
        WRITE_VIOLATION(dstBegin, dstPool, dstSize, bytesToCopy, SRC_INFO_ARGS);
      }
      const char *dstLimit = (char *) dst + bytesToCopy - 1;
      const char *srcLimit = (char *) src + bytesToCopy - 1;
      if (isOverlapped(dst, dstLimit, src, srcLimit)) { 
        err << "Input memory objects overlap each other!\n";
        C_LIBRARY_VIOLATION(dst, dstPool, "memccpy", SRC_INFO_ARGS);
      }
    }
  }
  return memccpy(dst, src, c, n);
}

//
// pool_memchr()
//
// See pool_memchr_debug().
//
void *
pool_memchr(DebugPoolTy *stringPool, 
            void *string, 
            int c, 
            size_t n,
            const uint8_t complete) {
  return pool_memchr_debug(stringPool, string, c, n,complete, DEFAULTS);
}

//
// Secure runtime wrapper function to replace memchr()
//
// Inputs:
//   stringPool  Pool handle for the string
//   string      Memory object
//   c           A byte value to be found
//   n           Number of bytes to search in
//   complete    Completeness bit vector
//   TAG         Tag information for debugging purposes
//   SRC_INFO    Source file and line number information for debugging purposes
//
// Returns:
//   This returns a pointer to the first location of c in the string, or NULL
//   if not found.
//
void *
pool_memchr_debug(DebugPoolTy *strPool, 
                  void *str,
                  int c, 
                  size_t n,
                  const uint8_t complete,
                  TAG,
                  SRC_INFO) {
  size_t strSize = 0;
  void *strBegin = str, *strEnd = NULL, *stop= NULL;
  const bool strComplete = ARG1_COMPLETE(complete);
  bool strFound;
  // Retrieve the memory buffer's boundaries from the pool.
  if (!(strFound = pool_find(strPool, str, strBegin, strEnd)) && strComplete) {
    err << "Memory object not found in pool!\n";
    LOAD_STORE_VIOLATION(str, strPool, SRC_INFO_ARGS);
  }
  // If the boundaries are found, determine if memchr() would read beyond them.
  if (strFound) {
    strSize = std::min(byte_range(str, strEnd), n);
    stop    = memchr(str, c, strSize);
    if (stop != NULL)
      return stop;
    else if (n > strSize) {
      err << "memchr() reads past the end of the memory object!\n";
      OOB_VIOLATION(str, strPool, str, n, SRC_INFO_ARGS);
    }
  }
  return memchr(str, c, n);
}

//
// pool_memcmp()
//
// See pool_memcmp_debug().
//
int
pool_memcmp(DebugPoolTy *s1p,
            DebugPoolTy *s2p, 
            void *s1, 
            void *s2,
            size_t num,
            const uint8_t complete) {
    return pool_memcmp_debug(s1p, s2p, s1, s2, num, complete, DEFAULTS);
}

//
// Secure runtime wrapper function to replace memcmp()
//
// Inputs:
//   str1Pool Pool handle for memory object1
//   str2Pool Pool handle for memory object1
//   num      Maximum number of bytes to compare
//   str1     Memory object 1 to be compared
//   str2     Memory object 2 to be compared
//   complete Completeness bit vector
//   TAG      Tag information for debugging purposes
//   SRC_INFO Source file and line number information for debugging purposes
//
// Returns:
//   This function returns 0 if the memory areas are identical or else the
//   difference between the first two bytes that are not identical.
//
int
pool_memcmp_debug(DebugPoolTy *s1Pool,
                  DebugPoolTy *s2Pool, 
                  void *s1, 
                  void *s2,
                  size_t num,
                  const uint8_t complete,
                  TAG,
                  SRC_INFO) {
  size_t s1Size = 0, s2Size = 0;
  void *s1Begin = s1, *s1End = NULL, *s2Begin = s2, *s2End = NULL;
  const bool s1Complete = ARG1_COMPLETE(complete);
  const bool s2Complete = ARG2_COMPLETE(complete);
  bool s1Found, s2Found;
  if (!(s1Found = pool_find(s1Pool, s1, s1Begin, s1End)) && s1Complete) {
    err << "Bytestring 1 not found in pool!\n";
    LOAD_STORE_VIOLATION(s1Begin, s1Pool, SRC_INFO_ARGS);
  }
  if (!(s2Found = pool_find(s2Pool, s2, s2Begin, s2End)) && s2Complete) {
    err << "Bytestring 2 not found in pool!\n";
    LOAD_STORE_VIOLATION(s2Begin, s2Pool, SRC_INFO_ARGS);
  }
  // These sizes are how far a read can continue safely.
  s1Size = s1Found ? byte_range(s1, s1End) : num;
  s2Size = s2Found ? byte_range(s2, s2End) : num;
  size_t p;
  const unsigned char *cs1 = (unsigned char *) s1;
  const unsigned char *cs2 = (unsigned char *) s2;
  // If we know the size of the memory objects, we can stop before we read out
  // of bounds.
  size_t stop = std::min(num, std::min(s1Size, s2Size));
  for (p = 0; p < stop; p++)
    if (cs1[p] != cs2[p])
      return cs1[p] - cs2[p];
  if (p == num)
    return 0;
  if (s1Found && p == s1Size) {
    err << "memcmp() reads beyond the end of bytestring 1!\n";
    OOB_VIOLATION(s1, s1Pool, s1, s1Size + 1, SRC_INFO_ARGS);
  }
  if (s2Found && p == s2Size) {
    err << "memcmp() reads beyond the end of bytestring 2!\n";
    OOB_VIOLATION(s2, s2Pool, s2, s2Size + 1, SRC_INFO_ARGS);
  }
  return memcmp(s1, s2, num);
}

//
// pool_memcpy()
//
// See pool_memcpy_debug().
//
void *
pool_memcpy(DebugPoolTy *dstPool, 
            DebugPoolTy *srcPool, 
            void *dst, 
            void *src, 
            size_t n,
            const uint8_t complete) {
  return pool_memcpy_debug(dstPool, srcPool, dst, src, n, complete, DEFAULTS);
}

//
// Secure runtime wrapper function to replace memcpy()
//
// Inputs:
//   dstPool  Pool handle for destination memory area
//   srcPool  Pool handle for source memory area
//   dst      Destination memory area
//   src      Source memory area
//   n        Maximum number of bytes to copy
//   complete Completeness bit vector
//   TAG      Tag information for debugging purposes
//   SRC_INFO Source file and line number information for debugging purposes
//
// Returns:
//  This function returns the value of dst.
//
void *
pool_memcpy_debug(DebugPoolTy *dstPool, 
                  DebugPoolTy *srcPool, 
                  void *dst, 
                  void *src, 
                  size_t n,
                  const uint8_t complete,
                  TAG,
                  SRC_INFO) {
  size_t dstSize = 0, srcSize = 0;
  void *dstBegin = dst, *dstEnd = NULL, *srcBegin = src, *srcEnd = NULL;
  const bool dstComplete = ARG1_COMPLETE(complete);
  const bool srcComplete = ARG2_COMPLETE(complete);
  bool dstFound = false, srcFound = false;
  // Retrieve both the destination and source buffers' bounds from the handles.
  if (!(dstFound = pool_find(dstPool, dst, dstBegin, dstEnd)) && dstComplete) {
    err << "Destination object not found in pool!\n";
    LOAD_STORE_VIOLATION(dst, dstPool, SRC_INFO_ARGS);
  }
  if (!(srcFound = pool_find(srcPool, src, srcBegin, srcEnd)) && srcComplete) {
    err << "Source object not found in pool!\n";
    LOAD_STORE_VIOLATION(src, srcPool, SRC_INFO_ARGS);
  }
  // Calculate the maximum number of bytes to copy.
  dstSize = byte_range(dst, dstEnd);
  srcSize = byte_range(src, srcEnd);
  if (srcFound && n > srcSize) {
    err << "memcpy() reads beyond the source object's boundaries!\n";
    OOB_VIOLATION(src, srcPool, src, n, SRC_INFO_ARGS);
  }
  if (dstFound && n > dstSize) {
    err << "memcpy() writes beyond the destination object's boundaries!\n";
    WRITE_VIOLATION(dst, dstPool, dstSize, n, SRC_INFO_ARGS);
  }
  if (dstFound && srcFound) {
    const char *srcLimit = (char *) src + n - 1;
    const char *dstLimit = (char *) dst + n - 1;
    if (isOverlapped(dst, dstLimit, src, srcLimit)) { 
      err << "Input memory objects overlap each other!\n";
      C_LIBRARY_VIOLATION(dst, dstPool, "memcpy", SRC_INFO_ARGS);
    }
  }
  return memcpy(dst, src, n);
}

//
// pool_memmove()
//
// See pool_memmove_debug().
//
void *
pool_memmove(DebugPoolTy *dstPool, 
             DebugPoolTy *srcPool, 
             void *dst, 
             void *src, 
             size_t n,
             const uint8_t complete) {
  return pool_memmove_debug(dstPool, srcPool, dst, src, n, complete, DEFAULTS);
}

//
// Secure runtime wrapper function to replace memmove()
//
// Inputs:
//   dstPool  Pool handle for destination memory area
//   srcPool  Pool handle for source memory area
//   dst      Destination memory area
//   src      Source memory area
//   n        Maximum number of bytes to copy
//   complete Completeness bit vector
//   TAG      Tag information for debugging purposes
//   SRC_INFO Source file and line number information for debugging purposes
// Returns:
//   This function returns the value of dst.
//
void *
pool_memmove_debug(DebugPoolTy *dstPool, 
                   DebugPoolTy *srcPool, 
                   void *dst, 
                   void *src, 
                   size_t n,
                   const uint8_t complete,
                   TAG,
                   SRC_INFO) {
  size_t dstSize = 0, srcSize = 0;
  void *dstBegin = dst, *dstEnd = NULL, *srcBegin = src, *srcEnd = NULL;
  const bool dstComplete = ARG1_COMPLETE(complete);
  const bool srcComplete = ARG2_COMPLETE(complete);
  bool dstFound, srcFound;
  // Retrieve both the destination and source buffers' bounds from the pools.
  if (!(dstFound = pool_find(dstPool, dst, dstBegin, dstEnd)) && dstComplete) {
    err << "Destination object not found in pool!\n";
    LOAD_STORE_VIOLATION(dst, dstPool, SRC_INFO_ARGS);
  }
  if (!(srcFound = pool_find(srcPool, src, srcBegin, srcEnd)) && srcComplete) {
    err << "Source object not found in pool!\n";
    LOAD_STORE_VIOLATION(src,srcPool, SRC_INFO_ARGS);
  }
  // Calculate the maximum number of bytes to copy safely.
  dstSize = byte_range(dst, dstEnd);
  srcSize = byte_range(src, srcEnd);
  if (srcFound && n > srcSize) {
    err << "memmove() reads beyond the end of the source bytestring!\n";
    OOB_VIOLATION(src, srcPool, src, n, SRC_INFO_ARGS);
  }
  if (dstFound && n > dstSize) {
    err << "memmove() write beyond the end of the destination bytestring!\n";
    OOB_VIOLATION(dst, dstPool, dst, n, SRC_INFO_ARGS);
  }
  // We don't need to check for overlap - memmove() already handles this.
  return memmove(dst, src, n);
}

//
// pool_memset()
//
// See pool_memset_debug().
//
void *pool_memset(DebugPoolTy *stringPool, 
                  void *string, 
                  int c, 
                  size_t n,
                  const uint8_t complete) {
  return pool_memset_debug(stringPool, string, c, n,complete, DEFAULTS);
}

//
// Secure runtime wrapper function to replace memset()
//
// Inputs:
//   sPool    Pool handle for the bytestring
//   s        Bytestring pointer
//   c        The byte to write
//   n        Number of bytes to be set
//   complete Completeness bit vector
//   TAG      Tag information for debugging purposes
//   SRC_INFO Source file and line number information for debugging purposes
// Returns:
//   This function returns the value of s.
//
void *
pool_memset_debug(DebugPoolTy *sPool, 
                  void *s, 
                  int c, 
                  size_t n,
                  const uint8_t complete,
                  TAG,
                  SRC_INFO) {
  size_t size = 0;
  void *sBegin = s, *sEnd = NULL;
  const bool sComplete = ARG1_COMPLETE(complete);
  bool sFound;
  // Retrive the object bounds.
  if (!(sFound = pool_find(sPool, s, sBegin, sEnd)) && sComplete) {
    err << "Memory object not found in pool!\n";
    LOAD_STORE_VIOLATION(s, sPool, SRC_INFO_ARGS);
  }
  size = byte_range(s, sEnd);
  // Check for writing out of bounds error.
  if (sFound && n > size) {
    err << "memset() writes beyond the end of the destination object!\n";
    WRITE_VIOLATION(s, sPool, size, n, SRC_INFO_ARGS);
  }
  return memset(s, c, n);
}

//
// pool_strcat()
//
// See pool_strcat_debug().
//
char *
pool_strcat(DebugPoolTy *dp,
            DebugPoolTy *sp,
            char *d,
            char *s,
            const unsigned char c) {
  return pool_strcat_debug(dp, sp, d, s, c, DEFAULTS);
}

//
// Secure runtime wrapper function to replace strcat()
//
// Appends the source string to the end of the destination string.
// Attempts to verify the following:
//  - source and destination pointers point to valid strings
//  - there is no overlap between source and destination strings
//  - the destination string's object has enough space to hold the
//    concatenation in memory.
//
// Inputs:
//   dp    Pool handle for destination string
//   sp    Pool handle for source string
//   d     Pointer to the destination string
//   s     Pointer to the source string
//   complete Completeness bit vector
//   TAG      Tag information for debugging purposes
//   SRC_INFO Source file and line number information for debugging purposes
// Returns:
//   This function returns a pointer to the destination string.
//
char *
pool_strcat_debug(DebugPoolTy *dstPool,
                  DebugPoolTy *srcPool,
                  char *dst,
                  char *src,
                  const uint8_t complete,
                  TAG,
                  SRC_INFO) {
  size_t srcLen = 0, dstLen = 0, maxLen, catLen;
  void *dstBegin = NULL, *dstEnd = NULL;
  void *srcBegin = NULL, *srcEnd = NULL;
  char *dstNulPosition;
  bool srcFound, dstFound, srcTerminated = false, dstTerminated = false;
  const bool srcComplete = ARG1_COMPLETE(complete);
  const bool dstComplete = ARG2_COMPLETE(complete);
  // Find the strings' memory objects in the pools.
  if (!(dstFound = pool_find(dstPool, dst, dstBegin, dstEnd)) && dstComplete) {
    err << "Destination string not found in pool\n";
    LOAD_STORE_VIOLATION(dstBegin, dstPool, SRC_INFO_ARGS);
  }
  if (!(srcFound = pool_find(srcPool, src, srcBegin, srcEnd)) && srcComplete) {
    err << "Source string not found in pool!\n";
    LOAD_STORE_VIOLATION(srcBegin, srcPool, SRC_INFO_ARGS);
  }
  // Check if both src and dst are terminated, if they were found in their pool.
  if (dstFound && !(dstTerminated = isTerminated(dst, dstEnd, dstLen))) {
    err << "Destination not terminated within bounds\n";
    C_LIBRARY_VIOLATION(dst, dstPool, "strcat", SRC_INFO_ARGS);
  }
  if (srcFound && !(srcTerminated = isTerminated(src, srcEnd, srcLen))) {
    err << "Source not terminated within bounds\n";
    C_LIBRARY_VIOLATION(src, srcPool, "strcat", SRC_INFO_ARGS);
  }
  // We assume an object that is not complete and not found is valid.
  // So get its length.
  if (!srcFound && !srcComplete) {
    srcTerminated = true;
    srcLen = strlen(src);
  }
  if (!dstFound && !dstComplete) {
    dstTerminated = true;
    dstLen = strlen(dst);
  }
  // The remainder of the checks require the string length to be known.
  if (dstTerminated && srcTerminated) {
    maxLen = byte_range(dst, dstEnd) - 1;
    catLen = srcLen + dstLen;
    // Do the check for if concatenation writes out of bounds.
    if (catLen > maxLen) {
      err << "Concatenation violated destination bounds!\n";
      WRITE_VIOLATION(dstBegin, dstPool, maxLen + 1, catLen + 1, SRC_INFO_ARGS);
    }
    // Overlap occurs exactly when they share the same nul terminator in memory.
    if (&dst[dstLen] == &src[srcLen]) {
      err << "Concatenating overlapping strings is undefined\n";
      C_LIBRARY_VIOLATION(dst, dstPool, "strcat", SRC_INFO_ARGS);
    }
    // Append at the end of dst so concatenation doesn't have to scan dst again.
    dstNulPosition = &dst[dstLen];
    strncat(dstNulPosition, src, srcLen);
    return dst;
  }
  else
    return strcat(dst, src);
}

//
// pool_strchr()
//
// This is the non-debug version of pool_strchr_debug().
//
//
char *
pool_strchr(DebugPoolTy *sp, char *s, int c, const uint8_t complete) {
  return pool_strchr_debug(sp, s, c, complete, DEFAULTS);
}

//
// Secure runtime wrapper function to replace strchr()
//
// Returns pointer to the first instance of the given character in the string,
// or NULL if not found.
//
// Ensures the following:
//  - string argument points to a valid string
//
// Inputs:
//   sPool     Pool handle for string
//   s         String pointer
//   c         Character to find
//   complete Completeness bit vector
//   TAG      Tag information for debugging purposes
//   SRC_INFO Source file and line number information for debugging purposes
// Returns:
//   This function returns a pointer to the first instance of the given
//   character in the string, or NULL if not found.
//
char *
pool_strchr_debug(DebugPoolTy *sPool,
                  char *s,
                  int c,
                  const uint8_t complete,
                  TAG,
                  SRC_INFO) {
  validStringCheck(s, sPool, ARG1_COMPLETE(complete), "strchr", SRC_INFO_ARGS);
  return strchr(s, c);
}

//
// pool_strcmp()
//
// See pool_strcmp_debug().
//
int
pool_strcmp(DebugPoolTy *s1p,
            DebugPoolTy *s2p, 
            char *s1, 
            char *s2,
            const uint8_t complete){
  return pool_strcmp_debug(s1p, s2p, s1, s2, complete, DEFAULTS);
}

//
// Secure runtime wrapper function to replace strcmp()
//
// Inputs:
//   s1Pool   Pool handle for str1
//   s2Pool   Pool handle for str2
//   s1       C string to be compared
//   s2       C string to be compared
//   complete Completeness bit vector
//   TAG      Tag information for debugging purposes
//   SRC_INFO Source file and line number information for debugging purposes
// Returns:
//  This function returns a negative, zero, or positive integer depending on
//  whether s1 < s2, s1 = s2, or s1 > s2.
//
int
pool_strcmp_debug(DebugPoolTy *s1Pool,
                  DebugPoolTy *s2Pool, 
                  char *s1, 
                  char *s2,
                  const uint8_t complete,
                  TAG,
                  SRC_INFO) {
  const bool s1Complete = ARG1_COMPLETE(complete);
  const bool s2Complete = ARG2_COMPLETE(complete);
  validStringCheck(s1, s1Pool, s1Complete, "strcmp", SRC_INFO_ARGS);
  validStringCheck(s2, s2Pool, s2Complete, "strcmp", SRC_INFO_ARGS);
  return strcmp(s1, s2);
}

//
// pool_strcoll()
//
// See pool_strcoll_debug().
//
int
pool_strcoll(DebugPoolTy *s1p,
             DebugPoolTy *s2p, 
             char *s1, 
             char *s2,
             const uint8_t complete){
  return pool_strcoll_debug(s1p, s2p, s1, s2, complete, DEFAULTS);
}

//
// Secure runtime wrapper function to replace strcoll()
//
// Inputs:
//   s1Pool   Pool handle for str1
//   s2Pool   Pool handle for str2
//   s1       C string to be compared
//   s2       C string to be compared
//   complete Completeness bit vector
//   TAG      Tag information for debugging purposes
//   SRC_INFO Source file and line number information for debugging purposes
// Returns:
//  This function returns a negative, zero, or positive integer depending on
//  whether s1 < s2, s1 = s2, or s1 > s2, in the ordering described by the
//  value of the LC_COLLATE category of the current locale.
//
int
pool_strcoll_debug(DebugPoolTy *s1Pool,
                   DebugPoolTy *s2Pool, 
                   char *s1, 
                   char *s2,
                   const uint8_t complete,
                   TAG,
                   SRC_INFO) {
  const bool s1Complete = ARG1_COMPLETE(complete);
  const bool s2Complete = ARG2_COMPLETE(complete);
  validStringCheck(s1, s1Pool, s1Complete, "strcoll", SRC_INFO_ARGS);
  validStringCheck(s2, s2Pool, s2Complete, "strcoll", SRC_INFO_ARGS);
  return strcoll(s1, s2);
}

//
// pool_strcpy()
//
// See pool_strcpy_debug().
//
char *
pool_strcpy(DebugPoolTy *dstPool, 
            DebugPoolTy *srcPool, 
            char *dst, 
            char *src, 
            const uint8_t complete) {
  return pool_strcpy_debug(dstPool, srcPool, dst, src, complete, DEFAULTS);
}

//
// Secure runtime wrapper function to replace strcpy()
//
// Inputs:
//   dstPool  Pool handle for destination buffer
//   srcPool  Pool handle for source string
//   dst      Destination string pointer
//   src      Source string pointer
//   complete Completeness bit vector
//   TAG      Tag information for debugging purposes
//   SRC_INFO Source file and line number information for debugging purposes
// Returns:
//   This function returns the destination string pointer.
//
char *
pool_strcpy_debug(DebugPoolTy *dstPool, 
                  DebugPoolTy *srcPool, 
                  char *dst, 
                  char *src, 
                  const uint8_t complete, 
                  TAG,
                  SRC_INFO) {
  size_t dstMax = 0, srcLen = 0;
  void *dstBegin = dst, *dstEnd = NULL, *srcBegin = src, *srcEnd = NULL;
  const bool dstComplete = ARG1_COMPLETE(complete);
  const bool srcComplete = ARG2_COMPLETE(complete);
  bool dstFound, srcFound, srcTerminated;
  // Retrieve both the destination and source buffer's bounds from the pools.
  if (!(dstFound = pool_find(dstPool, dst, dstBegin, dstEnd)) && dstComplete) {
    err << "Memory object not found in pool!\n";
    LOAD_STORE_VIOLATION(dst, dstPool, SRC_INFO_ARGS);
  }
  if (!(srcFound = pool_find(srcPool, src, srcBegin, srcEnd)) && srcComplete) {
    err << "Memory object not found in pool!\n";
    LOAD_STORE_VIOLATION(src,srcPool, SRC_INFO_ARGS);
  }
  // Check for source termination.
  if (srcFound && !(srcTerminated = isTerminated(src, srcEnd, srcLen))) {
    err << "Source string is not terminated within object bounds!\n";
    C_LIBRARY_VIOLATION(src, srcPool, "strcpy", SRC_INFO_ARGS);
  }
  if (dstFound) {
    // Assume an incomplete and not found object is valid.
    if (!srcFound && !srcComplete) {
      srcTerminated = true;
      srcLen = strlen(src);
    }
    // The remainder of the checks require us to know the length of src.
    if (srcTerminated) {
      dstMax = byte_range(dst, dstEnd) - 1;
      // Check for writing out of bounds errors.
      if (srcLen > dstMax) {
        err << "strcpy() writes beyond the end of the destination object!\n";
        WRITE_VIOLATION(dst, dstPool, dstMax + 1, srcLen + 1, SRC_INFO_ARGS);
      }
      // Check for overlap.
      void *dstEdge = srcLen + (char *) dst;
      void *srcEdge = srcLen + (char *) src;
      if (isOverlapped(dst, dstEdge, src, srcEdge)) {
        err << "Memory objects in call to strcpy() overlap each other!\n";
        C_LIBRARY_VIOLATION(dst, dstPool, "strcpy", SRC_INFO_ARGS);
      }
    }
  }
  return strcpy(dst, src);
}

//
// pool_strcspn()
//
// See pool_strcspn_debug().
//
size_t pool_strcspn(DebugPoolTy *s1p,
                    DebugPoolTy *s2p, 
                    char *s1, 
                    char *s2,
                    const uint8_t complete) {
  return pool_strcspn_debug(s1p, s2p, s1, s2, complete, DEFAULTS);
}

//
// Secure runtime wrapper function to replace strcspn()
//
// Inputs:
//   str1Pool Pool handle for str1
//   str2Pool Pool handle for str2
//   str1     C string to be compared
//   str2     C string to be compared
//   complete Completeness bit vector
//   TAG      Tag information for debugging purposes
//   SRC_INFO Source file and line number information for debugging purposes
// Returns:
//   This function returns the length of the initial portion of str1 that does
//   not contain any character from str2.
//
size_t
pool_strcspn_debug(DebugPoolTy *str1Pool,
                   DebugPoolTy *str2Pool, 
                   char *str1, 
                   char *str2,
                   const uint8_t complete,
                   TAG,
                   SRC_INFO) {
  const bool str1Complete = ARG1_COMPLETE(complete);
  const bool str2Complete = ARG2_COMPLETE(complete);
  validStringCheck(str1, str1Pool, str1Complete, "strcspn", SRC_INFO_ARGS);
  validStringCheck(str2, str2Pool, str2Complete, "strcspn", SRC_INFO_ARGS);
  return strcspn(str1, str2);
}

//
// pool_strlen()
//
// See pool_strlen_debug().
//
size_t
pool_strlen(DebugPoolTy *stringPool,
            char *string,
            const uint8_t complete) {
  return pool_strlen_debug(stringPool, string, complete, DEFAULTS);
}

//
// Secure runtime wrapper function to replace strlen()
//
// Inputs:
//   strPool  Pool handle for the string
//   str      String pointer
//   complete Completeness bit vector
//   TAG      Tag information for debugging purposes
//   SRC_INFO Source file and line number information for debugging purposes
// Returns:
//   This function returns the length of the input string.
//
size_t
pool_strlen_debug(DebugPoolTy *strPool, 
                  char *str, 
                  const uint8_t complete, 
                  TAG, 
                  SRC_INFO) {
  const bool strComplete = ARG1_COMPLETE(complete);
  bool strFound;
  size_t len = 0;
  void *strBegin = 0, *strEnd = 0;
  if (!(strFound = pool_find(strPool, str, strBegin, strEnd)) && strComplete) {
    err << "Object for string not found in pool!\n";
    LOAD_STORE_VIOLATION(str, strPool, SRC_INFO_ARGS);
  }
  if (strFound) {
    if (!isTerminated(str, strEnd, len)) {
      err << "Input string not terminated within object boundaries!\n";
      C_LIBRARY_VIOLATION(str, strPool, "strlen", SRC_INFO_ARGS);
    }
    else
      return len;
  }
  return strlen(str);
}

//
// pool_strncat()
//
// See pool_strncat_debug().
//
char *
pool_strncat(DebugPoolTy *dstPool,
             DebugPoolTy *srcPool,
             char *dst,
             char *src,
             size_t n,
             const uint8_t complete) {
  return pool_strncat_debug(dstPool, srcPool, dst, src, n, complete, DEFAULTS);
}

//
// Secure runtime wrapper function to replace strncat()
//
// Appends at most n characters of src onto the end of the string dst
// and then adds a nul terminator.
//
// Checks for the following:
//  - src and dst are non-null
//  - dst is terminated
//  - dst has enough space to hold the whole concatention
//  - src and dst do not overlap
//  - if src is unterminated, the first n characters of src fall within
//    the boundaries of src.
//
// Inputs:
//   dstPool  Pool handle for destination string
//   srcPool  Pool handle for source string
//   dst      Destination string pointer
//   src      Source string pointer
//   n        Number of characters to copy over
//   complete Completeness bit vector
//   TAG      Tag information for debugging purposes
//   SRC_INFO Source file and line number information for debugging purposes
// Returns:
//   This function returns a pointer to the destination buffer.
//
char *
pool_strncat_debug(DebugPoolTy *dstPool,
                   DebugPoolTy *srcPool,
                   char *dst,
                   char *src,
                   size_t n,
                   const uint8_t complete,
                   TAG,
                   SRC_INFO) {
  void *dstBegin = NULL, *dstEnd = NULL;
  void *srcBegin = NULL, *srcEnd = NULL;
  size_t dstLen = 0, srcLen = 0, maxLen, catLen, srcAmt;
  char *dstNulPosition;
  bool srcFound, dstFound, srcTerminated = false, dstTerminated;
  const bool srcComplete = ARG1_COMPLETE(complete);
  const bool dstComplete = ARG2_COMPLETE(complete);
  // Retrieve destination and source string memory objects from pool.
  if (!(dstFound = pool_find(dstPool, dst, dstBegin, dstEnd)) && dstComplete) {
    err << "Destination string not found in pool!\n";  
    LOAD_STORE_VIOLATION(dstBegin, dstPool, SRC_INFO_ARGS);
  }
  if (!(srcFound = pool_find(srcPool, src, srcBegin, srcEnd)) && srcComplete) {
    err << "Source string not found in pool!\n";  
    LOAD_STORE_VIOLATION(srcBegin, srcPool, SRC_INFO_ARGS);
  }
  // Check if dst is nul terminated.
  if (dstFound && !(dstTerminated = isTerminated(dst, dstEnd, dstLen))) {
    err << "String not terminated within bounds\n";
    C_LIBRARY_VIOLATION(dst, dstPool, "strncat", SRC_INFO_ARGS);
  }
  // According to POSIX, src doesn't have to be nul-terminated.
  // If it isn't, ensure strncat that doesn't read beyond the bounds of src.
  if (srcFound) {
    srcTerminated = isTerminated(src, srcEnd, srcLen);
  } else if (!srcFound && !srcComplete) {
    srcLen = _strnlen(src, n);
    srcTerminated = srcLen < n;
  }
  // Check if the number of bytes in source is < the number of bytes we have to
  // copy.
  // This will result in reading out of bounds.
  if (srcFound && !srcTerminated && byte_range(src, srcEnd) < n) {
    err << "strncat() reads beyond the boundaries of the source object!\n";
    OOB_VIOLATION(src, srcPool, src, srcLen, SRC_INFO_ARGS);
  }
  // The remaining checks require us to know the lengths of the destination and
  // the object boundaries of the source string.
  if (srcFound && dstTerminated) {
    // Determine the amount of characters to be copied over from src.
    // If the string is terminated, this is the smaller of n or the length of
    // src.
    // Otherwise this is n.
    srcAmt = srcTerminated ? std::min(srcLen, n) : n;
    // Check for undefined behavior due to overlapping objects.
    // Overlap occurs when the characters to be copied from src
    // end inside the dst string. &src[srcAmt] represents one past the end of
    // what will be copied over.
    if (dst < &src[srcAmt] && &src[srcAmt] <= &dst[dstLen]) {
      err << "Concatenating overlapping objects is undefined\n";
      C_LIBRARY_VIOLATION(dst, dstPool, "strncat", SRC_INFO_ARGS);
    }
    // maxLen is the maximum length string dst can hold without overflowing.
    maxLen = byte_range(dst, dstEnd) - 1;
    // catLen is the length of the string resulting from the concatenation.
    catLen = srcAmt + dstLen;
    // Check if the copy operation would go beyong the bounds of dst.
    if (catLen > maxLen) {
      err << "Concatenation violated destination bounds!\n";
      WRITE_VIOLATION(dst, dstPool, 1+maxLen, 1+catLen, SRC_INFO_ARGS);
    }
    // Start concatenation the end of dst so strncat() doesn't have to scan dst
    // all over again.
    dstNulPosition = &dst[dstLen];
    strncat(dstNulPosition, src, srcAmt);
    // strncat() returns the original destination string.
    return dst;
  }
  else
    return strncat(dst, src, n);
}

//
// pool_strncmp()
//
// See pool_strncmp_debug().
//
int
pool_strncmp(DebugPoolTy *s1p,
             DebugPoolTy *s2p, 
             char *s1, 
             char *s2,
             size_t num,
             const uint8_t complete){
  return pool_strncmp_debug(s1p,s2p,s1,s2,num,complete, DEFAULTS);
}

//
// Secure runtime wrapper function to replace strncmp()
//
// Inputs:
//   s1Pool Pool handle for string1
//   s2Pool Pool handle for string2
//   n      Maximum number of chars to compare
//   s1     string1 to be compared
//   s2     string2 to be compared
//   complete Completeness bit vector
//   TAG      Tag information for debugging purposes
//   SRC_INFO Source file and line number information for debugging purposes
// Returns:
//   This function returns a negative, zero, or positive integer depending on if
//   s1 < s2, s1 = s2, or s1 > s2 in the first n characters.
//
int
pool_strncmp_debug(DebugPoolTy *s1Pool,
                   DebugPoolTy *s2Pool, 
                   char *s1, 
                   char *s2,
                   size_t n,
                   const uint8_t complete,
                   TAG,
                   SRC_INFO) {  
  size_t s1Size = 0, s2Size = 0;
  void *s1Begin = s1, *s1End = NULL, *s2Begin = s2, *s2End = NULL;
  const bool s1Complete = ARG1_COMPLETE(complete);
  const bool s2Complete = ARG2_COMPLETE(complete);
  bool s1Found, s2Found;
  // Find the objects in their pools.
  if (!(s1Found = pool_find(s1Pool, s1, s1Begin, s1End)) && s1Complete) {
    err << "String 1 not found in pool!\n";
    LOAD_STORE_VIOLATION(s1Begin, s1Pool, SRC_INFO_ARGS);
  }
  if (!(s2Found = pool_find(s2Pool, s2, s2Begin, s2End)) && s2Complete) {
    err << "String 2 not found in pool!\n";
    LOAD_STORE_VIOLATION(s1Begin, s1Pool, SRC_INFO_ARGS);
  }
  // These sizes represent the 'safe' range to read.
  s1Size = s1Found ? byte_range(s1, s1End) : n;
  s2Size = s2Found ? byte_range(s2, s2End) : n;
  size_t stop = std::min(n, std::min(s1Size, s2Size));
  size_t p;
  // Comparison is done using unsigned characters.
    const unsigned char *cs1 = (const unsigned char *) s1;
  const unsigned char *cs2 = (const unsigned char *) s2;
  for (p = 0; p < stop; p++) {
    if (cs1[p] != cs2[p])
      return cs1[p] - cs2[p];
    // We're comparing strings, so a nul terminator indicates we're done. 
    else if (cs1[p] == 0)
      return 0;
  }
  if (p == n)
    return 0;
  if (s1Found && p == s1Size) {
    err << "strncmp() reads beyond the end of string 1!\n";
    OOB_VIOLATION(s1, s1Pool, s1, s1Size + 1, SRC_INFO_ARGS);
  }
  if (s2Found && p == s2Size) {
    err << "strncmp() reads beyond the end of string 2!\n";
    OOB_VIOLATION(s2, s2Pool, s2, s2Size + 1, SRC_INFO_ARGS);
  }
  return strncmp(s1, s2, n);
}

//
// pool_strncpy()
//
// See pool_strncpy_debug().
//
char *
pool_strncpy(DebugPoolTy *dstPool,
             DebugPoolTy *srcPool,
             char *dst,
             char *src,
             size_t n,
             const uint8_t complete) {
  return pool_strncpy_debug(dstPool, srcPool, dst, src, n, complete, DEFAULTS);
}

//
// Secure runtime wrapper function to replace strncpy()
//
// Copies exactly n bytes to dst, which are read from src until a nul
// terminator is encountered. If len(src) < n then pads the rest of the write
// with zeroes. If len(src) >= n then the copying will be truncated and
// consequently no nul terminator will be appended.
//
// Inputs:
//   dstPool  Pool handle for destination buffer
//   srcPool  Pool handle for source string
//   dst      Destination string pointer
//   src      Source string pointer
//   n        Maximum number of bytes to copy
//   complete Completeness bit vector
//   TAG      Tag information for debugging purposes
//   SRC_INFO Source file and line number information for debugging purposes
// Returns:
//   This function returns the value of the destination string pointer.
//
char *
pool_strncpy_debug(DebugPoolTy *dstPool,
                   DebugPoolTy *srcPool,
                   char *dst, 
                   char *src,
                   size_t n, 
                   const uint8_t complete,
                   TAG,
                   SRC_INFO) {
  size_t dstSize = 0, srcSize = 0, srcLen = 0;
  void *dstBegin = dst, *dstEnd = NULL, *srcBegin = src, *srcEnd = NULL;
  const bool dstComplete = ARG1_COMPLETE(complete);
  const bool srcComplete = ARG2_COMPLETE(complete);
  bool dstFound, srcFound;
  // Retrieve both the destination and source object bounds from the pools.
  if (!(dstFound = pool_find(dstPool, dst, dstBegin, dstEnd)) && dstComplete) {
    err << "Memory object not found in pool!\n";
    LOAD_STORE_VIOLATION(dst, dstPool, SRC_INFO_ARGS);
  }
  if (!(srcFound = pool_find(srcPool, src, srcBegin, srcEnd)) && srcComplete) {
    err << "Memory object not found in pool!\n";
    LOAD_STORE_VIOLATION(src, srcPool, SRC_INFO_ARGS);
  }
  if (srcFound) {
    srcSize = byte_range(src, srcEnd);
    // Check if src is read out of bounds. This happens when n > the object
    // size of src, and src is not terminated before the end of the object.
    srcLen = _strnlen(src, srcSize);
    if (n > srcSize && srcLen == srcSize) {
        err << "strncpy() reads source string out of bounds!\n";
        OOB_VIOLATION(src, srcPool, src, srcSize + 1, SRC_INFO_ARGS);
      } else {
        // Check for overlap. This check doesn't work when the previous
        // condition is true, which is why it is in the else clause.
        // This is the amount of characters actually read from src.
        size_t srcRead = std::min(n, 1 + srcLen);
        void *dstEdge = (char *) dst + srcRead - 1;
        void *srcEdge = (char *) src + srcRead - 1;
        if (isOverlapped(dst, dstEdge, src, srcEdge)) {
          err << "The objects passed to strncpy() overlap!\n";
          C_LIBRARY_VIOLATION(dst, dstPool, "strncpy()", SRC_INFO_ARGS);
        }
      }
  }
  if (dstFound) {
    dstSize = byte_range(dst, dstEnd);
    if (dstSize < n) {
      err << "strncpy() writes beyond end of destination object!\n";
      WRITE_VIOLATION(dst, dstPool, dstSize, n, SRC_INFO_ARGS);
    }
  }
  return strncpy(dst, src, n);
}

//
// pool_strpbrk()
//
// See pool_strpbrk_debug().
//
char *
pool_strpbrk(DebugPoolTy *sp,
             DebugPoolTy *ap,
             char *s,
             char *a,
             const uint8_t complete) {
  return pool_strpbrk_debug(sp, ap, s, a, complete, DEFAULTS);
}

//
// Secure runtime wrapper function to replace strpbrk()
//
// Searches for the first instance in s of any character in a, and returns a
// pointer to that instance, or returns NULL if no instance was found.
//
// Attempts to verify that both s and a are valid strings terminated within
// their memory objects' boundaries.
//
// Inputs:
//   sPool    Pool handle for source string
//   aPool    Pool handle for accept string
//   s        String pointer
//   a        Pointer to string of characters to find
//   complete Completeness bit vector
//   TAG      Tag information for debugging purposes
//   SRC_INFO Source file and line number information for debugging purposes
// Returns:
//   This function returns a pointer to the first instance in s of some
//   character in a, or NULL if none was found.
//
char *
pool_strpbrk_debug(DebugPoolTy *sPool,
                   DebugPoolTy *aPool,
                   char *s,
                   char *a,
                   const uint8_t complete,
                   TAG,
                   SRC_INFO) {
  const bool sComplete = ARG1_COMPLETE(complete);
  const bool aComplete = ARG2_COMPLETE(complete);
  validStringCheck(s, sPool, sComplete, "strpbrk", SRC_INFO_ARGS);
  validStringCheck(a, aPool, aComplete, "strpbrk", SRC_INFO_ARGS);
  return strpbrk(s, a);
}

//
// pool_strrchr()
//
// See pool_strrchr_debug().
//
char *
pool_strrchr(DebugPoolTy *sPool,
             char *s,
             int c,
             const uint8_t complete) {
  return pool_strrchr_debug(sPool, s, c, complete, DEFAULTS);
}

//
// Secure runtime wrapper function to replace strrchr()
//
// Inputs:
//   sPool    Pool handle for string
//   s        String pointer
//   c        Character to find
//   complete Completeness bit vector
//   TAG      Tag information for debugging purposes
//   SRC_INFO Source file and line number information for debugging purposes
// Returns:
//   This function returns a pointer to the last instance of c in the string s,
//   or NULL if none was found.
//
char *
pool_strrchr_debug(DebugPoolTy *sPool,
                   char *s,
                   int c,
                   const uint8_t complete,
                   TAG,
                   SRC_INFO) {
  const bool sComplete = ARG1_COMPLETE(complete);
  validStringCheck(s, sPool, sComplete, "strrchr", SRC_INFO_ARGS);
  return strrchr(s, c);
}

//
// pool_strspn()
//
// See pool_strspn_debug().
//
size_t
pool_strspn(DebugPoolTy *s1p,
            DebugPoolTy *s2p, 
            char *s1, 
            char *s2,
            const uint8_t complete) {
  return pool_strspn_debug(s1p, s2p, s1, s2, complete, DEFAULTS);
}

//
// Secure runtime wrapper function to replace strspn()
//
// Inputs:
//   str1Pool Pool handle for str1
//   str2Pool Pool handle for str2
//   str1     A string to scan
//   str2     A string of allowed characters
//   complete Completeness bit vector
//   TAG      Tag information for debugging purposes
//   SRC_INFO Source file and line number information for debugging purposes
// Returns:
//   This function returns the length of the initial portion of str1 which
//   contains characters only from str2.
//
size_t
pool_strspn_debug(DebugPoolTy *str1Pool,
                  DebugPoolTy *str2Pool, 
                  char *str1, 
                  char *str2,
                  const uint8_t complete,
                  TAG,
                  SRC_INFO) {
  const bool str1Complete = ARG1_COMPLETE(complete);
  const bool str2Complete = ARG2_COMPLETE(complete);
  validStringCheck(str1, str1Pool, str1Complete, "strspn", SRC_INFO_ARGS);
  validStringCheck(str2, str2Pool, str2Complete, "strspn", SRC_INFO_ARGS);
  return strspn(str1, str2);
}

//
// pool_strstr()
//
// See pool_strstr_debug().
//
//
char *
pool_strstr(DebugPoolTy *s1Pool,
            DebugPoolTy *s2Pool,
            char *s1,
            char *s2,
            const uint8_t complete) {
  return pool_strstr_debug(s1Pool, s2Pool, s1, s2, complete, DEFAULTS);
}

//
// Secure runtime wrapper function to replace strstr()
//
// Searches for the first occurence of the substring s2 in s1.
// Returns a pointer to the discovered substring, or NULL if not found.
//
// Attempts to verify that s1 and s2 are valid strings terminated within
// their memory objects' boundaries.
//
// Inputs:
//   s1Pool   Pool handle for s1
//   s2Pool   Pool handle for s2
//   s1       Pointer to string to be searched
//   s2       Pointer to substring to search for
//   complete Completeness bit vector
//   TAG      Tag information for debugging purposes
//   SRC_INFO Source file and line number information for debugging purposes
// Returns:
//   This function returns a pointer to the first instance of s2 in s1, or NULL
//   if none exists.
//
char *
pool_strstr_debug(DebugPoolTy *s1Pool,
                  DebugPoolTy *s2Pool,
                  char *s1,
                  char *s2,
                  const uint8_t complete,
                  TAG,
                  SRC_INFO) {
  const bool s1Complete = ARG1_COMPLETE(complete);
  const bool s2Complete = ARG2_COMPLETE(complete);
  validStringCheck(s1, s1Pool, s1Complete, "strstr", SRC_INFO_ARGS);
  validStringCheck(s2, s2Pool, s2Complete, "strstr", SRC_INFO_ARGS);
  return strstr(s1, s2);
}

//
// pool_strxfrm()
//
// See pool_strxfrm_debug().
//
size_t
pool_strxfrm(DebugPoolTy *dPool,
             DebugPoolTy *sPool,
             char *d,
             char *s,
             size_t n,
             const uint8_t complete) {
  return pool_strxfrm_debug(dPool, sPool, d, s, n, complete, DEFAULTS);
}

//
// Secure runtime wrapper function to replace strxfrm()
//
// Uses the current locale information to convert the first n characters of the
// source string into a format suitable for usage with strcmp().
//
// Inputs:
//   dPool    Pool handle for destionation
//   sPool    Pool handle for source string
//   d        Pointer to destination buffer
//   s        Pointer to source string
//   n        Number of characters to convert
//   complete Completeness bit vector
//   TAG      Tag information for debugging purposes
//   SRC_INFO Source file and line number information for debugging purposes
// Returns:
//   This function returns the length of the transformed string.
//
size_t
pool_strxfrm_debug(DebugPoolTy *dPool,
                   DebugPoolTy *sPool,
                   char *d,
                   char *s,
                   size_t n,
                   const uint8_t complete,
                   TAG,
                   SRC_INFO) {
  const bool dComplete = ARG1_COMPLETE(complete);
  const bool sComplete = ARG2_COMPLETE(complete);
  bool dFound;
  void *dStart, *dEnd = NULL;
  // Retrive the memory object boundaries from the pools.
  if (!(dFound = pool_find(dPool, d, dStart, dEnd)) && dComplete) {
    err << "Destination object not found in pool!\n";
    LOAD_STORE_VIOLATION(d, dPool, SRC_INFO_ARGS);
  }
  //
  // Check only for string termination of s, because we don't know how much of
  // s will be read.
  //
  validStringCheck(s, sPool, sComplete, "strxfrm", SRC_INFO_ARGS);
  //
  // Check if we write out if bounds.
  //
  if (dFound && n > 0) {
    // Call strxfrm(NULL, s, 0) to discover the length of the transformed
    // string.
    size_t xfrmLen = std::min(strxfrm(NULL, s, 0), n - 1);
    size_t dSize   = byte_range(d, dEnd);
    if (xfrmLen + 1 > dSize) {
      err << "strxfrm() writes past the end of the destination object!\n";
      WRITE_VIOLATION(d, dPool, dSize, xfrmLen + 1, SRC_INFO_ARGS);
    }
  }
  return strxfrm(d, s, n);
}

#ifdef HAVE_MEMPCPY
//
// pool_mempcpy()
//
// See pool_mempcpy_debug().
//
void *
pool_mempcpy(DebugPoolTy *dstPool, 
             DebugPoolTy *srcPool, 
             void *dst, 
             void *src, 
             size_t n,
             const uint8_t complete) {
  return pool_mempcpy_debug(dstPool, srcPool, dst, src, n, complete, DEFAULTS);
}

//
// Secure runtime wrapper function to replace mempcpy()
//
// This function is identical to memcpy(), except it returns a pointer to the
// byte right after the first n bytes of the destination.
//
// Inputs:
//   dstPool  Pool handle for destination memory area
//   srcPool  Pool handle for source memory area
//   dst      Destination memory area
//   src      Source memory area
//   n        Maximum number of bytes to copy
//   complete Completeness bit vector
//   TAG      Tag information for debugging purposes
//   SRC_INFO Source file and line number information for debugging purposes
// Returns:
//   This function returns the value of &((char *) dst)[n].
//
void *
pool_mempcpy_debug(DebugPoolTy *dstPool, 
                   DebugPoolTy *srcPool, 
                   void *dst, 
                   void *src, 
                   size_t n,
                   const uint8_t complete,
                   TAG,
                   SRC_INFO) {
  size_t dstSize = 0, srcSize = 0;
  void *dstBegin = dst, *dstEnd = NULL, *srcBegin = src, *srcEnd = NULL;
  const bool dstComplete = ARG1_COMPLETE(complete);
  const bool srcComplete = ARG2_COMPLETE(complete);
  bool dstFound, srcFound;
  // Retrieve both the destination and source buffer bounds from the handles.
  if (!(dstFound = pool_find(dstPool, dst, dstBegin, dstEnd)) && dstComplete) {
    err << "Memory object not found in pool!\n";
    LOAD_STORE_VIOLATION(dst, dstPool, SRC_INFO_ARGS);
  }
  if (!(srcFound = pool_find(srcPool, src, srcBegin, srcEnd)) && srcComplete) {
    err << "Memory object not found in pool!\n";
    LOAD_STORE_VIOLATION(src,srcPool, SRC_INFO_ARGS);
  }
  // Check 1) if we write or read past the end of the memory objects, and 2) if
  // the memory areas overlap.
  dstSize = byte_range(dst, dstEnd);
  srcSize = byte_range(src, srcEnd);
  if (dstFound && n > dstSize) {
    err << "mempcpy() writes past the end of the destination object!\n";
    WRITE_VIOLATION(dst, dstPool, dstSize, n, SRC_INFO_ARGS);
  }
  if (srcFound && n > srcSize) {
    err << "mempcpy() reads past the end of the source object!\n";
    OOB_VIOLATION(src, srcPool, src, n, SRC_INFO_ARGS);
  }
#if 0
  if (dstFound && srcFound) {
    void *dstEdge = (char *) dst + n - 1;
    void *srcEdge = (char *) dst + n - 1;
    if (isOverlapped(dst, dstEdge, src, srcEdge)) {
      err << "Inputs to mempcpy() overlap!\n";
      C_LIBRARY_VIOLATION(dst, dstPool, "mempcpy", SRC_INFO_ARGS);
    }
  }
#endif
  return mempcpy(dst, src, n);
}
#endif

#ifdef HAVE_STRCASESTR
//
// pool_strcasestr()
//
// See pool_strcasestr_debug().
//
char *
pool_strcasestr(DebugPoolTy *s1Pool,
                DebugPoolTy *s2Pool,
                char *s1,
                char *s2,
                const uint8_t complete) {
  return pool_strcasestr_debug(s1Pool, s2Pool, s1, s2, complete, DEFAULTS);
}


//
// Secure runtime wrapper function to replace strcasestr()
//
// Searches for the first occurence of the substring s2 in s1, case
// insensitively.
// Returns a pointer to the discovered substring, or NULL if not found.
//
// Attempts to verify that s1 and s2 are valid strings terminated within
// their memory objects' boundaries.
//
// Inputs:
//   s1Pool   Pool handle for s1
//   s2Pool   Pool handle for s2
//   s1       Pointer to string to be searched
//   s2       Pointer to substring to search for
//   complete Completeness bit vector
//   TAG      Tag information for debugging purposes
//   SRC_INFO Source file and line number information for debugging purposes
// Returns:
//   Returns a pointer to the start of the first case-insensitive occurence of
//   s2 in s1, or NULL if not found.
//
char *
pool_strcasestr_debug(DebugPoolTy *s1Pool,
                      DebugPoolTy *s2Pool,
                      char *s1,
                      char *s2,
                      const uint8_t complete,
                      TAG,
                      SRC_INFO) {
  const bool s1Complete = ARG1_COMPLETE(complete);
  const bool s2Complete = ARG2_COMPLETE(complete);
  validStringCheck(s1, s1Pool, s1Complete, "strcasestr", SRC_INFO_ARGS);
  validStringCheck(s2, s2Pool, s2Complete, "strcasestr", SRC_INFO_ARGS);
  return strcasestr(s1, s2);
}
#endif

#ifdef HAVE_STRNLEN
//
// pool_strnlen()
//
// See pool_strnlen_debug().
//
size_t
pool_strnlen(DebugPoolTy *stringPool, 
             char *string, 
             size_t maxlen,
             const uint8_t complete) {
  return pool_strnlen_debug(stringPool, string, maxlen, complete, DEFAULTS);
}

//
// Secure runtime wrapper function to replace strnlen()
//
// Like strlen(), but searches for the nul terminator only within the first
// maxlen bytes of the string. If the terminator is not found, then it returns
// the value of maxlen.
//
// Inputs:
//   strPool  Pool handle for the string
//   str      Pointer to string to analyze
//   maxlen   Maximum length to search
//   complete Completeness bit vector
//   TAG      Tag information for debugging purposes
//   SRC_INFO Source file and line number information for debugging purposes
// Returns:
//   This function returns strlen(str) if strlen(str) < maxlen, otherwise it
//   returns maxlen.
//
size_t
pool_strnlen_debug(DebugPoolTy *strPool, 
                   char *str, 
                   size_t maxlen, 
                   const uint8_t complete, 
                   TAG, 
                   SRC_INFO) {
  size_t safelen, len;
  void *strBegin = str, *strEnd = NULL;
  const bool strComplete = ARG1_COMPLETE(complete);
  bool strFound;
  if (!(strFound = pool_find(strPool, str, strBegin, strEnd)) && strComplete) {
    err << "String not found in pool!\n";
    LOAD_STORE_VIOLATION(str, strPool, SRC_INFO_ARGS);
  }
  if (strFound) {
    // This is the maximum number of characters that can be read from str
    // without causing a memory safety error.
    safelen = byte_range(str, strEnd);
    // Thus the maximum length that _strnlen() can return is safelen - 1.
    len     = _strnlen(str, std::min(maxlen, safelen));
    // If _strnlen() returns safelen, then that means that the string is not
    // terminated within the first safelen characters, meaning that if maxlen
    // > safelen, we will be reading at least safelen + 1 characters to find
    // a nul terminator, which causes a memory safety error.
    if (len == safelen && maxlen > safelen) {
      err << "strnlen() reads beyond the end of the input string's object!\n";
      OOB_VIOLATION(str, strPool, str, safelen + 1, SRC_INFO_ARGS);
    } else // If no memory safety error occurred, len is guaranteed to be the
           // value of strnlen(str, maxlen), so just return it right away.
      return len;
  }
  return strnlen(str, maxlen);
}
#endif

#ifdef HAVE_STPCPY
//
// pool_stpcpy()
//
// See pool_stpcpy_debug().
//
char *
pool_stpcpy(DebugPoolTy *dstPool,
            DebugPoolTy *srcPool,
            char *dst,
            char *src,
            const uint8_t complete) {
  return pool_stpcpy_debug(dstPool, srcPool, dst, src, complete, DEFAULTS);
}

//
// Secure runtime wrapper function to replace stpcpy()
//
// Copies the string src to dst and returns a pointer to the nul terminator of
// dst.
//
// Attempts to verify the following:
//  - src is nul terminated within memory object bounds
//  - src and dst do not overlap
//  - dst is long enough to hold src.
//
// Inputs:
//   dstPool  Pool handle for destination string's pool
//   srcPool  Pool handle for source string's pool
//   dst      Pointer to destination of copy operation
//   src      Pointer to string to copy
//   complete Completeness bit vector
//   TAG      Tag information for debugging purposes
//   SRC_INFO Source file and line number information for debugging purposes
// Returns:
//   This functions returns the value of &dst[strlen(dst)] after the copy
//   operation is completed.
//
char *
pool_stpcpy_debug(DebugPoolTy *dstPool,
                  DebugPoolTy *srcPool,
                  char *dst,
                  char *src,
                  const uint8_t complete,
                  TAG,
                  SRC_INFO) {
  void *dstBegin = NULL, *dstEnd = NULL, *srcBegin = NULL, *srcEnd = NULL;
  size_t srcLen = 0;
  const bool dstComplete = ARG1_COMPLETE(complete);
  const bool srcComplete = ARG2_COMPLETE(complete);
  bool dstFound, srcFound;
  // Find the destination and source strings in their pools.
  if (!(dstFound = pool_find(dstPool, dst, dstBegin, dstEnd)) && dstComplete) {
    err << "Could not find destination object in pool!\n";
    LOAD_STORE_VIOLATION(dst, dstPool, SRC_INFO_ARGS);
  }
  if (!(srcFound = pool_find(srcPool, src, srcBegin, srcEnd)) && srcComplete) {
    err << "Could not find source object in pool\n";
    LOAD_STORE_VIOLATION(src, srcPool, SRC_INFO_ARGS);
  }
  // Check if source is terminated.
  if (srcFound && !isTerminated(src, srcEnd, srcLen)) {
    err << "Source string not terminated within bounds!\n";
    C_LIBRARY_VIOLATION(src, srcPool, "stpcpy", SRC_INFO_ARGS);
  }
  // The remainder of the checks require both objects to be found.
  if (dstFound && srcFound) {
    // Check for overlap of objects.
    if (isOverlapped(dst, &dst[srcLen], src, &src[srcLen])) {
      err << "Copying overlapping strings has undefined behavior!\n";
      C_LIBRARY_VIOLATION(dst, dstPool, "stpcpy", SRC_INFO_ARGS);
    }
    // dstLen is the maximum length string that dst can hold.
    size_t dstLen = byte_range(dst, dstEnd) - 1;
    // Check for overflow of dst.
    if (srcLen > dstLen) {
      err << "Destination object too short to hold string!\n";
      WRITE_VIOLATION(dst, dstPool, dstLen, srcLen, SRC_INFO_ARGS);
    }
  }
  return stpcpy(dst, src);
}
#endif
