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


#include "CStdLib.h"


/*
 * pool_bcmp()
 *
 * This is the non-debug version of pool_bcmp_debug().
 *
 */
int
pool_bcmp(DebugPoolTy *aPool,
          DebugPoolTy *bPool,
          const void *a,
          const void *b,
          size_t n,
          const uint8_t complete)
{
  return pool_bcmp_debug(aPool, bPool, a, b, n, complete,
                         DEFAULT_TAG, DEFAULT_SRC_INFO);
}


/**
 * Secure runtime wrapper function to replace bcmp(), debug version
 *
 * Returns 0 if the first n bytes of the memory areas pointed to by a and b are
 * identical in value, and nonzero otherwise.
 *
 * Attempts to verify that the first n bytes of the memory areas pointed to by
 * a and b are both completely contained within their respective objects.
 *
 * @param aPool     Pool handle for a
 * @param bPool     Pool handle for b
 * @param a         Pointer to first memory area
 * @param b         Pointer to second memory area
 * @param n         Amount of bytes to compare
 * @param complete  Completeness bitwise vector
 * @param TAG       Tag infomation for debug
 * @param SRC_INFO  Source and line information for debug
 * @return          Returns 0 if the first n bytes of the areas pointed to by
 *                  a and b are identical in value, and nonzero otherwise.
 *
 */
int
pool_bcmp_debug(DebugPoolTy *aPool,
                DebugPoolTy *bPool,
                const void *a,
                const void *b,
                size_t n,
                const uint8_t complete,
                TAG,
                SRC_INFO)
{
  void *aStart = NULL, *aEnd = NULL;
  void *bStart = NULL, *bEnd = NULL;
  const bool aComplete = ARG1_COMPLETE(complete);
  const bool bComplete = ARG2_COMPLETE(complete);
  bool aFound, bFound;

  // Retrieve both objects from their pointers' pools.
  if (!(aFound = pool_find(aPool, (void *) a, aStart, aEnd)) && aComplete)
    LOAD_STORE_VIOLATION(a, aPool, SRC_INFO_ARGS);

  if (!(bFound = pool_find(bPool, (void *) b, bStart, bEnd)) && bComplete)
    LOAD_STORE_VIOLATION(b, bPool, SRC_INFO_ARGS);

  // Determine if both pointers can be read safely up to n bytes into their
  // target without going out of bounds of the memory object.
  size_t aBytes = 1 + (char *) aEnd - (char *) a;
  if (aFound && n > aBytes)
    OOB_VIOLATION(a, aPool, a, n, SRC_INFO_ARGS);

  size_t bBytes = 1 + (char *) bEnd - (char *) b;
  if (bFound && n > bBytes)
    OOB_VIOLATION(b, bPool, b, n, SRC_INFO_ARGS);

  return bcmp(a, b, n);
}


/*
 * pool_bcopy()
 *
 * This is the non-debug version of pool_bcmp_debug().
 *
 */
void
pool_bcopy(DebugPoolTy *s1Pool,
           DebugPoolTy *s2Pool,
           const void *s1,
           void *s2,
           size_t n,
           const uint8_t complete)
{
  pool_bcopy_debug(s1Pool, s2Pool, s1, s2, n, complete,
                   DEFAULT_TAG, DEFAULT_SRC_INFO);
}


/**
 * Secure runtime wrapper function to replace bcopy(), debug version
 *
 * Copies n bytes from s1 into s2.
 *
 * Attempts to verify that:
 *  - the first n bytes of s1 are completely contained in s1's memory object
 *  - the area pointed to by s2 has enough space to hold the result of the copy
 *
 * @param s1Pool    Pool handle for s1
 * @param s2Pool    Pool handle for s2
 * @param s1        Pointer to source memory area
 * @param s2        Pointer to destination memory area
 * @param n         Amount of bytes to copy
 * @param complete  Completeness bitwise vector
 * @param TAG       Tag infomation for debug
 * @param SRC_INFO  Source and line information for debug
 * @return          Does not return a value
 *
 */
