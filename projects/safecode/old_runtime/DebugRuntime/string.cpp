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


#include "CStdLib.h"

//
// The following functions have CStdLib wrapper versions in this file:
//  - stpcpy
//  - strchr
//  - strrchr
//  - strstr
//  - strcasestr
//  - strcat
//  - strncat
//  - strpbrk
//  - strcmp
//  - memcpy
//  - memmove
//  - mempcpy
//  - memset
//  - strcpy
//  - strlen
//  - strncpy
//  - strnlen
//  - strncmp
//  - memcmp
//  - strspn
//  - strcspn
//  - memchr
//  - memccpy
//


/*
 * pool_stpcpy()
 *
 * This is the non-debug version of pool_stpcpy_debug().
 *
 */
char *
pool_stpcpy(DebugPoolTy *dstPool,
            DebugPoolTy *srcPool,
            char *dst,
            const char *src,
            const uint8_t complete) {
  return pool_stpcpy_debug(dstPool, srcPool, dst, src, complete,
    DEFAULT_TAG, DEFAULT_SRC_INFO);
}


/**
 * Secure runtime wrapper function to replace stpcpy()
 *
 * Copies the string src to dst and returns a pointer
 * to the nul terminator of dst.
 * 
 * Attempts to verify the following:
 *  - src is nul terminated within memory object bounds
 *  - src and dst do not overlap
 *  - dst is long enough to hold src.
 *
 * @param   dstPool    Pool handle for destination string's pool
 * @param   srcPool    Pool handle for destination string's pool
 * @param   dst        Pointer to destination of copy operation
 * @param   src        Pointer to string to copy
 * @param   complete   Completeness bitwise vector
 * @param   TAG        Tag information for debug
 * @param   SRC_INFO   Source and line number information
 * @return             Returns a pointer to the nul-terminator of dst.
 *
 */
char *
pool_stpcpy_debug(DebugPoolTy *dstPool,
                  DebugPoolTy *srcPool,
                  char *dst,
                  const char *src,
                  const uint8_t complete,
                  TAG,
                  SRC_INFO) {
  void *dstBegin = NULL, *dstEnd = NULL, *srcBegin = NULL, *srcEnd = NULL;
  size_t srcLen = 0;
  const bool dstComplete = ARG1_COMPLETE(complete);
  const bool srcComplete = ARG2_COMPLETE(complete);
  bool dstFound, srcFound;

  // Find the destination and source strings in their pools.
  if (!(dstFound = pool_find(dstPool, dst, dstBegin, dstEnd))
        && dstComplete) {
    std::cout << "Could not find destination object in pool!\n";
    LOAD_STORE_VIOLATION(dst, dstPool, SRC_INFO_ARGS);
  }
  if (!(srcFound = pool_find(srcPool, (void*)src, srcBegin, srcEnd))
        && srcComplete) {
    std::cout << "Could not find source object in pool\n";
    LOAD_STORE_VIOLATION(src, srcPool, SRC_INFO_ARGS);
  }

  // Check if source is terminated.
  if (srcFound && !isTerminated(src, srcEnd, srcLen)) {
    std::cout << "Source string not terminated within bounds!\n";
    C_LIBRARY_VIOLATION(src, srcPool, "stpcpy", SRC_INFO_ARGS);
  }

  // The remainder of the checks require both objects to be found.
  if (dstFound && srcFound) {
    // Check for overlap of objects.
    if (isOverlapped(dst, &dst[srcLen], src, &src[srcLen])) {
      std::cout << "Copying overlapping strings has undefined behavior!\n";
      C_LIBRARY_VIOLATION(dst, dstPool, "stpcpy", SRC_INFO_ARGS);
    }
    // dstLen is the maximum length string that dst can hold.
    size_t dstLen = (char *) dstEnd - dst;
    // Check for overflow of dst.
    if (srcLen > dstLen) {
      std::cout << "Destination object too short to hold string!\n";
      WRITE_VIOLATION(dst, dstPool, dstLen, srcLen, SRC_INFO_ARGS);
    }
  }

  return stpcpy(dst, src);
}


/*
 * pool_strchr()
 *
 * This is the non-debug version of pool_strchr_debug().
 *
 */
char *
pool_strchr(DebugPoolTy *sp, const char *s, int c, const uint8_t complete) {
  return pool_strchr_debug(sp, s, c, complete, DEFAULT_TAG, DEFAULT_SRC_INFO);
}


/**
 * Secure runtime wrapper function to replace strchr(), debug version
 *
 * Returns pointer to the first instance of the given character in the string, or
 * NULL if not found.
 *
 * Ensures the following:
 *  - string argument points to a valid string
 *
 * @param   sPool     Pool handle for string
 * @param   s         String pointer
 * @param   c         Character to find
 * @param   complete  Completeness bitwise vector
 * @param   TAG       Tag information for debug
 * @param   SRC_INFO  Source and line number information
 * @return  Pointer to first instance of c in s or NULL
 *
 */
char *
pool_strchr_debug(DebugPoolTy *sPool,
                  const char *s,
                  int c,
                  const uint8_t complete,
                  TAG,
                  SRC_INFO) {
  validStringCheck(s, sPool, ARG1_COMPLETE(complete), "strchr", SRC_INFO_ARGS);
  return strchr((char*)s, c);
}


/*
 * pool_strrchr()
 *
 * This is the non-debug version of pool_strchr_debug().
 *
 */
char *
pool_strrchr(DebugPoolTy *sPool,
             const char *s,
             int c,
             const uint8_t complete) {
  return pool_strrchr_debug(sPool, s, c, complete, DEFAULT_TAG,
    DEFAULT_SRC_INFO);
}


/**
 * Secure runtime wrapper function to replace strrchr()
 *
 * @param   sPool  Pool handle for string
 * @param   s      String pointer
 * @param   c      Character to find
 * @return  Pointer to last instance of c in s or NULL
 */
char *
pool_strrchr_debug(DebugPoolTy *sPool,
                   const char *s,
                   int c,
                   const uint8_t complete,
                   TAG,
                   SRC_INFO) {
  validStringCheck(s, sPool, ARG1_COMPLETE(complete), "strrchr", SRC_INFO_ARGS);
  return strrchr((char*)s, c);
}


/*
 * pool_strstr()
 *
 * This is the non-debug version of pool_strstr_debug().
 *
 */
char *
pool_strstr(DebugPoolTy *s1Pool,
            DebugPoolTy *s2Pool,
            const char *s1,
            const char *s2,
            const uint8_t complete) {
  return pool_strstr_debug(s1Pool, s2Pool, s1, s2, complete, DEFAULT_TAG,
    DEFAULT_SRC_INFO);
}


/**
 * Secure runtime wrapper function to replace strstr(), debug version
 *
 * Searches for the first occurence of the substring s2 in s1.
 * Returns a pointer to the discovered substring, or NULL if not found.
 *
 * Attempts to verify that s1 and s2 are valid strings terminated within
 * their memory objects' boundaries.
 *
 * @param   s1Pool    Pool handle for s1
 * @param   s2Pool    Pool handle for s2
 * @param   s1        Pointer to string to be searched
 * @param   s2        Pointer to substring to search for
 * @param   complete  Completeness bitwise vector
 * @param   TAG       Tag information for debug
 * @param   SRC_INFO  Source and line number information for debug
 * @retrun            Returns a pointer to the first instance of s2 in s1,
 *                    or NULL if none exists.
 *
 */
