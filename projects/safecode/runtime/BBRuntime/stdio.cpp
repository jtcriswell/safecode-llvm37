//===------- stdio.cpp - CStdLib Runtime functions for <stdio.h> ----------===//
// 
//                          The SAFECode Compiler
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file provides CStdLib runtime wrapper versions of functions found in
// <stdio.h>.
//
//===----------------------------------------------------------------------===//

#include <cstdio>

#include "CStdLib.h"

//
// Portions of this file are based on code found in MINIX's source distribution
// in the directory lib/libc/stdio, which has the following license:
//
// Copyright (c) 1987, 1997, 2006, Vrije Universiteit, Amsterdam,
// The Netherlands All rights reserved. Redistribution and use of the MINIX 3
// operating system in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// * Neither the name of the Vrije Universiteit nor the names of the
// software authors or contributors may be used to endorse or promote
// products derived from this software without specific prior written
// permission.
//
// * Any deviations from these conditions require written permission
// from the copyright holder in advance
//
//
// Disclaimer
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS, AUTHORS, AND
// CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
// NO EVENT SHALL PRENTICE HALL OR ANY AUTHORS OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
// BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

//
// Function: pool_fgets()
//
// Description:
//  This is a memory safe replacement for the fgets() function.
//
// Inputs:
//   Pool     - The pool handle for the string to write.
//   s        - The memory buffer into which to read the result.
//   n        - The maximum number of bytes to read.
//   stream   - The FILE * from which to read the data.
//   complete - The Completeness bit vector.
//   TAG      - The Tag information for debugging purposes
//   SRC_INFO - Source file and line number information for debugging purposes
//
char *
pool_fgets_debug (DebugPoolTy * Pool,
                  char * s,
                  int n,
                  FILE * stream,
                  const uint8_t complete,
                  TAG,
                  SRC_INFO) {
  //
  // Determine the completeness of the various pointers.
  //
  const bool BufferComplete = ARG1_COMPLETE(complete);
  
  //
  // Retrieve the buffer's bounds from the pool.  If we cannot find the object
  // and we know everything about what the buffer should be pointing to (i.e.,
  // the check is complete), then report an error.
  //
  bool found;
  void * ObjStart = 0;
  void * ObjEnd = 0;
  if (!(found = pool_find (Pool, s, ObjStart, ObjEnd)) && BufferComplete) {
    LOAD_STORE_VIOLATION (s, Pool, SRC_INFO_ARGS);
  }
  
  register int ch = 0;
  register char *ptr;
  
  ptr = s;
  while (--n > 0 && (ch = getc (stream)) != EOF) {
    //
    // Check if the object is going to be written out of bounds.
    //
    if (found && ptr == &((char *) ObjEnd)[1]) {
      size_t ObjSz = byte_range ((void *) s, ObjEnd);
      WRITE_VIOLATION (ptr, Pool, ObjSz, ObjSz + 1, SRC_INFO_ARGS);
    }
    *ptr++ = ch;
    if (ch == '\n')
      break;
  }
  if (ch == EOF) {
    if (feof (stream)) {
      if (ptr == s) return NULL;
    } else return NULL;
  }
  //
  // Check if the nul terminator is written out of bounds.
  //
  if (found && ptr == &((char *) ObjEnd)[1]) {
    size_t ObjSz = byte_range ((void *) s, ObjEnd);
    WRITE_VIOLATION (ptr, Pool, ObjSz, ObjSz + 1, SRC_INFO_ARGS);
  }
  *ptr = '\0';
  return s;
}

char *
pool_fgets (DebugPoolTy * Pool,
            char * s,
            int n,
            FILE * stream,
            const uint8_t complete) {
  return pool_fgets_debug (Pool, s, n, stream, complete, DEFAULTS);
}

//
// Function: pool_fputs()
//
// Description:
//  This is a memory safe replacement for the fputs() function.
//
// Inputs:
//   Pool     - The pool handle for the string to output.
//   s        - The memory buffer from which to read the result.
//   stream   - The FILE * into which to write the data.
//   complete - The Completeness bit vector.
//   TAG      - The Tag information for debugging purposes
//   SRC_INFO - Source file and line number information for debugging purposes
//
int
pool_fputs_debug (DebugPoolTy * Pool,
                  char * s,
                  FILE * stream,
                  const uint8_t complete,
                  TAG,
                  SRC_INFO) {
  validStringCheck (s, Pool, ARG1_COMPLETE(complete), "fputs", SRC_INFO_ARGS);
  return fputs (s, stream);
}