void
pool_bcopy_debug(DebugPoolTy *s1Pool,
                 DebugPoolTy *s2Pool,
                 const void *s1,
                 void *s2,
                 size_t n,
                 const uint8_t complete,
                 TAG,
                 SRC_INFO)
{
  void *s1Start = NULL, *s1End = NULL;
  void *s2Start = NULL, *s2End = NULL;
  const bool s1Complete = ARG1_COMPLETE(complete);
  const bool s2Complete = ARG2_COMPLETE(complete);
  bool s1Found, s2Found;

  // Retrieve both memory objects' boundaries from their pointers' pools.
  if (!(s1Found = pool_find(s1Pool, (void *) s1, s1Start, s1End)) && s1Complete)
    LOAD_STORE_VIOLATION(s1, s1Pool, SRC_INFO_ARGS);

  if (!(s2Found = pool_find(s2Pool, s2, s2Start, s2End)) && s2Complete)
    LOAD_STORE_VIOLATION(s2, s2Pool, SRC_INFO_ARGS);

  // Determine if the copy operation does not read or write data out of bounds
  // of the pointer's memory areas.
  size_t s1Bytes = 1 + (char *) s1End - (char *) s1;
  if (s1Found && n > s1Bytes)
    OOB_VIOLATION(s1, s1Pool, s1, n, SRC_INFO_ARGS);

  size_t s2Bytes = 1 + (char *) s2End - (char *) s2;
  if (s2Found && n > s2Bytes)
    WRITE_VIOLATION(s2, s2Pool, s2Bytes, n, SRC_INFO_ARGS);

  bcopy(s1, s2, n);
}


/*
 * pool_bzero()
 *
 * This is the non-debug version of pool_bzero_debug().
 *
 */
void
pool_bzero(DebugPoolTy *sPool,
           void *s,
           size_t n,
           const uint8_t complete)
{
  pool_bzero_debug(sPool, s, n, complete, DEFAULT_TAG, DEFAULT_SRC_INFO);
}


/**
 * Secure runtime wrapper function to replace bzero(), debug version
 *
 * Overwrites the first n bytes of the memory area pointed to by s with bytes
 * of value 0.
 *
 * Attempts to verify that the first n bytes of s are completely contained
 * in s's memory object.
 *
 * @param sPool     Pool handle for s
 * @param s         Pointer to memory area to be zeroed
 * @param n         Amount of bytes to zero
 * @param complete  Completeness bitwise vector
 * @param TAG       Tag infomation for debug
 * @param SRC_INFO  Source and line information for debug
 * @return          Does not return a value
 *
 */
void
pool_bzero_debug(DebugPoolTy *sPool,
                 void *s,
                 size_t n,
                 const uint8_t complete,
                 TAG,
                 SRC_INFO)
{
  void *sStart = NULL, *sEnd = NULL;
  const bool sComplete = ARG1_COMPLETE(complete);
  bool sFound;

  // Get the memory object that s points to from the pool.
  if (!(sFound = pool_find(sPool, (void *) s, sStart, sEnd)) && sComplete)
    LOAD_STORE_VIOLATION(s, sPool, SRC_INFO_ARGS);

  // Determine if the write operation would write beyond the end of the
  // memory object.
  size_t sBytes = 1 + (char *) sEnd - (char *) s;
  if (sFound && n > sBytes)
    WRITE_VIOLATION(s, sPool, sBytes, n, SRC_INFO_ARGS);

  bzero(s, n);
}


/*
 * pool_index()
 *
 * This is the non-debug version of pool_index_debug().
 *
 */
char *
pool_index(DebugPoolTy *sPool,
           const char *s,
           int c,
           const uint8_t complete)
{
  return pool_index_debug(sPool, s, c, complete,
                          DEFAULT_TAG, DEFAULT_SRC_INFO);
}


/**
 * Secure runtime wrapper function to replace index(), debug version
 *
 * Returns a pointer to the first instance of the character c in the string s,
 * or NULL if c is not found.
 *
 * Attempts to verify that s is a string terminated within object boundaries.
 *
 * @param sPool     Pool handle for s
 * @param s         Pointer to string to search
 * @param c         Character to find in s
 * @param complete  Completeness bitwise vector
 * @param TAG       Tag infomation for debug
 * @param SRC_INFO  Source and line information for debug
 * @return          A pointer to the first instance of c in s, or NULL if c is
 *                  not found
 *
 */
char *
pool_index_debug(DebugPoolTy *sPool,
                 const char *s,
                 int c,
                 const uint8_t complete,
                 TAG,
                 SRC_INFO)
{
  validStringCheck(s, sPool, ARG1_COMPLETE(complete), "index", SRC_INFO_ARGS);
  return index((char *)s, c);
}


/*
 * pool_rindex()
 *
 * This is the non-debug version of pool_rindex_debug().
 *
 */
char *
pool_rindex(DebugPoolTy *sPool,
           const char *s,
           int c,
           const uint8_t complete)
{
  return pool_rindex_debug(sPool, s, c, complete,
                          DEFAULT_TAG, DEFAULT_SRC_INFO);
}


