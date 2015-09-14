//===------- strings.cpp - CStdLib runtime for functions in <strings.h> ---===//
// 
//                          The SAFECode Compiler
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file provides CStdLib runtime wrapper versions of functions found in
// <strings.h>.
//
//===----------------------------------------------------------------------===//

#include "safecode/Config/config.h"
#include "CStdLib.h"

#include <algorithm>
#include <cctype>
#include <strings.h>

//
// pool_bcmp()
//
// This is the non-debug version of pool_bcmp_debug().
//
int
pool_bcmp(DebugPoolTy *aPool,
          DebugPoolTy *bPool,
          void *a,
          void *b,
          size_t n,
          const uint8_t complete) {
  return pool_bcmp_debug(aPool, bPool, a, b, n, complete, DEFAULTS);
}

//
// Secure runtime wrapper function to replace bcmp()
//
// Returns 0 if the first n bytes of the memory areas pointed to by a and b are
// identical in value, and nonzero otherwise.
//
// Inputs:
//   aPool    Pool handle for a
//   bPool    Pool handle for b
//   a        Pointer to first memory area
//   b        Pointer to second memory area
//   n        Number of bytes to compare
//   complete Completeness bit vector
//   TAG      Tag information for debugging purposes
//   SRC_INFO Source file and line number information for debugging purposes
// Returns:
//   Returns 0 if the first n bytes of the areas pointed to by a and b are
//   identical in value, and nonzero otherwise.
//
int
pool_bcmp_debug(DebugPoolTy *aPool,
                DebugPoolTy *bPool,
                void *a,
                void *b,
                size_t n,
                const uint8_t complete,
                TAG,
                SRC_INFO) {
  void *aStart = NULL, *aEnd = NULL;
  void *bStart = NULL, *bEnd = NULL;
  const bool aComplete = ARG1_COMPLETE(complete);
  const bool bComplete = ARG2_COMPLETE(complete);
  bool aFound, bFound;
  // Retrieve both objects from their pointers' pools.
  if (!(aFound = pool_find(aPool, (void *) a, aStart, aEnd)) && aComplete) {
    err << "Object for 1st argument to bcmp() not found in pool!\n";
    LOAD_STORE_VIOLATION(a, aPool, SRC_INFO_ARGS);
  }
  if (!(bFound = pool_find(bPool, (void *) b, bStart, bEnd)) && bComplete) {
    err << "Object for 2nd argument to bcmp() not found in pool!\n";
    LOAD_STORE_VIOLATION(b, bPool, SRC_INFO_ARGS);
  }
  // Determine if both pointers can be read safely.
  size_t aSize   = aFound ? byte_range(a, aEnd) : n;
  size_t bSize   = bFound ? byte_range(b, bEnd) : n;
  size_t safelen = std::min(n, std::min(aSize, bSize));
  int    result  = bcmp(a, b, safelen);
  if (safelen == n)
    return result;
  else if (result == 0) {
    err << "bcmp() reads beyond object boundaries!\n";
    if (aSize <= bSize)
      OOB_VIOLATION(a, aPool, a, safelen + 1, SRC_INFO_ARGS);
    if (bSize <= aSize)
      OOB_VIOLATION(b, bPool, b, safelen + 1, SRC_INFO_ARGS);
    return bcmp(a, b, n);
  }
  else
    return 1;
}

//
// pool_bcopy()
//
// See pool_bcopy_debug().
//
void
pool_bcopy(DebugPoolTy *s1Pool,
           DebugPoolTy *s2Pool,
           void *s1,
           void *s2,
           size_t n,
           const uint8_t complete) {
  pool_bcopy_debug(s1Pool, s2Pool, s1, s2, n, complete, DEFAULTS);
}