int
pool_fputs (DebugPoolTy * Pool,
            char * s,
            FILE * stream,
            const uint8_t complete) {
  return pool_fputs_debug (Pool, s, stream, complete, DEFAULTS);
}

//
// Function: pool_gets()
//
// Description:
//  This is a "memory safe" replacement for the gets() function.
//
// Inputs:
//   Pool     - The pool handle for the string to output.
//   s        - The memory buffer into which to write the result.
//   complete - The Completeness bit vector.
//   TAG      - The Tag information for debugging purposes
//   SRC_INFO - Source file and line number information for debugging purposes
//
//
// NOTE:
//   This code is very close to that of pool_fgets(), with the following
//   changes:
//     - No limit on input size is set.
//     - Reading is done from stdin.
//     - No final newline is ever appended.
//  
char *
pool_gets_debug (DebugPoolTy * Pool,
                 char * s,
                 const uint8_t complete,
                 TAG,
                 SRC_INFO) {
  //
  // Determine the completeness of the various pointers.
  //
  const bool BufferComplete = ARG1_COMPLETE(complete);
  
  //
  // Retrieve the buffer's bounds from the pool.  If we cannot find the object
  // and we know everything about what the buffer should be pointing to (i.e.,
  // the check is complete), then report an error.
  //
  bool found;
  void * ObjStart = 0;
  void * ObjEnd = 0;
  if (!(found = pool_find (Pool, s, ObjStart, ObjEnd)) && BufferComplete) {
    LOAD_STORE_VIOLATION (s, Pool, SRC_INFO_ARGS);
  }
  
  register int ch;
  register char *ptr;
  
  ptr = s;
  while ((ch = getc(stdin)) != EOF) {
    //
    // Exit the loop immediately upon reading a newline.
    //
    if ( ch == '\n')
      break;
    //
    // Check if the object is going to be written out of bounds.
    //
    if (found && ptr == &((char *) ObjEnd)[1]) {
      size_t ObjSz = byte_range ((void *) s, ObjEnd);
      WRITE_VIOLATION (ptr, Pool, ObjSz, ObjSz + 1, SRC_INFO_ARGS);
    }
    *ptr++ = ch;
  }
  if (ch == EOF) {
    if (feof (stdin)) {
      if (ptr == s) return NULL;
    } else return NULL;
  }
  //
  // Check if the nul terminator is written out of bounds.
  //
  if (found && ptr == &((char *) ObjEnd)[1]) {
    size_t ObjSz = byte_range ((void *) s, ObjEnd);
    WRITE_VIOLATION (ptr, Pool, ObjSz, ObjSz + 1, SRC_INFO_ARGS);
  }
  *ptr = '\0';
  return s;
}

char *
pool_gets (DebugPoolTy * Pool,
           char * s,
           const uint8_t complete) {
  return pool_gets_debug (Pool, s, complete, DEFAULTS);
}

//
// Function: pool_puts()
//
// Description:
//  This is a memory safe replacement for the puts() function.
//
// Inputs:
//   Pool     - The pool handle for the string to output.
//   s        - The memory buffer from which to read the result.
//   complete - The Completeness bit vector.
//   TAG      - The Tag information for debugging purposes
//   SRC_INFO - Source file and line number information for debugging purposes
//
int
pool_puts_debug (DebugPoolTy * Pool,
                  char * s,
                  const uint8_t complete,
                  TAG,
                  SRC_INFO) {
  validStringCheck (s, Pool, ARG1_COMPLETE(complete), "fputs", SRC_INFO_ARGS);
  return puts (s);
}

int
pool_puts (DebugPoolTy * Pool,
           char * s,
           const uint8_t complete) {
  return pool_puts_debug (Pool, s, complete, DEFAULTS);
}