char *
pool_strstr_debug(DebugPoolTy *s1Pool,
                  DebugPoolTy *s2Pool,
                  const char *s1,
                  const char *s2,
                  const uint8_t complete,
                  TAG,
                  SRC_INFO) {
  validStringCheck(s1, s1Pool, ARG1_COMPLETE(complete), "strstr",
    SRC_INFO_ARGS);
  validStringCheck(s2, s2Pool, ARG2_COMPLETE(complete), "strstr",
    SRC_INFO_ARGS);
  return strstr((char*)s1, s2);
}


/*
 * pool_strcasestr()
 *
 * This is the non-debug version of pool_strcasestr_debug().
 *
 */
char *
pool_strcasestr(DebugPoolTy *s1Pool,
                DebugPoolTy *s2Pool,
                const char *s1,
                const char *s2,
                const uint8_t complete) {
  return pool_strcasestr_debug(s1Pool, s2Pool, s1, s2, complete, DEFAULT_TAG,
    DEFAULT_SRC_INFO);
}


/**
 * Secure runtime wrapper function to replace strcasestr(), debug version
 *
 * Searches for the first occurence of the substring s2 in s1, case
 * insensitively.
 * Returns a pointer to the discovered substring, or NULL if not found.
 *
 * Attempts to verify that s1 and s2 are valid strings terminated within
 * their memory objects' boundaries.
 *
 * @param   s1Pool    Pool handle for s1
 * @param   s2Pool    Pool handle for s2
 * @param   s1        Pointer to string to be searched
 * @param   s2        Pointer to substring to search for
 * @param   complete  Completeness bitwise vector
 * @param   TAG       Tag information for debug
 * @param   SRC_INFO  Source and line number information for debug
 * @retrun            Returns a pointer to the first instance of s2 in s1,
 *                    disregarding case, or NULL if none exists.
 *
 */
char *
pool_strcasestr_debug(DebugPoolTy *s1Pool,
                      DebugPoolTy *s2Pool,
                      const char *s1,
                      const char *s2,
                      const uint8_t complete,
                      TAG,
                      SRC_INFO) {
  validStringCheck(s1, s1Pool, ARG1_COMPLETE(complete), "strcasestr",
    SRC_INFO_ARGS);
  validStringCheck(s2, s2Pool, ARG2_COMPLETE(complete), "strcasestr",
    SRC_INFO_ARGS);
  return strcasestr((char*)s1, s2);
}


/*
 * pool_strcat()
 *
 * This is the non-debug version of pool_strcat_debug().
 *
 */
char *
pool_strcat(DebugPoolTy *dp,
            DebugPoolTy *sp,
            char *d,
            const char *s,
            const unsigned char c) {
  return pool_strcat_debug(dp, sp, d, s, c, DEFAULT_TAG, DEFAULT_SRC_INFO);
}


/**
 * Secure runtime wrapper function to replace strcat(), debug version
 *
 * Appends the source string to the end of the destination string.
 * Attempts to verify the following:
 *  - source and destination pointers point to valid strings
 *  - the destination string's object has enough space to hold the
 *    concatenation in memory.
 *
 * @param  dp    Pool handle for destination string
 * @param  sp    Pool handle for source string
 * @param  d     Pointer to the destination string
 * @param  s     Pointer to the source string
 * @param  c     Bitwise vector holding completeness information
 *               for source and destination strings
 * @param  TAG   Tag information for debug
 * @param  SRC_INFO Source and line number information.
 * @return       This returns the pointer to the destination string.
 *
 */
char *
pool_strcat_debug(DebugPoolTy *dstPool,
                  DebugPoolTy *srcPool,
                  char *dst,
                  const char *src,
                  const uint8_t complete,
                  TAG,
                  SRC_INFO) {
  size_t srcLen = 0, dstLen = 0, maxLen, catLen;
  void *dstBegin = NULL, *dstEnd = NULL;
  void *srcBegin = NULL, *srcEnd = NULL;
  char *dstNulPosition;
  bool terminated = true;
  bool srcObjFound, dstObjFound;
  const bool srcComplete = ARG1_COMPLETE(complete);
  const bool dstComplete = ARG2_COMPLETE(complete);

  // Find the strings' memory objects in the pool.
  if (!(srcObjFound = pool_find(dstPool, (void*)dst, dstBegin, dstEnd)) &&
      srcComplete) {
    std::cout << "Destination string not found in pool\n";
    LOAD_STORE_VIOLATION(dstBegin, dstPool, SRC_INFO_ARGS);
  }
  if (!(dstObjFound = pool_find(srcPool, (void*)src, srcBegin, srcEnd)) &&
      dstComplete) {
    std::cout << "Source string not found in pool!\n";
    LOAD_STORE_VIOLATION(srcBegin, srcPool, SRC_INFO_ARGS);
  }

  // Check if both src and dst are terminated, if they were found in their pool.
  if (dstObjFound && !isTerminated(dst, dstEnd, dstLen)) {
    terminated = false;
    std::cout << "Destination not terminated within bounds\n";
    C_LIBRARY_VIOLATION(dst, dstPool, "strcat", SRC_INFO_ARGS);
  }
  if (srcObjFound && !isTerminated(src, srcEnd, srcLen)) {
    terminated = false;
    std::cout << "Source not terminated within bounds\n";
    C_LIBRARY_VIOLATION(src, srcPool, "strcat", SRC_INFO_ARGS);
  }

  // The remainder of the checks require both memory objects to have been found
  // in the pool.
  if (srcObjFound && dstObjFound) {

    // If both src and dst are terminated, check for string overlap.
    // Overlap occurs exactly when they share the same nul terminator in memory.
    if (terminated && &dst[dstLen] == &src[srcLen]) {
      std::cout << "Concatenating overlapping strings is undefined\n";
      C_LIBRARY_VIOLATION(dst, dstPool, "strcat", SRC_INFO_ARGS);
    }

    // maxLen is the maximum length string dst can hold without going out of bounds
    maxLen  = (char *) dstEnd - dst;
    // catLen is the length of the string resulting from concatenation.
    catLen  = srcLen + dstLen;

    // Check if the concatenation writes out of bounds.
    if (catLen > maxLen) {
      std::cout << "Concatenation violated destination bounds!\n";
      WRITE_VIOLATION(dstBegin, dstPool, maxLen + 1, catLen + 1, SRC_INFO_ARGS);
    }

    // Append at the end of dst so concatenation doesn't have to scan dst again.
    dstNulPosition = &dst[dstLen];
    strncat(dstNulPosition, src, srcLen);
    return dst;
  }
  else
    return strcat(dst, src);
}


/*
 * pool_strncat()
 *
 * This is the non-debug version of pool_strncat_debug().
 *
 */
char *
pool_strncat(DebugPoolTy *dstPool,
             DebugPoolTy *srcPool,
             char *dst,
             const char *src,
             size_t n,
             const uint8_t complete)
{
  return pool_strncat_debug(dstPool, srcPool, dst, src, n, complete,
    DEFAULT_TAG, DEFAULT_SRC_INFO);
}


/**
 * Secure runtime wrapper function to replace strncat()
 *
 * Appends at most n characters of src onto the end of the string dst
 * and then adds a nul terminator.
 *
 * Checks for the following:
 *  - src and dst are non-null
 *  - dst is terminated
 *  - dst has enough space to hold the whole concatention
 *  - src and dst do not overlap
 *  - if src is unterminated, the first n characters of src fall within
 *    the boundaries of src.
 *
 * @param  dstPool  Pool handle for destination string
 * @param  srcPool  Pool handle for source string
 * @param  dst      Destination string pointer
 * @param  src      Source string pointer
 * @param  n        Number of characters to copy over
 * @param  complete Completeness bitwise vector
 * @param  TAG      Tag debug information
 * @param  SRC_INFO Source and line number information
 * @return          Destination string pointer
 *
 */