/**
 * Secure runtime wrapper function to replace rindex(), debug version
 *
 * Returns a pointer to the last instance of the character c in the string s,
 * or NULL if c is not found.
 *
 * Attempts to verify that s is a string terminated within object boundaries.
 *
 * @param sPool     Pool handle for s
 * @param s         Pointer to string to search
 * @param c         Character to find in s
 * @param complete  Completeness bitwise vector
 * @param TAG       Tag infomation for debug
 * @param SRC_INFO  Source and line information for debug
 * @return          A pointer to the last instance of c in s, or NULL if c is
 *                  not found
 *
 */
char *
pool_rindex_debug(DebugPoolTy *sPool,
                 const char *s,
                 int c,
                 const uint8_t complete,
                 TAG,
                 SRC_INFO)
{
  validStringCheck(s, sPool, ARG1_COMPLETE(complete), "rindex", SRC_INFO_ARGS);
  return rindex((char *)s, c);
}


/*
 * pool_strcasecmp()
 *
 * This is the non-debug version of pool_strcasecmp_debug().
 *
 */
int
pool_strcasecmp(DebugPoolTy *s1p,
                DebugPoolTy *s2p, 
                const char *s1, 
                const char *s2,
                const uint8_t complete)
{
  return pool_strcasecmp_debug(s1p, s2p, s1, s2, complete,
                               DEFAULT_TAG, DEFAULT_SRC_INFO);
}


/**
 * Secure runtime wrapper function to replace strcasecmp(), debug version
 *
 * Compares str1 and str2 in a case-insensitive manner.
 *
 * Attempts to verify that str1 and str2 point to valid strings terminated
 * within their memory objects' boundaries.
 *
 * @param str1Pool   Pool handle for str1
 * @param str2Pool   Pool handle for str2
 * @param str1       First string to be compared
 * @param str2       Second string to be compared
 * @param complete   Completeness bitwise vector
 * @param TAG        Tag information for debug
 * @param SRC_INFO   Source file and line number information  for debug
 * @return           Returns zero if str1 = str2,
 *                           a positive integer if str1 > str2
 *                           a negative integer if str1 < str2,
 *                   with string comparison done case insensitively
 *
 */
int
pool_strcasecmp_debug(DebugPoolTy *str1Pool,
                      DebugPoolTy *str2Pool, 
                      const char *str1, 
                      const char *str2,
                      const uint8_t complete,
                      TAG,
                      SRC_INFO)
{
  validStringCheck(str1, str1Pool, ARG1_COMPLETE(complete),
                   "strcasecmp", SRC_INFO_ARGS);
  validStringCheck(str2, str2Pool, ARG2_COMPLETE(complete),
                   "strcasecmp", SRC_INFO_ARGS);
  return strcasecmp(str1, str2);
}


/*
 * pool_strncasecmp()
 *
 * This is the non-debug version of pool_strncasecmp_debug().
 *
 */
int
pool_strncasecmp(DebugPoolTy *s1p,
                 DebugPoolTy *s2p, 
                 const char *s1, 
                 const char *s2,
                 size_t num,
                 const uint8_t complete){

  return pool_strncasecmp_debug(s1p, s2p, s1, s2, num, complete,
                                DEFAULT_TAG, DEFAULT_SRC_INFO);
}


/**
 * Secure runtime wrapper function to replace strncasecmp()
 *
 * Compares at most the first n characters from str1 and str2 in a case
 * insensitive manner.
 *
 * Attempts to verify that str1 and str2 point to valid strings terminated
 * within memory object boundaries
 *
 * @param str1Pool   Pool handle for str1
 * @param str2Pool   Pool handle for str2
 * @param str1       First string to be compared
 * @param str2       Second string to be compared
 * @param complete   Completeness bitwise vector
 * @param TAG        Tag information for debug
 * @param SRC_INFO   Source file and line number information for debug
 * @return           Taking s1 to be the string or array found in the
 *                   first n characters of the area pointed to by str1,
 *                   and s2 to be the string or array found in the first
 *                   n characters of the area pointed to by s2, returns
 *                   zero if s1 = s2, a positive integer if s1 > s2,
 *                   and a negative integer if s1 < s2, with string
 *                   comparison being done case insensitively
 *
 */
int
pool_strncasecmp_debug(DebugPoolTy *str1Pool,
                       DebugPoolTy *str2Pool, 
                       const char *str1, 
                       const char *str2,
                       size_t n,
                       const uint8_t complete,
                       TAG,
                       SRC_INFO)
{
  validStringCheck(str1, str1Pool, ARG1_COMPLETE(complete),
                   "strncasecmp", SRC_INFO_ARGS);
  validStringCheck(str2, str2Pool, ARG2_COMPLETE(complete),
                   "strncasecmp", SRC_INFO_ARGS);
  return strncasecmp(str1, str2, n);
}