//
// Secure runtime wrapper function to replace bcopy()
//
// Copies n bytes from s1 into s2.
//
// Attempts to verify that:
//  - the first n bytes of s1 are completely contained in s1's memory object
//  - the area pointed to by s2 has enough space to hold the result of the copy
//
// Inputs:
//   s1Pool   Pool handle for s1
//   s2Pool   Pool handle for s2
//   s1       Pointer to source memory area
//   s2       Pointer to destination memory area
//   n        Number of bytes to copy
//   complete Completeness bit vector
//   TAG      Tag information for debugging purposes
//   SRC_INFO Source file and line number information for debugging purposes
// Returns:
//   This function does not return a value.
//
void
pool_bcopy_debug(DebugPoolTy *s1Pool,
                 DebugPoolTy *s2Pool,
                 void *s1,
                 void *s2,
                 size_t n,
                 const uint8_t complete,
                 TAG,
                 SRC_INFO) {
  void *s1Start = NULL, *s1End = NULL;
  void *s2Start = NULL, *s2End = NULL;
  const bool s1Complete = ARG1_COMPLETE(complete);
  const bool s2Complete = ARG2_COMPLETE(complete);
  bool s1Found, s2Found;
  // Retrieve both memory objects' boundaries from their pointers' pools.
  if (!(s1Found = pool_find(s1Pool, s1, s1Start, s1End)) && s1Complete) {
    err << "Source object not found in pool!\n";
    LOAD_STORE_VIOLATION(s1, s1Pool, SRC_INFO_ARGS);
  }
  if (!(s2Found = pool_find(s2Pool, s2, s2Start, s2End)) && s2Complete) {
    err << "Destination object not found in pool!\n";
    LOAD_STORE_VIOLATION(s2, s2Pool, SRC_INFO_ARGS);
  }
  // Determine if the copy operation does not read or write data out of bounds
  // of the pointer's memory areas.
  size_t s1Bytes = byte_range(s1, s1End);
  if (s1Found && n > s1Bytes) {
    err << "bcopy() reads beyond the end of the source object!\n";
    OOB_VIOLATION(s1, s1Pool, s1, n, SRC_INFO_ARGS);
  }
  size_t s2Bytes = byte_range(s2, s2End);
  if (s2Found && n > s2Bytes) {
    err << "bcopy() writes beyond the end of the destination object!\n";
    WRITE_VIOLATION(s2, s2Pool, s2Bytes, n, SRC_INFO_ARGS);
  }
  // No need to handle overlap - bcopy() takes care of this.
  bcopy(s1, s2, n);
}

//
// pool_bzero()
//
// See pool_bzero_debug().
//
void
pool_bzero(DebugPoolTy *sPool,
           void *s,
           size_t n,
           const uint8_t complete) {
  pool_bzero_debug(sPool, s, n, complete, DEFAULTS);
}

//
// Secure runtime wrapper function to replace bzero()
//
// Overwrites the first n bytes of the memory area pointed to by s with bytes
// of value 0.
//
// Attempts to verify that the first n bytes of s are completely contained
// in s's memory object.
//
// Inputs:
//   sPool    Pool handle for s
//   s        Pointer to memory area to be zeroed
//   n        Number of bytes to zero
//   complete Completeness bit vector
//   TAG      Tag information for debugging purposes
//   SRC_INFO Source file and line number information for debugging purposes
// Returns:
//   This function does not return a value.
//
void
pool_bzero_debug(DebugPoolTy *sPool,
                 void *s,
                 size_t n,
                 const uint8_t complete,
                 TAG,
                 SRC_INFO) {
  void *sStart = NULL, *sEnd = NULL;
  const bool sComplete = ARG1_COMPLETE(complete);
  bool sFound;
  // Get the memory object that s points to from the pool.
  if (!(sFound = pool_find(sPool, s, sStart, sEnd)) && sComplete) {
    err << "Memory object not found in pool!\n";
    LOAD_STORE_VIOLATION(s, sPool, SRC_INFO_ARGS);
  }
  // Determine if the write operation would write beyond the end of the
  // memory object.
  size_t sBytes = byte_range(s, sEnd);
  if (sFound && n > sBytes) {
    err << "bzero() writes beyond the end of the destination memory object!\n";
    WRITE_VIOLATION(s, sPool, sBytes, n, SRC_INFO_ARGS);
  }
  bzero(s, n);
}