char *
pool_strncat_debug(DebugPoolTy *dstPool,
                   DebugPoolTy *srcPool,
                   char *dst,
                   const char *src,
                   size_t n,
                   const uint8_t complete,
                   TAG,
                   SRC_INFO) {
  void *dstBegin = NULL, *dstEnd = NULL;
  void *srcBegin = NULL, *srcEnd = NULL;
  size_t dstLen = 0, srcLen = 0, maxLen, catLen, srcAmt;
  char *dstNulPosition;
  bool dst_terminated = true;
  bool srcObjFound, dstObjFound;
  const bool srcComplete = ARG1_COMPLETE(complete);
  const bool dstComplete = ARG2_COMPLETE(complete);
 
  // Retrieve destination and source string memory objects from pool.
  if (!(dstObjFound = pool_find(dstPool, (void *) dst, dstBegin, dstEnd)) &&
      dstComplete) {
    std::cout << "Destination string not found in pool!\n";  
    LOAD_STORE_VIOLATION(dstBegin, dstPool, SRC_INFO_ARGS);
  }
  if (!(srcObjFound = pool_find(srcPool, (void *) src, srcBegin, srcEnd)) &&
      srcComplete) {
    std::cout << "Source string not found in pool!\n";  
    LOAD_STORE_VIOLATION(srcBegin, srcPool, SRC_INFO_ARGS);
  }

  // Check if dst is nul terminated.
  if (dstObjFound && !isTerminated(dst, dstEnd, dstLen)) {
    dst_terminated = false;
    std::cout << "String not terminated within bounds\n";
    C_LIBRARY_VIOLATION(dst, dstPool, "strncat", SRC_INFO_ARGS);
  }

  // According to POSIX, src doesn't have to be nul-terminated.
  // If it isn't, ensure strncat that doesn't read beyond the bounds of src.
  if (srcObjFound && !isTerminated(src, srcEnd, srcLen) && srcLen < n) {
    std::cout << "Source object too small\n";
    OOB_VIOLATION(src, srcPool, src, srcLen, SRC_INFO_ARGS);
  }

  // The remaining checks require destination and source objects to be found.
  if (srcObjFound && dstObjFound) {

    // Determine the amount of characters to be copied over from src.
    // This is either n or the length of src, whichever is smaller.
    srcAmt = std::min(srcLen, n);

    // Check for undefined behavior due to overlapping objects.
    // Overlap occurs when the characters to be copied from src
    // end inside the dst string.
    if (dst_terminated && dst < &src[srcAmt] && &src[srcAmt] <= &dst[dstLen]) {
      std::cout << "Concatenating overlapping objects is undefined\n";
      C_LIBRARY_VIOLATION(dst, dstPool, "strncat", SRC_INFO_ARGS);
    }

    // maxLen is the maximum length string dst can hold without overflowing.
    maxLen = (char *) dstEnd - dst;
    // catLen is the length of the string resulting from the concatenation.
    catLen = srcAmt + dstLen;

    // Check if the copy operation would go beyong the bounds of dst.
    if (catLen > maxLen) {
      std::cout << "Concatenation violated destination bounds!\n";
      WRITE_VIOLATION(dst, dstPool, 1+maxLen, 1+catLen, SRC_INFO_ARGS);
    }

    // Start concatenation the end of dst so strncat() doesn't have to scan dst
    // all over again.
    dstNulPosition = &dst[dstLen];
    strncat(dstNulPosition, src, srcAmt);

    // strncat() the returns destination string.
    return dst;
  }
  else
    return strncat(dst, src, n);
}


/*
 * pool_strpbrk()
 *
 * This is the non-debug version of pool_strpbrk_debug().
 *
 */
char *
pool_strpbrk(DebugPoolTy *sp,
             DebugPoolTy *ap,
             const char *s,
             const char *a,
             const uint8_t complete) {
  return pool_strpbrk_debug(sp, ap, s, a, complete,
    DEFAULT_TAG, DEFAULT_SRC_INFO);
}


/**
 * Secure runtime wrapper function to replace strpbrk()
 *
 * Searches for the first instance in s of any character in a, and returns a
 * pointer to that instance, or returns NULL if no instance was found.
 *
 * Attempts to verify that both s and a are valid strings terminated within
 * their memory objects' boundaries.
 *
 * @param   sPool  Pool handle for source string
 * @param   aPool  Pool handle for accept string
 * @param   s      String pointer
 * @param   a      Pointer to string of characters to find
 * @return  Pointer to first instance in s of some character in s, or NULL
 */
char *
pool_strpbrk_debug(DebugPoolTy *sPool,
                   DebugPoolTy *aPool,
                   const char *s,
                   const char *a,
                   const uint8_t complete,
                   TAG,
                   SRC_INFO) {
  validStringCheck(s, sPool, ARG1_COMPLETE(complete), "strpbrk",
    DEFAULT_SRC_INFO);
  validStringCheck(a, aPool, ARG2_COMPLETE(complete), "strpbrk",
    DEFAULT_SRC_INFO);
  return strpbrk((char*)s, a);
}


int
pool_strcmp(DebugPoolTy *s1p,
            DebugPoolTy *s2p, 
            const char *s1, 
            const char *s2,
            const uint8_t complete){
  return pool_strcmp_debug(s1p,s2p,s1,s2,complete, DEFAULT_TAG, DEFAULT_SRC_INFO);
}


/**
 * Secure runtime wrapper function to replace strcmp()
 *
 * @param   str1Pool Pool handle for str1
 * @param   str2Pool Pool handle for str2
 * @param   str1     c string to be compared
 * @param   str2     c string to be compared
 * @return  
 */
int
pool_strcmp_debug(DebugPoolTy *str1Pool,
                  DebugPoolTy *str2Pool, 
                  const char *str1, 
                  const char *str2,
                  const uint8_t complete,
                  TAG,
                  SRC_INFO) {

  size_t str1Size = 0, str2Size = 0;
  void *str1Begin =(void*)str1, *str1End = NULL, *str2Begin = (void*)str2, *str2End = NULL;

  const bool str1Complete = ARG1_COMPLETE(complete);
  const bool str2Complete = ARG2_COMPLETE(complete);
  bool str1ObjFound, str2ObjFound;

  if (!(str1ObjFound = pool_find(str1Pool, (void*)str1, str1Begin, str1End))
      && str1Complete) {
    std::cout << "String 1 not found in pool!\n";
    LOAD_STORE_VIOLATION(str1Begin, str1Pool, SRC_INFO_ARGS);
  }
  if (!(str2ObjFound = pool_find(str2Pool, (void*)str2, str2Begin, str2End))
      && str2Complete) {
    std::cout << "String 2 not found in pool!\n";
    LOAD_STORE_VIOLATION(str1Begin, str1Pool, SRC_INFO_ARGS);
  }

  // Check if strings are terminated.
  if (str1ObjFound && !isTerminated(str1, str1End, str1Size)) {
    std::cout << "String 1 not terminated within bounds!\n";
    OOB_VIOLATION(str1Begin, str1Pool, str1Begin, str1Size, SRC_INFO_ARGS);
  }
  if (str2ObjFound && !isTerminated(str2, str2End, str2Size)) {
    std::cout << "String 2 not terminated within bounds!\n";
    OOB_VIOLATION(str2Begin, str2Pool, str2Begin, str2Size, SRC_INFO_ARGS);
  }

  return strcmp(str1, str2);
}


/**
 * Secure runtime wrapper function to replace memcpy()
 *
 * @param   dstPool  Pool handle for destination memory area
 * @param   srcPool  Pool handle for source memory area
 * @param   dst      Destination memory area
 * @param   src      Source memory area
 * @param   n        Maximum number of bytes to copy
 * @return  Destination memory area
 */
