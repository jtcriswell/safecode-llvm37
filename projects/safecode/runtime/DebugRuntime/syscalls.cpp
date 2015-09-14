//===------- syscalls.cpp - CStdLib runtime wrappers for system calls -----===//
// 
//                          The SAFECode Compiler
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file provides CStdLib runtime wrapper versions of system calls.
//
//===----------------------------------------------------------------------===//

#include "CStdLib.h"

#include <climits>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

//
// Function: pool_read()
//
// Description:
//  This is a memory safe replacement for the read() function.
//
// Inputs:
//   Pool     - The pool handle for the input buffer
//   Buf      - The input buffer
//   FD       - The file descriptor
//   Count    - The maximum number of bytes to read
//   Complete - The Completeness bit vector
//   TAG      - The Tag information for debugging purposes
//   SRC_INFO - Source file and line number information for debugging purposes
//
ssize_t
pool_read_debug (DebugPoolTy * Pool,
                 void * Buf,
                 int FD,
                 size_t Count,
                 const uint8_t Complete,
                 TAG,
                 SRC_INFO) {
  minSizeCheck (Pool, Buf, ARG1_COMPLETE(Complete), Count, SRC_INFO_ARGS);
  return read (FD, Buf, Count);
}

ssize_t
pool_read (DebugPoolTy * Pool,
           void * Buf,
           int FD,
           size_t Count,
           const uint8_t Complete) {
  return pool_read_debug (Pool, Buf, FD, Count, Complete, DEFAULTS);
}

//
// Function: pool_recv()
//
// Description:
//  This is a memory safe replacement for the recv() function.
//
// Inputs:
//   Pool     - The pool handle for the input buffer
//   Buf      - The input buffer
//   FD       - The file descriptor
//   Len      - The maximum number of bytes to read
//   Flags    - Additional options
//   Complete - The Completeness bit vector
//   TAG      - The Tag information for debugging purposes
//   SRC_INFO - Source file and line number information for debugging purposes
//
ssize_t
pool_recv_debug (DebugPoolTy * Pool,
                 void * Buf,
                 int SockFD,
                 size_t Len,
                 int Flags,
                 const uint8_t Complete,
                 TAG,
                 SRC_INFO) {
  minSizeCheck (Pool, Buf, ARG1_COMPLETE(Complete), Len, SRC_INFO_ARGS);
  return recv (SockFD, Buf, Len, Flags);
}

ssize_t
pool_recv (DebugPoolTy * Pool,
           void * Buf,
           int SockFD,
           size_t Len,
           int Flags,
           const uint8_t Complete) {
  return pool_recv_debug (Pool, Buf, SockFD, Len, Flags, Complete, DEFAULTS);
}

//
// Function: pool_recvfrom()
//
// Description:
//  This is a memory safe replacement for the recvfrom() function.
//
// Inputs:
//   Pool     - The pool handle for the input buffer
//   Buf      - The input buffer
//   FD       - The file descriptor
//   Len      - The maximum number of bytes to read
//   Flags    - Additional options
//   SrcAddr  - Behavior depends on protocol
//   AddrLen  - Behavior depends on protocol
//   Complete - The Completeness bit vector
//   TAG      - The Tag information for debugging purposes
//   SRC_INFO - Source file and line number information for debugging purposes
//
ssize_t
pool_recvfrom_debug (DebugPoolTy * Pool,
                     void * Buf,
                     int SockFD,
                     size_t Len,
                     int Flags,
                     struct sockaddr *SrcAddr,
                     socklen_t *AddrLen,
                     const uint8_t Complete,
                     TAG,
                     SRC_INFO) {
  minSizeCheck (Pool, Buf, ARG1_COMPLETE(Complete), Len, SRC_INFO_ARGS);
  return recvfrom (SockFD, Buf, Len, Flags, SrcAddr, AddrLen);
}

ssize_t
pool_recvfrom (DebugPoolTy * Pool,
               void * Buf,
               int SockFD,
               size_t Len,
               int Flags,
               struct sockaddr *SrcAddr,
               socklen_t *AddrLen,
               const uint8_t Complete) {
  return pool_recvfrom_debug(
           Pool, Buf, SockFD, Len, Flags, SrcAddr, AddrLen, Complete, DEFAULTS
         );
}

//
// Function: pool_write()
//
// Description:
//  This is a memory safe replacement for the write() function.
//
// Inputs:
//   Pool     - The pool handle for the output buffer
//   Buf      - The output buffer
//   FD       - The file descriptor
//   Count    - The maximum number of bytes to write
//   Complete - The Completeness bit vector
//   TAG      - The Tag information for debugging purposes
//   SRC_INFO - Source file and line number information for debugging purposes
//
ssize_t
pool_write_debug (DebugPoolTy * Pool,
                 void * Buf,
                 int FD,
                 size_t Count,
                 const uint8_t Complete,
                 TAG,
                 SRC_INFO) {
  minSizeCheck (Pool, Buf, ARG1_COMPLETE(Complete), Count, SRC_INFO_ARGS);
  return write (FD, Buf, Count);
}

ssize_t
pool_write (DebugPoolTy * Pool,
            void * Buf,
            int FD,
            size_t Count,
            const uint8_t Complete) {
  return pool_write_debug (Pool, Buf, FD, Count, Complete, DEFAULTS);
}