//
// pool_index()
//
// See pool_index_debug().
//
char *
pool_index(DebugPoolTy *sPool,
           char *s,
           int c,
           const uint8_t complete) {
  return pool_index_debug(sPool, s, c, complete, DEFAULTS);
}

//
// Secure runtime wrapper function to replace index()
//
// Returns a pointer to the first instance of the character c in the string s,
// or NULL if c is not found.
//
// Attempts to verify that s is a string terminated within object boundaries.
//
// Inputs:
//   sPool    Pool handle for s
//   s        String to search
//   c        Character to find in s
//   complete Completeness bit vector
//   TAG      Tag information for debugging purposes
//   SRC_INFO Source file and line number information for debugging purposes
// Returns:
//   This function returns a pointer to the first instance of c in s, or NULL if
//   c is not found in s.
//
char *
pool_index_debug(DebugPoolTy *sPool,
                 char *s,
                 int c,
                 const uint8_t complete,
                 TAG,
                 SRC_INFO) {
  const bool sComplete = ARG1_COMPLETE(complete);
  validStringCheck(s, sPool, sComplete, "index", SRC_INFO_ARGS);
  return index(s, c);
}

//
// pool_rindex()
//
// See pool_rindex_debug().
//
char *
pool_rindex(DebugPoolTy *sPool,
           char *s,
           int c,
           const uint8_t complete) {
  return pool_rindex_debug(sPool, s, c, complete, DEFAULTS);
}

//
// Secure runtime wrapper function to replace rindex()
//
// Returns a pointer to the last instance of the character c in the string s,
// or NULL if c is not found.
//
// Attempts to verify that s is a string terminated within object boundaries.
//
// Inputs:
//   sPool     Pool handle for s
//   s         Pointer to string to search
//   c         Character to find in s
//   complete Completeness bit vector
//   TAG      Tag information for debugging purposes
//   SRC_INFO Source file and line number information for debugging purposes
// Returns:
//   This function returns a pointer to the last instance of c in s, or NULL if
//   c is not found in s.
//
char *
pool_rindex_debug(DebugPoolTy *sPool,
                  char *s,
                  int c,
                  const uint8_t complete,
                  TAG,
                  SRC_INFO) {
  const bool sComplete = ARG1_COMPLETE(complete);
  validStringCheck(s, sPool, sComplete, "rindex", SRC_INFO_ARGS);
  return rindex(s, c);
}

//
// pool_strcasecmp()
//
// See pool_strcasecmp_debug().
//
int
pool_strcasecmp(DebugPoolTy *s1p,
                DebugPoolTy *s2p, 
                char *s1, 
                char *s2,
                const uint8_t complete) {
  return pool_strcasecmp_debug(s1p, s2p, s1, s2, complete, DEFAULTS);
}

//
// Secure runtime wrapper function to replace strcasecmp()
//
// Compares str1 and str2 in a case-insensitive manner.
//
// Attempts to verify that str1 and str2 point to valid strings terminated
// within their memory objects' boundaries.
//
// Inputs:
//   str1Pool   Pool handle for str1
//   str2Pool   Pool handle for str2
//   str1      First string to be compared
//   str2     Second string to be compared
//   complete Completeness bit vector
//   TAG      Tag information for debugging purposes
//   SRC_INFO Source file and line number information for debugging purposes
// Returns:
//   This function returns zero if str1 = str2, a positive integer if str1 >
//   str2, and a negative integer if str1 < str2, with string comparison done
//   case insensitively.
//
int
pool_strcasecmp_debug(DebugPoolTy *str1Pool,
                      DebugPoolTy *str2Pool, 
                      char *str1, 
                      char *str2,
                      const uint8_t complete,
                      TAG,
                      SRC_INFO) {
  const bool str1Complete = ARG1_COMPLETE(complete);
  const bool str2Complete = ARG2_COMPLETE(complete);
  validStringCheck(str1, str1Pool, str1Complete, "strcasecmp", SRC_INFO_ARGS);
  validStringCheck(str2, str2Pool, str2Complete, "strcasecmp", SRC_INFO_ARGS);
  return strcasecmp(str1, str2);
}