void *
pool_memcpy(DebugPoolTy *dstPool, 
            DebugPoolTy *srcPool, 
            void *dst, 
            const void *src, 
            size_t n,
            const uint8_t complete) {
  return pool_memcpy_debug(dstPool,srcPool,dst,src, n,complete,DEFAULT_TAG, DEFAULT_SRC_INFO);
}


/**
 * Secure runtime wrapper function to replace memcpy()
 *
 * @param   dstPool  Pool handle for destination memory area
 * @param   srcPool  Pool handle for source memory area
 * @param   dst      Destination memory area
 * @param   src      Source memory area
 * @param   n        Maximum number of bytes to copy
 * @return  Destination memory area
 */
void *
pool_memcpy_debug(DebugPoolTy *dstPool, 
                  DebugPoolTy *srcPool, 
                  void *dst, 
                  const void *src, 
                  size_t n,
                  const uint8_t complete,
                  TAG,
                  SRC_INFO){

  size_t dstSize = 0, srcSize = 0;
  void *dstBegin = dst, *dstEnd = NULL, *srcBegin = (char *)src, *srcEnd = NULL;
  const bool dstComplete = ARG1_COMPLETE(complete);
  const bool srcComplete = ARG2_COMPLETE(complete);
  bool dstFound = false, srcFound = false;

  // Retrieve both the destination and source buffer's bounds from the pool handle.
  if(!(dstFound = pool_find(dstPool, (void*)dst, dstBegin, dstEnd)) && dstComplete){
    std::cout<<"Memory object not found in pool!\n";
    LOAD_STORE_VIOLATION(dst,dstPool, SRC_INFO_ARGS);
  }
  if(!(srcFound = pool_find(srcPool, (void*)src, srcBegin, srcEnd)) && srcComplete){
    std::cout<<"Memory object not found in pool!\n";
    LOAD_STORE_VIOLATION(src,srcPool, SRC_INFO_ARGS);
  }

  // Calculate the maximum number of bytes to copy.
  dstSize = (char *)dstEnd - (char *)dst + 1;
  srcSize = (char *)srcEnd - (char *)src + 1;

  // The remainder of the checks require the objects to have been located in
  // their pools.
  if (dstFound && srcFound){
    if (n > srcSize || n > dstSize){
      std::cout << "Cannot copy more bytes than the size of the source!\n";
      WRITE_VIOLATION(srcBegin, srcPool, dstSize, srcSize, SRC_INFO_ARGS);
    }

    if(isOverlapped(dst,(const char*)dst+n-1,src,(const char*)src+n-1)){ 
      std::cout<<"Two memory objects overlap each other!/n";
      LOAD_STORE_VIOLATION(dst,dstPool, SRC_INFO_ARGS);
    }
  }

  memcpy(dst, src, n);

  return dst;
}


/**
 * Secure runtime wrapper function to replace memmove()
 *
 * @param   dstPool  Pool handle for destination memory area
 * @param   srcPool  Pool handle for source memory area
 * @param   dst      Destination memory area
 * @param   src      Source memory area
 * @param   n        Maximum number of bytes to copy
 * @return  Destination memory area
 */
void *
pool_memmove(DebugPoolTy *dstPool, 
             DebugPoolTy *srcPool, 
             void *dst, 
             const void *src, 
             size_t n,
             const uint8_t complete) {
  return pool_memmove_debug(dstPool,srcPool,dst,src, n,complete, DEFAULT_TAG, DEFAULT_SRC_INFO);
}


/**
 * Secure runtime wrapper function to replace memmove()
 *
 * @param   dstPool  Pool handle for destination memory area
 * @param   srcPool  Pool handle for source memory area
 * @param   dst      Destination memory area
 * @param   src      Source memory area
 * @param   n        Maximum number of bytes to copy
 * @return  Destination memory area
 */
void *
pool_memmove_debug(DebugPoolTy *dstPool, 
                   DebugPoolTy *srcPool, 
                   void *dst, 
                   const void *src, 
                   size_t n,
                   const uint8_t complete,
                   TAG,
                   SRC_INFO){

  size_t dstSize = 0, srcSize = 0, stop = 0;
  void *dstBegin = dst, *dstEnd = NULL, *srcBegin = (char *)src, *srcEnd = NULL;

  //assert(dstPool && srcPool && dst && src && "Null pool parameters!");
  const bool dstComplete = ARG1_COMPLETE(complete);
  const bool srcComplete = ARG2_COMPLETE(complete);
  bool dstFound, srcFound;

  // Retrieve both the destination and source buffer's bounds from the pool handle.
  if(!(dstFound = pool_find(dstPool, (void*)dst, dstBegin, dstEnd))
     && dstComplete){
    std::cout<<"Memory object not found in pool!\n";
    LOAD_STORE_VIOLATION(dst,dstPool, SRC_INFO_ARGS);
  }
  if(!(srcFound = pool_find(srcPool, (void*)src, srcBegin, srcEnd))
     && srcComplete){
    std::cout<<"Memory object not found in pool!\n";
    LOAD_STORE_VIOLATION(src,srcPool, SRC_INFO_ARGS);
  }

  if (dstFound && srcFound)
  {
    // Calculate the maximum number of bytes to copy.
    dstSize = (char *)dstEnd - (char *)dst + 1;
    srcSize = (char *)srcEnd - (char *)src + 1;
    stop = std::min(n, srcSize);
    if (n > srcSize || n > dstSize) {
      std::cout << "Cannot copy more bytes than the size of the source!\n";
      WRITE_VIOLATION(srcBegin, srcPool, dstSize, srcSize, SRC_INFO_ARGS);
    }
  }

  memmove(dst, src, stop);

  return dst;
}

#if !defined(__APPLE__)
/**
 * Secure runtime wrapper function to replace mempcpy()
 *
 * @param   dstPool  Pool handle for destination memory area
 * @param   srcPool  Pool handle for source memory area
 * @param   dst      Destination memory area
 * @param   src      Source memory area
 * @param   n        Maximum number of bytes to copy
 * @return  Byte following the last written byte
 */
void *
pool_mempcpy(DebugPoolTy *dstPool, 
             DebugPoolTy *srcPool, 
             void *dst, 
             const void *src, 
             size_t n,
             const uint8_t complete) {
  return pool_mempcpy_debug(dstPool,srcPool,dst,src, n,complete,DEFAULT_TAG, DEFAULT_SRC_INFO);
}


/**
 * Secure runtime wrapper function to replace mempcpy()
 *
 * @param   dstPool  Pool handle for destination memory area
 * @param   srcPool  Pool handle for source memory area
 * @param   dst      Destination memory area
 * @param   src      Source memory area
 * @param   n        Maximum number of bytes to copy
 * @return  Byte following the last written byte
 */