//
// Function: pool_fread()
//
// Description:
//  This is a memory safe replacement for the fread() function.
//
// Inputs:
//   Pool     - The pool handle for the buffer to write.
//   ptr      - The buffer into which to write what is read.
//   size     - The size of an object to read.
//   nmemb    - The number of objects to read.
//   stream   - The FILE * from which to read the contents.
//   complete - The Completeness bit vector.
//   TAG      - The Tag information for debugging purposes
//   SRC_INFO - Source file and line number information for debugging purposes
//
size_t
pool_fread_debug (DebugPoolTy *Pool,
                  void *ptr,
                  size_t size,
                  size_t nmemb,
                  FILE *stream,
                  const uint8_t complete,
                  TAG,
                  SRC_INFO) {
  //
  // Determine the completeness of the various pointers.
  //
  const bool BufferComplete = ARG1_COMPLETE(complete);
  
  //
  // Retrieve the buffer's bounds from the pool.  If we cannot find the object
  // and we know everything about what the buffer should be pointing to (i.e.,
  // the check is complete), then report an error.
  //
  bool found;
  void * ObjStart = 0;
  void * ObjEnd = 0;
  if (!(found = pool_find (Pool, ptr, ObjStart, ObjEnd)) && BufferComplete) {
    LOAD_STORE_VIOLATION (ptr, Pool, SRC_INFO_ARGS);
  }
  
  register char *cp = (char *) ptr;
  register int c;
  size_t ndone = 0;
  register size_t s;
  
  if (size)
    while ( ndone < nmemb ) {
      s = size;
      do {
        if ((c = getc (stream)) != EOF) {
          //
          // Check if the object is going to be written out of bounds.
          //
          if (found && (void*) cp == &((char *) ObjEnd)[1]) {
            size_t ObjSz = byte_range (ptr, ObjEnd);
            WRITE_VIOLATION (ptr, Pool, ObjSz, ObjSz + 1, SRC_INFO_ARGS);
          }
          *cp++ = c;
        }
        else
          return ndone;
      } while (--s);
      ndone++;
    }
  return ndone;
}

size_t
pool_fread (DebugPoolTy *Pool,
            void *ptr,
            size_t size,
            size_t nmemb,
            FILE *stream,
            const uint8_t complete) {
  return pool_fread_debug (Pool, ptr, size, nmemb, stream, complete, DEFAULTS);
}

//
// Function: pool_fwrite()
//
// Description:
//  This is a memory safe replacement for the fwrite() function.
//
// Inputs:
//   Pool     - The pool handle for the buffer to write.
//   ptr      - The buffer from which to read the data.
//   size     - The size of an object to read.
//   nmemb    - The number of objects to read.
//   stream   - The FILE * into which to write the contents.
//   complete - The Completeness bit vector.
//   TAG      - The Tag information for debugging purposes
//   SRC_INFO - Source file and line number information for debugging purposes
//
size_t
pool_fwrite_debug (DebugPoolTy *Pool,
                   void *ptr,
                   size_t size,
                   size_t nmemb,
                   register FILE *stream,
                   const uint8_t complete,
                   TAG,
                   SRC_INFO) {
  //
  // Determine the completeness of the various pointers.
  //
  const bool BufferComplete = ARG1_COMPLETE(complete);
  
  //
  // Retrieve the buffer's bounds from the pool.  If we cannot find the object
  // and we know everything about what the buffer should be pointing to (i.e.,
  // the check is complete), then report an error.
  //
  bool found;
  void * ObjStart = 0;
  void * ObjEnd = 0;
  if (!(found = pool_find (Pool, ptr, ObjStart, ObjEnd)) && BufferComplete) {
    LOAD_STORE_VIOLATION (ptr, Pool, SRC_INFO_ARGS);
  }
  
  //
  // Check if the function reads a quantity more than the size of the buffer.
  //
  if (found && size * nmemb > byte_range(ptr, ObjEnd))
    OOB_VIOLATION(ptr, Pool, ptr, size * nmemb, SRC_INFO_ARGS);

  //
  // Perform the write operation.
  //
  return fwrite(ptr, size, nmemb, stream);
}

size_t
pool_fwrite (DebugPoolTy *Pool,
             void *ptr,
             size_t size,
             size_t nmemb,
             register FILE *stream,
             const uint8_t complete) {
  return pool_fwrite_debug(Pool, ptr, size, nmemb, stream, complete, DEFAULTS);
}

//
// Function: pool_tmpnam()
//
// Description:
//  This is a memory safe replacement for the tmpnam() function.
//
// Inputs:
//   Pool     - The pool handle for the buffer to write.
//   str      - A pointer to a buffer or NULL
//   complete - The Completeness bit vector.
//   TAG      - The Tag information for debugging purposes
//   SRC_INFO - Source file and line number information for debugging purposes
//
// Returns:
//  Returns a pointer to a temporary filename
//
char *
pool_tmpnam_debug (DebugPoolTy *Pool,
                   char *str,
                   const uint8_t complete,
                   TAG,
                   SRC_INFO) {
  // str is allowed to be NULL; only perform checks if the string is not null.
  if (str != 0) {
    // The passed pointer should point to an object at least L_tmpnam in size.
    minSizeCheck(Pool, str, ARG1_COMPLETE(complete), L_tmpnam, SRC_INFO_ARGS);
  }
  return tmpnam(str);
}

char *
pool_tmpnam (DebugPoolTy *Pool,
             char *str,
             const uint8_t complete) {
  return pool_tmpnam_debug(Pool, str, complete, DEFAULTS);
}