//
// pool_strncasecmp()
//
// See pool_strncasecmp_debug().
//
int
pool_strncasecmp(DebugPoolTy *s1p,
                 DebugPoolTy *s2p, 
                 char *s1, 
                 char *s2,
                 size_t num,
                 const uint8_t complete){
  return pool_strncasecmp_debug(s1p, s2p, s1, s2, num, complete, DEFAULTS);
}

//
// Secure runtime wrapper function to replace strncasecmp()
//
// Compares at most the first n characters from s1 and s2 in a case insensitive
// manner.
//
// Inputs:
//   s1Pool   Pool handle for s1
//   s2Pool   Pool handle for s2
//   s1       First string to be compared
//   s2       Second string to be compared
//   complete Completeness bit vector
//   TAG      Tag information for debugging purposes
//   SRC_INFO Source file and line number information for debugging purposes
// Returns:
//   Let str1 = s1[0..n-1] and str2 = s2[0..n-1]. The return value is negative
//   when str1 < str2, zero when str1 = str2, and positive when str1 > str2.
//   The comparison is done case insensitively.
//
int
pool_strncasecmp_debug(DebugPoolTy *s1Pool,
                       DebugPoolTy *s2Pool, 
                       char *s1, 
                       char *s2,
                       size_t n,
                       const uint8_t complete,
                       TAG,
                       SRC_INFO)
{
  const bool s1Complete = ARG1_COMPLETE(complete);
  const bool s2Complete = ARG2_COMPLETE(complete);
  bool s1Found, s2Found;
  void *s1Start, *s1End = NULL, *s2Start, *s2End = NULL;
  if (!(s1Found = pool_find(s1Pool, s1, s1Start, s1End)) && s1Complete) {
    err << "Memory object containing string 1 not found in pool!\n";
    LOAD_STORE_VIOLATION(s1, s1Pool, SRC_INFO_ARGS);
  }
  if (!(s2Found = pool_find(s2Pool, s2, s2Start, s2End)) && s2Complete) {
    err << "Memory object containing string 2 not found in pool!\n";
    LOAD_STORE_VIOLATION(s2, s2Pool, SRC_INFO_ARGS);
  }
  const unsigned char *str1 = (const unsigned char *) s1;
  const unsigned char *str2 = (const unsigned char *) s2;
  size_t s1Safe = s1Found ? byte_range(s1, s1End) : n;
  size_t s2Safe = s2Found ? byte_range(s2, s2End) : n;
  // Get the maximum number of characters we can look into without reading out
  // of bounds.
  size_t safe   = std::min(n, std::min(s1Safe, s2Safe));
  // Compare the strings safely.
  for (size_t i = 0; i < safe; ++i) {
    if (int diff = std::tolower(str1[i]) - std::tolower(str2[i]))
      return diff;
    else if (str1[i] == 0) // End of strings reached.
      return 0;
  }
  if (safe == n) // Strings are equal up to the first n characters.
    return 0;
  else {
    err << "strncasecmp() reads beyond the end of string's object!\n";
    if (s1Safe <= s2Safe)
      OOB_VIOLATION(s1, s1Pool, s1, s1Safe + 1, SRC_INFO_ARGS);
    if (s2Safe <= s1Safe)
      OOB_VIOLATION(s2, s2Pool, s2, s2Safe + 1, SRC_INFO_ARGS);
    return strncasecmp(s1, s2, n);
  }
}