void *
pool_mempcpy_debug(DebugPoolTy *dstPool, 
                   DebugPoolTy *srcPool, 
                   void *dst, 
                   const void *src, 
                   size_t n,
                   const uint8_t complete,
                   TAG,
                   SRC_INFO){

  size_t dstSize = 0, srcSize = 0;
  void *dstBegin = dst, *dstEnd = NULL, *srcBegin = (char *)src, *srcEnd = NULL;
  const bool dstComplete = ARG1_COMPLETE(complete);
  const bool srcComplete = ARG2_COMPLETE(complete);
  bool dstFound, srcFound;

  // assert(dstPool && srcPool && dst && src && "Null pool parameters!");

  // Retrieve both the destination and source buffer's bounds from the pool handle.
  if(!(dstFound = pool_find(dstPool, (void*)dst, dstBegin, dstEnd))
     && dstComplete){
    std::cout<<"Memory object not found in pool!\n";
    LOAD_STORE_VIOLATION(dst,dstPool, SRC_INFO_ARGS);
  }
  if(!(srcFound = pool_find(srcPool, (void*)src, srcBegin, srcEnd))
     && srcComplete){
    std::cout<<"Memory object not found in pool!\n";
    LOAD_STORE_VIOLATION(src,srcPool, SRC_INFO_ARGS);
  }

  if (dstFound && srcFound)
  {
    // Calculate the maximum number of bytes to copy.
    dstSize = (char *)dstEnd - (char *)dst + 1;
    srcSize = (char *)srcEnd - (char *)src + 1;
    // Check that copy size is too big
    if (n > srcSize || n > dstSize) {
      std::cout << "Cannot copy more bytes than the size of the source!\n";
      WRITE_VIOLATION(srcBegin, srcPool, dstSize, srcSize, SRC_INFO_ARGS);
    }
    // Check if two memory object overlap
    if(isOverlapped(dst,(const char*)dst+n-1,src,(const char*)src+n-1)){ 
      std::cout<<"Two memory objects overlap each other!/n";
      LOAD_STORE_VIOLATION(dst,dstPool, SRC_INFO_ARGS);
    }
  }
 
  return  mempcpy(dst, src, n);
}
#endif


/**
 * Secure runtime wrapper function to replace memset()
 *
 * @param   stringPool  Pool handle for the string
 * @param   string      String pointer
 * @param   c           an int value to be set
 * @param   n           number of bytes to be set
 * @return  Pointer to memory area
 */
void *pool_memset(DebugPoolTy *stringPool, 
             void *string, 
             int c, 
             size_t n,
             const uint8_t complete) {
  return pool_memset_debug(stringPool, string, c, n,complete, DEFAULT_TAG, DEFAULT_SRC_INFO);

}


/**
 * Secure runtime wrapper function to replace memset()
 *
 * @param   stringPool  Pool handle for the string
 * @param   string      String pointer
 * @param   c           an int value to be set
 * @param   n           number of bytes to be set
 * @return  Pointer to memory area
 */