//
// Function: pool_send()
//
// Description:
//  This is a memory safe replacement for the send() function.
//
// Inputs:
//   Pool     - The pool handle for the output buffer
//   Buf      - The output buffer
//   FD       - The file descriptor
//   Len      - The maximum number of bytes to write
//   Flags    - Additional options
//   Complete - The Completeness bit vector
//   TAG      - The Tag information for debugging purposes
//   SRC_INFO - Source file and line number information for debugging purposes
//
ssize_t
pool_send_debug (DebugPoolTy * Pool,
                 void * Buf,
                 int SockFD,
                 size_t Len,
                 int Flags,
                 const uint8_t Complete,
                 TAG,
                 SRC_INFO) {
  minSizeCheck (Pool, Buf, ARG1_COMPLETE(Complete), Len, SRC_INFO_ARGS);
  return send (SockFD, Buf, Len, Flags);
}

ssize_t
pool_send (DebugPoolTy * Pool,
           void * Buf,
           int SockFD,
           size_t Len,
           int Flags,
           const uint8_t Complete) {
  return pool_send_debug (Pool, Buf, SockFD, Len, Flags, Complete, DEFAULTS);
}

//
// Function: pool_sendto()
//
// Description:
//  This is a memory safe replacement for the sendto() function.
//
// Inputs:
//   Pool     - The pool handle for the output buffer
//   Buf      - The output buffer
//   FD       - The file descriptor
//   Len      - The maximum number of bytes to write
//   Flags    - Additional options
//   DestAddr - Behavior depends on protocol
//   DestLen  - Behavior depends on protocol
//   Complete - The Completeness bit vector
//   TAG      - The Tag information for debugging purposes
//   SRC_INFO - Source file and line number information for debugging purposes
//
ssize_t
pool_sendto_debug (DebugPoolTy * Pool,
                   void * Buf,
                   int SockFD,
                   size_t Len,
                   int Flags,
                   const struct sockaddr *DestAddr,
                   socklen_t DestLen,
                   const uint8_t Complete,
                   TAG,
                   SRC_INFO) {
  minSizeCheck (Pool, Buf, ARG1_COMPLETE(Complete), Len, SRC_INFO_ARGS);
  return sendto (SockFD, Buf, Len, Flags, DestAddr, DestLen);
}

ssize_t
pool_sendto (DebugPoolTy * Pool,
             void * Buf,
             int SockFD,
             size_t Len,
             int Flags,
             const struct sockaddr *DestAddr,
             socklen_t DestLen,
             const uint8_t Complete) {
  return pool_sendto_debug(
           Pool, Buf, SockFD, Len, Flags, DestAddr, DestLen, Complete, DEFAULTS
         );
}

//
// Function: pool_readlink()
//
// Description:
//  Implement a memory safe replacement for readlink().
//
ssize_t
pool_readlink_debug (DebugPoolTy * PathPool,
                     DebugPoolTy * BufPool,
                     const char *path,
                     char *buf,
                     size_t len,
                     const uint8_t Complete,
                     TAG,
                     SRC_INFO) {
  //
  // First ensure that the buffer containing the pathname is properly
  // terminated.  Note that a NULL path pointer is okay.
  //
  if (path != NULL) {
    validStringCheck (path, PathPool, ARG1_COMPLETE(Complete), "Generic", SRC_INFO_ARGS);
  }

  //
  // Next, if the caller provided a buffer for the result, make sure that the
  // size is okay.
  //
  if (buf) {
    minSizeCheck (BufPool, buf, ARG2_COMPLETE(Complete), len, SRC_INFO_ARGS);
  }

  return readlink (path, buf, len);
}

ssize_t
pool_readlink (DebugPoolTy * PathPool,
               DebugPoolTy * BufPool,
               const char *path,
               char *buf,
               size_t len,
               const uint8_t Complete) {
  return pool_readlink_debug (PathPool,
                              BufPool,
                              path,
                              buf,
                              len,
                              Complete,
                              DEFAULTS);
}

//
// Function: pool_realpath()
//
// Description:
//  Implement a memory safe replacement for realpath().
//
char *
pool_realpath_debug (DebugPoolTy * PathPool,
                     DebugPoolTy * BufPool,
                     const char *path,
                     char *buf,
                     const uint8_t Complete,
                     TAG,
                     SRC_INFO) {
  //
  // First ensure that the buffer containing the pathname is properly
  // terminated.  Note that a NULL path pointer is okay.
  //
  if (path != NULL) {
    validStringCheck (path, PathPool, ARG1_COMPLETE(Complete), "Generic", SRC_INFO_ARGS);
  }

  //
  // Next, get the path, but permit realpath to allocate the buffer.  This
  // should ensure that the buffer is sufficiently large.
  //
  char *result;
  result = realpath (path, NULL);

  if (buf && result) {
    //
    // If buf is non-NULL and realpath() succeeded, copy the data into the
    // caller's buffer and return the buffer.
    //
    size_t realLength = strlen (result);
    minSizeCheck (BufPool,
                  buf,
                  ARG2_COMPLETE(Complete),
                  realLength + 1,
                  SRC_INFO_ARGS);
    strcpy (buf, result);
    free (result);
    return buf;
  } else if (buf == NULL && result) {
    //
    // If buf is NULL, return the allocated result.
    //
    return result;
  } else {
    //
    // Otherwise, result must be NULL and so an error occurred.
    //
    return NULL;
  }
}

char *
pool_realpath (DebugPoolTy * PathPool,
               DebugPoolTy * BufPool,
               const char *path,
               char *buf,
               const uint8_t Complete) {
  return pool_realpath_debug (PathPool,
                              BufPool,
                              path,
                              buf,
                              Complete,
                              DEFAULTS);
}