void *
pool_memset_debug(DebugPoolTy *stringPool, 
                  void *string, 
                  int c, 
                  size_t n,
                  const uint8_t complete,
                  TAG,
                  SRC_INFO){
  size_t stringSize = 0;
  void *stringBegin = string, *stringEnd = NULL;
  const bool objComplete = ARG1_COMPLETE(complete);
  bool objFound;

  assert(stringPool && string && "Null pool parameters!");

  //// Retrieve both the destination and source buffer's bounds from the pool handle.
  if(!(objFound = pool_find(stringPool, (void*)string, stringBegin, stringEnd))
     && objComplete){
    std::cout<<"Memory object not found in pool!\n";
    LOAD_STORE_VIOLATION(string,stringPool, SRC_INFO_ARGS);
  }

  stringSize = (char *)stringEnd - (char *)string + 1;
  if (objFound && n > stringSize) {
    std::cout << "Cannot write more bytes than the size of the destination string!\n";
    WRITE_VIOLATION(stringBegin, stringPool, stringSize, 0, SRC_INFO_ARGS);
  }
  return memset(string, c, n);
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
char *
pool_strcpy(DebugPoolTy *dstPool, 
            DebugPoolTy *srcPool, 
            char *dst, 
            const char *src, 
            const uint8_t complete) {
  return pool_strcpy_debug(dstPool, srcPool, dst, src, complete, DEFAULT_TAG, DEFAULT_SRC_INFO);
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
char *
pool_strcpy_debug(DebugPoolTy *dstPool, 
                  DebugPoolTy *srcPool, 
                  char *dst, 
                  const char *src, 
                  const uint8_t complete, 
                  TAG,
                  SRC_INFO) {
  size_t dstSize = 0, srcSize = 0, len = 0;
  void *dstBegin = dst, *dstEnd = NULL, *srcBegin = (char *)src, *srcEnd = NULL;
  const bool dstComplete = ARG1_COMPLETE(complete);
  const bool srcComplete = ARG2_COMPLETE(complete);
  bool dstFound, srcFound;

  // Ensure all valid pointers.
  // assert(dstPool && srcPool && dst && src && "Null pool parameters!");
  
  // Retrieve both the destination and source buffer's bounds from the pool handle.
  if(!(dstFound = pool_find(dstPool, (void*)dst, dstBegin, dstEnd))
     && dstComplete) {
    std::cout<<"Memory object not found in pool!\n";
    LOAD_STORE_VIOLATION(dst,dstPool, SRC_INFO_ARGS);
  }
  if(!(srcFound = pool_find(srcPool, (void*)src, srcBegin, srcEnd))
     && srcComplete) {
    std::cout<<"Memory object not found in pool!\n";
    LOAD_STORE_VIOLATION(src,srcPool, SRC_INFO_ARGS);
  }

  // Calculate the maximum number of bytes to copy.
  dstSize = (char *)dstEnd - (char *)dst + 1;
  srcSize = (char *)srcEnd - (char *)src + 1;
  if (srcFound)
  {
    len = strnlen(src,srcSize);

    if (len == srcSize) {
      std::cout << "Source string is not NULL terminated!\n";
      OOB_VIOLATION(src,srcPool,src,len,SRC_INFO_ARGS);
    }

    if (dstFound)
    {
      if (len+1 > dstSize) {
        std::cout << "Cannot copy more bytes than the size of the source!\n";
        WRITE_VIOLATION(dstBegin, dstPool, dstSize, srcSize, SRC_INFO_ARGS);
      }

      if(isOverlapped(dst,(const char*)dst+len,src,(const char*)src+len)){ 
        std::cout<<"Two memory objects overlap each other!\n";
        LOAD_STORE_VIOLATION(dst,dstPool, SRC_INFO_ARGS);
      }
    }
  }

  strncpy(dst, src, len+1);

  return dst;
}


/**
 * Secure runtime wrapper function to replace strlen()
 *
 * @param   stringPool  Pool handle for the string
 * @param   string      String pointer
 * @return  Length of the string
 */
size_t
pool_strlen(DebugPoolTy *stringPool, 
            const char *string, 
            const uint8_t complete) {
  return pool_strlen_debug(stringPool, string, 
    complete, DEFAULT_TAG, DEFAULT_SRC_INFO);
}

/**
 * Secure runtime wrapper function to replace strlen()
 *
 * @param   strPool  Pool handle for the string
 * @param   str      String pointer
 * @return  Length of the string
 */
size_t
pool_strlen_debug(DebugPoolTy *strPool, 
                  const char *str, 
                  const uint8_t complete, 
                  TAG, 
                  SRC_INFO) {
  const bool strComplete = ARG1_COMPLETE(complete);
  bool strFound;
  size_t len = 0;
  void *strBegin = 0, *strEnd = 0;

  if (!(strFound = pool_find(strPool, (void *) str, strBegin, strEnd)) &&
      strComplete)
  {
    LOAD_STORE_VIOLATION(str, strPool, SRC_INFO_ARGS);
  }
  if (strFound)
  {
    if (!isTerminated(str, strEnd, len))
      C_LIBRARY_VIOLATION(str, strPool, "strlen", SRC_INFO_ARGS);
    else
      return len;
  }
  return strlen(str);
}

//// FIXME: WHen it is tested, compiled program outputs "Illegal Instruction"
/**
 * Secure runtime wrapper function to replace strncpy()
 *
 * @param   dstPool  Pool handle for destination buffer
 * @param   srcPool  Pool handle for source string
 * @param   dst      Destination string pointer
 * @param   src      Source string pointer
 * @param   n        Maximum number of bytes to copy
 * @return  Destination string pointer
 */
char *
pool_strncpy(DebugPoolTy *dstPool, 
             DebugPoolTy *srcPool, 
             char *dst, 
             const char *src, 
             size_t n,
             const uint8_t complete){
  return pool_strncpy_debug(dstPool,srcPool,dst,src, n,
    complete,DEFAULT_TAG, DEFAULT_SRC_INFO);
}

/**
 * Secure runtime wrapper function to replace strncpy()
 *
 * @param   dstPool  Pool handle for destination buffer
 * @param   srcPool  Pool handle for source string
 * @param   dst      Destination string pointer
 * @param   src      Source string pointer
 * @param   n        Maximum number of bytes to copy
 * @return  Destination string pointer
 */
char *
pool_strncpy_debug(DebugPoolTy *dstPool, 
                   DebugPoolTy *srcPool, 
                   char *dst, 
                   const char *src, 
                   size_t n, 
                   const uint8_t complete, 
                   TAG, 
                   SRC_INFO){
  size_t dstSize = 0, srcSize = 0, stop = 0;
  void *dstBegin = dst, *dstEnd = NULL, *srcBegin = (char *)src, *srcEnd = NULL;
  const bool dstComplete = ARG1_COMPLETE(complete);
  const bool srcComplete = ARG2_COMPLETE(complete);
  bool dstFound, srcFound;

  // assert(dstPool && srcPool && dst && src && "Null pool parameters!");

  // Retrieve both the destination and source buffer's bounds from the pool handle.
  if(!(dstFound = pool_find(dstPool, (void*)dst, dstBegin, dstEnd)) &&
     dstComplete) {
    std::cout<<"Memory object not found in pool!\n";
    LOAD_STORE_VIOLATION(dst,dstPool, SRC_INFO_ARGS);
  }
  if(!(srcFound = pool_find(srcPool, (void*)src, srcBegin, srcEnd))
     && srcComplete){
    std::cout<<"Memory object not found in pool!\n";
    LOAD_STORE_VIOLATION(src,srcPool, SRC_INFO_ARGS);
  }

  // Calculate the maximum number of bytes to copy.
  dstSize = (char *)dstEnd - (char *)dst + 1;
  srcSize = (char *)srcEnd - (char *)src + 1;
  stop = strnlen(src,srcSize);
  // If source string is not bounded and copy length is longer than the source object
  // Behavior is undefined
  if (srcFound && stop==srcSize && n > srcSize) {
    std::cout << "String is not bounded and copy length is out of bound!\n";
    WRITE_VIOLATION(srcBegin, srcPool, dstSize, srcSize, SRC_INFO_ARGS);
  }
  // Check if destination will be overflowed
  if (dstFound && n > dstSize) {
    std::cout << "Cannot copy more bytes than the size of the source!\n";
    WRITE_VIOLATION(srcBegin, srcPool, dstSize, srcSize, SRC_INFO_ARGS);
  }
  // Check if two strings are over lapped
  if(srcFound && dstFound &&
     isOverlapped(dst,(const char*)dst+stop-1,src,(const char*)src+stop-1)){ 
    std::cout<<"Two memory objects overlap each other!/n";
    LOAD_STORE_VIOLATION(dst,dstPool, SRC_INFO_ARGS);
  }
  // Copy string 
  strncpy_asm(dst, src, stop+1);
  // Check whether result string is NULL terminated
  // FIXME: Is this necessary?
  if(dstFound && !isTerminated(dst,dstEnd,stop)){
    std::cout<<"NULL terminator is not copied!\n";
    OOB_VIOLATION(dst,dstPool,dst,stop,SRC_INFO_ARGS);
  }
  // Pad with zeros
  memset(dst+stop+1,0,n-stop-1);

  return dst;
}

/**
 * Secure runtime wrapper function to replace strnlen()
 *
 * @param   stringPool  Pool handle for the string
 * @param   string      String pointer
 * @return  Length of the string
 */
size_t
pool_strnlen(DebugPoolTy *stringPool, 
             const char *string, 
             size_t maxlen,
             const uint8_t complete) {
  return pool_strnlen_debug(stringPool, string, maxlen, 
    complete, DEFAULT_TAG, DEFAULT_SRC_INFO);
}

/**
 * Secure runtime wrapper function to replace strnlen()
 *
 * @param   stringPool  Pool handle for the string
 * @param   string      String pointer
 * @return  Length of the string
 */
size_t
pool_strnlen_debug(DebugPoolTy *stringPool, 
                   const char *string, 
                   size_t maxlen, 
                   const uint8_t complete, 
                   TAG, 
                   SRC_INFO) {
  size_t len = 0, difflen = 0;
  void *stringBegin = (char *)string, *stringEnd = NULL;
  const bool strComplete = ARG1_COMPLETE(complete);
  bool strFound;

  // assert(stringPool && string && "Null pool parameters!");
  // Retrieve string from the pool
  if(!(strFound = pool_find(stringPool, (void*)string, stringBegin, stringEnd))
     && strComplete) {
    std::cout<<"String not found in pool!\n";
    LOAD_STORE_VIOLATION(string,stringPool, SRC_INFO_ARGS);
  }

  difflen = (char *)stringEnd - (char *)string +1;
  len = strnlen(string, difflen);
  // If the string is not terminated within range and maxlen is bigger than object size
  if(strFound && maxlen > len && len==difflen){
    std::cout<<"String is not bounded!\n";
    OOB_VIOLATION(string,stringPool,string,difflen,SRC_INFO_ARGS);
  }
  return len;
}

/**
 * Secure runtime wrapper function to replace strncmp()
 *
 * @param   str1Pool Pool handle for string1
 * @param   str2Pool Pool handle for string2
 * @param   num      Maximum number of chars to compare
 * @param   str1     string1 to be compared
 * @param   str2     string2 to be compared
 * @return  position of first different character, or 0 it they are the same
 */
int
pool_strncmp(DebugPoolTy *s1p,
             DebugPoolTy *s2p, 
             const char *s1, 
             const char *s2,
             size_t num,
             const uint8_t complete){
  return pool_strncmp_debug(s1p,s2p,s1,s2,num,complete, DEFAULT_TAG, DEFAULT_SRC_INFO);
}

/**
 * Secure runtime wrapper function to replace strncmp()
 *
 * @param   str1Pool Pool handle for string1
 * @param   str2Pool Pool handle for string2
 * @param   num      Maximum number of chars to compare
 * @param   str1     string1 to be compared
 * @param   str2     string2 to be compared
 * @return  position of first different character, or 0 it they are the same
 */
int
pool_strncmp_debug(DebugPoolTy *str1Pool,
                   DebugPoolTy *str2Pool, 
                   const char *str1, 
                   const char *str2,
                   size_t num,
                   const uint8_t complete,
                   TAG,
                   SRC_INFO) {

  size_t str1Size = 0, str2Size = 0;
  void *str1Begin =(void*)str1, *str1End = NULL, *str2Begin = (void*)str2, *str2End = NULL;
  const bool str1Complete = ARG1_COMPLETE(complete);
  const bool str2Complete = ARG2_COMPLETE(complete);
  bool str1Found, str2Found;

  // assert(str1Pool && str2Pool && str2 && str1 && "Null pool parameters!");

  if (!(str1Found = pool_find(str1Pool, (void*)str1, str1Begin, str1End))
      && str1Complete) {
    std::cout << "String 1 not found in pool!\n";
    LOAD_STORE_VIOLATION(str1Begin, str1Pool, SRC_INFO_ARGS);
  }
  if (!(str2Found = pool_find(str2Pool, (void*)str2, str2Begin, str2End))
      && str2Complete) {
    std::cout << "String 2 not found in pool!\n";
    LOAD_STORE_VIOLATION(str1Begin, str1Pool, SRC_INFO_ARGS);
  }

  str1Size = (char*) str1End - str1+1;
  str2Size = (char*) str2End - str2+1;  

  if (str1Found && str1Size<num) {
    std::cout << "Possible read out of bound in string1!\n";
    OOB_VIOLATION(str1Begin, str1Pool, str1Begin, str1Size, SRC_INFO_ARGS);
  }
  if (str2Found && str2Size<num) {
    std::cout << "Possible read out of bound in string2!\n";
    OOB_VIOLATION(str2Begin, str2Pool, str2Begin, str2Size, SRC_INFO_ARGS);
  }

  return strncmp(str1, str2,num);
}

/**
 * Secure runtime wrapper function to replace memcmp()
 *
 * @param   str1Pool Pool handle for memory object1
 * @param   str2Pool Pool handle for memory object1
 * @param   num      Maximum number of bytes to compare
 * @param   str1     memory object1 to be compared
 * @param   str2     memory object2 to be compared
 * @return  position of first different character, or 0 it they are the same
 */
int
pool_memcmp(DebugPoolTy *s1p,
            DebugPoolTy *s2p, 
            const void *s1, 
            const void *s2,
            size_t num,
            const uint8_t complete){
  return pool_memcmp_debug(s1p,s2p,s1,s2,num,complete, DEFAULT_TAG, DEFAULT_SRC_INFO);
}

/**
 * Secure runtime wrapper function to replace memcmp()
 *
 * @param   str1Pool Pool handle for memory object1
 * @param   str2Pool Pool handle for memory object1
 * @param   num      Maximum number of bytes to compare
 * @param   str1     memory object1 to be compared
 * @param   str2     memory object2 to be compared
 * @return  position of first different character, or 0 it they are the same
 */
int
pool_memcmp_debug(DebugPoolTy *str1Pool,
                  DebugPoolTy *str2Pool, 
                  const void *str1, 
                  const void *str2,
                  size_t num,
                  const uint8_t complete,
                  TAG,
                  SRC_INFO) {

  size_t str1Size = 0, str2Size = 0;
  void *str1Begin =(void*)str1, *str1End = NULL, *str2Begin = (void*)str2, *str2End = NULL;
  const bool str1Complete = ARG1_COMPLETE(complete);
  const bool str2Complete = ARG2_COMPLETE(complete);
  bool str1Found, str2Found;

  // assert(str1Pool && str2Pool && str2 && str1 && "Null pool parameters!");

  if (!(str1Found = pool_find(str1Pool, (void*)str1, str1Begin, str1End))
      && str1Complete) {
    std::cout << "String 1 not found in pool!\n";
    LOAD_STORE_VIOLATION(str1Begin, str1Pool, SRC_INFO_ARGS);
  }
  if (!(str2Found = pool_find(str2Pool, (void*)str2, str2Begin, str2End))
      && str2Complete) {
    std::cout << "String 2 not found in pool!\n";
    LOAD_STORE_VIOLATION(str1Begin, str1Pool, SRC_INFO_ARGS);
  }

  str1Size = (char*) str1End - (char*)str1+1;
  str2Size = (char*) str2End - (char*)str2+1;  

  if (str1Found && str1Size<num) {
    std::cout << "Possible read out of bound in string1!\n";
    OOB_VIOLATION(str1Begin, str1Pool, str1Begin, str1Size, SRC_INFO_ARGS);
  }
  if (str2Found && str2Size<num) {
    std::cout << "Possible read out of bound in string2!\n";
    OOB_VIOLATION(str2Begin, str2Pool, str2Begin, str2Size, SRC_INFO_ARGS);
  }

  return memcmp(str1, str2,num);
}

/**
 * Secure runtime wrapper function to replace strspn()
 *
 * @param   str1Pool Pool handle for str1
 * @param   str2Pool Pool handle for str2
 * @param   str1     c string to be scanned
 * @param   str2     c string consisted of matching characters
 * @return  length if initial portion of str1, which consists of 
 *          characters only from str2.
 */
int
pool_strspn(DebugPoolTy *s1p,
            DebugPoolTy *s2p, 
            const char *s1, 
            const char *s2,
            const uint8_t complete){

  return pool_strspn_debug(s1p,s2p,s1,s2,complete, DEFAULT_TAG, DEFAULT_SRC_INFO);
}
/**
 * Secure runtime wrapper function to replace strspn()
 *
 * @param   str1Pool Pool handle for str1
 * @param   str2Pool Pool handle for str2
 * @param   str1     c string to be compared
 * @param   str2     c string to be compared
 * @return  length if initial portion of str1, which consists of 
 *          characters only from str2.
 */
int
pool_strspn_debug(DebugPoolTy *str1Pool,
                  DebugPoolTy *str2Pool, 
                  const char *str1, 
                  const char *str2,
                  const uint8_t complete,
                  TAG,
                  SRC_INFO) {

  size_t str1Size = 0, str2Size = 0;
  void *str1Begin =(void*)str1, *str1End = NULL, *str2Begin = (void*)str2, *str2End = NULL;
  const bool str1Complete = ARG1_COMPLETE(complete);
  const bool str2Complete = ARG2_COMPLETE(complete);
  bool str1Found, str2Found;

  // assert(str1Pool && str2Pool && str2 && str1 && "Null pool parameters!");

  if (!(str1Found = pool_find(str1Pool, (void*)str1, str1Begin, str1End))
      && str1Complete) {
    std::cout << "String 1 not found in pool!\n";
    LOAD_STORE_VIOLATION(str1Begin, str1Pool, SRC_INFO_ARGS);
  }
  if (!(str2Found = pool_find(str2Pool, (void*)str2, str2Begin, str2End))
      && str2Complete) {
    std::cout << "String 2 not found in pool!\n";
    LOAD_STORE_VIOLATION(str1Begin, str1Pool, SRC_INFO_ARGS);
  }

  // Check if strings are terminated.
  if (str1Found && !isTerminated(str1, str1End, str1Size)) {
    std::cout << "String 1 not terminated within bounds!\n";
    OOB_VIOLATION(str1Begin, str1Pool, str1Begin, str1Size, SRC_INFO_ARGS);
  }
  if (str2Found && !isTerminated(str2, str2End, str2Size)) {
    std::cout << "String 2 not terminated within bounds!\n";
    OOB_VIOLATION(str2Begin, str2Pool, str2Begin, str2Size, SRC_INFO_ARGS);
  }

  return strspn(str1, str2);
}

/**
 * Secure runtime wrapper function to replace strcspn()
 *
 * @param   str1Pool Pool handle for str1
 * @param   str2Pool Pool handle for str2
 * @param   str1     c string to be scanned
 * @param   str2     c string consisted of matching characters
 * @return  length if initial portion of str1, which does not 
 *          consist of any characters from str2.
 */
int pool_strcspn(DebugPoolTy *s1p,
          DebugPoolTy *s2p, 
                const char *s1, 
                const char *s2,
                const uint8_t complete){

  return pool_strcspn_debug(s1p,s2p,s1,s2,complete, DEFAULT_TAG, DEFAULT_SRC_INFO);
}
/**
 * Secure runtime wrapper function to replace strcspn()
 *
 * @param   str1Pool Pool handle for str1
 * @param   str2Pool Pool handle for str2
 * @param   str1     c string to be compared
 * @param   str2     c string to be compared
 * @return  length if initial portion of str1, which does not 
 *          consist of any characters from str2.
 */
int pool_strcspn_debug(DebugPoolTy *str1Pool,
                       DebugPoolTy *str2Pool, 
                       const char *str1, 
                       const char *str2,
                       const uint8_t complete,
                       TAG,
                       SRC_INFO) {

  size_t str1Size = 0, str2Size = 0;
  void *str1Begin =(void*)str1, *str1End = NULL, *str2Begin = (void*)str2, *str2End = NULL;
  const bool str1Complete = ARG1_COMPLETE(complete);
  const bool str2Complete = ARG2_COMPLETE(complete);
  bool str1Found, str2Found;

  // assert(str1Pool && str2Pool && str2 && str1 && "Null pool parameters!");

  if (!(str1Found = pool_find(str1Pool, (void*)str1, str1Begin, str1End))
      && str1Complete) {
    std::cout << "String 1 not found in pool!\n";
    LOAD_STORE_VIOLATION(str1Begin, str1Pool, SRC_INFO_ARGS);
  }
  if (!(str2Found = pool_find(str2Pool, (void*)str2, str2Begin, str2End))
      && str2Complete) {
    std::cout << "String 2 not found in pool!\n";
    LOAD_STORE_VIOLATION(str1Begin, str1Pool, SRC_INFO_ARGS);
  }

  // Check if strings are terminated.
  if (str1Found && !isTerminated(str1, str1End, str1Size)) {
    std::cout << "String 1 not terminated within bounds!\n";
    OOB_VIOLATION(str1Begin, str1Pool, str1Begin, str1Size, SRC_INFO_ARGS);
  }
  if (str2Found && !isTerminated(str2, str2End, str2Size)) {
    std::cout << "String 2 not terminated within bounds!\n";
    OOB_VIOLATION(str2Begin, str2Pool, str2Begin, str2Size, SRC_INFO_ARGS);
  }

  return strcspn(str1, str2);
}

/**
 * Secure runtime wrapper function to replace memchr()
 *
 * @param   stringPool  Pool handle for the string
 * @param   string      Memory object
 * @param   c           An int value to be found
 * @param   n           Number of bytes to search in
 * @return  Pointer to position where c is first found
 */
void *
pool_memchr(DebugPoolTy *stringPool, 
            void *string, 
            int c, 
            size_t n,
            const uint8_t complete) {
  return pool_memchr_debug(stringPool, string, c, n,complete, DEFAULT_TAG, DEFAULT_SRC_INFO);

}
/**
 * Secure runtime wrapper function to replace memchr()
 *
 * @param   stringPool  Pool handle for the string
 * @param   string      Memory object
 * @param   c           An int value to be found
 * @param   n           Number of bytes to search in
 * @return  Pointer to position where c is first found
 */
void *
pool_memchr_debug(DebugPoolTy *stringPool, 
                  void *string, int c, 
                  size_t n,
                  const uint8_t complete,
                  TAG,
                  SRC_INFO){
  size_t stringSize = 0;
  void *stringBegin = string, *stringEnd = NULL, *stop= NULL;
  const bool strComplete = ARG1_COMPLETE(complete);
  bool strFound;

  //assert(stringPool && string && "Null pool parameters!");

  // Retrieve both the destination and source buffer's bounds from the pool handle.
  if(!(strFound = pool_find(stringPool, (void*)string, stringBegin, stringEnd))
     && strComplete){
    std::cout<<"Memory object not found in pool!\n";
    LOAD_STORE_VIOLATION(string,stringPool, SRC_INFO_ARGS);
  }

  if (strFound)
  {
    stringSize = (char *)stringEnd - (char *)string + 1;
    stop = memchr(string,c,stringSize);
    if (stop && (char*)stop<(char*)string+std::min(n,stringSize))
      return stop;
    else {
      std::cout << "Possible read out of bound in memory object!\n";
      OOB_VIOLATION(stringBegin, stringPool, stringBegin, stringSize, SRC_INFO_ARGS);
      return NULL;
    }
  }
  else
    return memchr(string, c, n);
}

/**
 * Secure runtime wrapper function to replace memccpy()
 *
 * @param   dstPool  Pool handle for destination memory area
 * @param   srcPool  Pool handle for source memory area
 * @param   dst      Destination memory area
 * @param   src      Source memory area
 * @param   c        It stops copying when it sees this char
 * @param   n        Maximum number of bytes to copy
 * @return           A pointer to the first byte after c in dst or, 
 *                   If c was not found in the first n bytes of s2, it returns a null pointer.
 */
void *
pool_memccpy(DebugPoolTy *dstPool, 
             DebugPoolTy *srcPool, 
             void *dst, 
             const void *src, 
             char c,
             size_t n,
             const uint8_t complete) {
  return pool_memccpy_debug(dstPool,srcPool,dst,src, c, n,complete,DEFAULT_TAG, DEFAULT_SRC_INFO);
}


/**
 * Secure runtime wrapper function to replace memccpy()
 *
 * @param   dstPool  Pool handle for destination memory area
 * @param   srcPool  Pool handle for source memory area
 * @param   dst      Destination memory area
 * @param   src      Source memory area
 * @param   c        It stops copying when it sees this char
 * @param   n        Maximum number of bytes to copy
 * @return           A pointer to the first byte after c in dst or, 
 *                   If c was not found in the first n bytes of s2, it returns a null pointer.
 */
void *
pool_memccpy_debug(DebugPoolTy *dstPool, 
                   DebugPoolTy *srcPool, 
                   void *dst, 
                   const void *src, 
                   char c,
                   size_t n,
                   const uint8_t complete,
                   TAG,
                   SRC_INFO){

  size_t dstSize = 0, srcSize = 0;
  void *dstBegin = dst, *dstEnd = NULL, *srcBegin = (char *)src, *srcEnd = NULL, *stop = NULL;
  const bool dstComplete = ARG1_COMPLETE(complete);
  const bool srcComplete = ARG2_COMPLETE(complete);

  bool dstFound, srcFound;

  //assert(dstPool && srcPool && dst && src && "Null pool parameters!");

  // Retrieve both the destination and source buffer's bounds from the pool handle.
  if(!(dstFound = pool_find(dstPool, (void*)dst, dstBegin, dstEnd))
     && dstComplete){
    std::cout<<"Memory object not found in pool!\n";
    LOAD_STORE_VIOLATION(dst,dstPool, SRC_INFO_ARGS);
  }
  if(!(srcFound = pool_find(srcPool, (void*)src, srcBegin, srcEnd))
     && srcComplete){
    std::cout<<"Memory object not found in pool!\n";
    LOAD_STORE_VIOLATION(src,srcPool, SRC_INFO_ARGS);
  }

  if (dstFound && srcFound)
  {
    // Calculate the maximum number of bytes to copy.
    dstSize = (char *)dstEnd - (char *)dst + 1;
    srcSize = (char *)srcEnd - (char *)src + 1;
    stop= memchr((void*)src,c,srcSize);
    if(!stop){
      
      if (n > srcSize) {
        std::cout << "Cannot copy more bytes than the size of the source!\n";
        WRITE_VIOLATION(srcBegin, srcPool, dstSize, srcSize, SRC_INFO_ARGS);
      }

      if (n >dstSize) {
        std::cout << "Cannot copy more bytes than the size of the destination!\n";
        WRITE_VIOLATION(dstBegin, dstPool, dstSize, srcSize, SRC_INFO_ARGS);
      }

      if(isOverlapped(dst,(const char*)dst+n-1,src,(const char*)src+n-1)){ 
        std::cout<<"Two memory objects overlap each other!/n";
        LOAD_STORE_VIOLATION(dst,dstPool, SRC_INFO_ARGS);
      }
    }
    
    if((size_t)((char*)stop-(char*)src+1)>dstSize){
      std::cout << "Cannot copy more bytes than the size of the destination!\n";
      WRITE_VIOLATION(dstBegin, dstPool, dstSize, srcSize, SRC_INFO_ARGS);
    }
  }

  memccpy(dst, src, c, n);

  return dst;
}
