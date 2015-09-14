//===- CStdLibSupport.h -- CStdLib Runtime Interface ----------------------===//
// 
//                     The LLVM Compiler Infrast`ructure
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file defines the interface of the runtime library to replace functions
// from the C Standard Library.
//
//===----------------------------------------------------------------------===//

#ifndef _CSTDLIB_SUPPORT_H
#define _CSTDLIB_SUPPORT_H

#include "safecode/Config/config.h"

#include "DebugRuntime.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>

// Use macros so that I won't pollute the namespace

#define PPOOL      llvm::DebugPoolTy*
#define TAG        unsigned
#define SRC_INFO   const char *, unsigned int
#define COMPLETE   const uint8_t complete
#define DEBUG_INFO TAG, SRC_INFO

extern "C"
{
  // Generic C string checking functions
  void poolcheckstr (PPOOL, const char * str);
  void poolcheckstr_debug (PPOOL, char * str, TAG, SRC_INFO);
  void poolcheckstrui (PPOOL, char * str);
  void poolcheckstrui_debug (PPOOL, char * str, TAG, SRC_INFO);

  // Functions from <stdio.h>, <stdarg.h>

  int pool_vprintf (PPOOL fmtPool, char *fmt, va_list ap, COMPLETE);
  int pool_vprintf_debug (PPOOL fmtPool, char *fmt, va_list ap, COMPLETE, DEBUG_INFO);

  int pool_vfprintf (PPOOL filPool, PPOOL fmtPool, void *fil, char *fmt, va_list ap, COMPLETE);
  int pool_vfprintf_debug (PPOOL filPool, PPOOL fmtPool, void *fil, char *fmt, va_list ap, COMPLETE, DEBUG_INFO);

  int pool_vsprintf (PPOOL strPool, PPOOL fmtPool, char *str, char *fmt, va_list ap, COMPLETE);
  int pool_vsprintf_debug (PPOOL strPool, PPOOL fmtPool, char *str, char *fmt, va_list ap, COMPLETE, DEBUG_INFO);

  // Note argument order differs from library prototype.
  int pool_vsnprintf (PPOOL strPool, PPOOL fmtPool, char *str, char *fmt, size_t sz, va_list ap, COMPLETE);
  int pool_vsnprintf_debug (PPOOL strPool, PPOOL fmtPool, char *str, char *fmt, size_t sz, va_list ap, COMPLETE, DEBUG_INFO);

  int pool_vscanf (PPOOL fmtPool, char *fmt, va_list ap, COMPLETE);
  int pool_vscanf_debug (PPOOL fmtPool, char *fmt, va_list ap, COMPLETE, DEBUG_INFO);

  int pool_vsscanf (PPOOL strPool, PPOOL fmtPool, char *str, char *fmt, va_list ap, COMPLETE);
  int pool_vsscanf_debug (PPOOL strPool, PPOOL fmtPool, char *str, char *fmt, va_list ap, COMPLETE, DEBUG_INFO);

  int pool_vfscanf (PPOOL filPool, PPOOL fmtPool, void *fil, char *fmt, va_list ap, COMPLETE);
  int pool_vfscanf_debug (PPOOL filPool, PPOOL fmtPool, void *fil, char *fmt, va_list ap, COMPLETE, DEBUG_INFO);

  // Functions from <syslog.h>

  // Note argument order differs from library prototype.
  void pool_vsyslog (PPOOL fmtPool, char *fmt, int p, va_list ap, COMPLETE);
  void pool_vsyslog_debug (PPOOL fmtPool, char *fmt, int p, va_list ap, COMPLETE, DEBUG_INFO);

  // Functions from <string.h>

  void *pool_memccpy (PPOOL dstPool, PPOOL srcPool, void *dst, void *src, int c, size_t n, COMPLETE);
  void *pool_memccpy_debug (PPOOL dstPool, PPOOL srcPool, void *dst, void *src, int c, size_t n, COMPLETE, DEBUG_INFO);

  void *pool_memchr (PPOOL sPool, void *s, int c, size_t n, COMPLETE);
  void *pool_memchr_debug (PPOOL sPool, void *s, int c, size_t n, COMPLETE, DEBUG_INFO);

  int pool_memcmp (PPOOL s1p,PPOOL s2p, void *s1, void *s2, size_t num, COMPLETE);
  int pool_memcmp_debug (PPOOL s1p,PPOOL s2p, void *s1, void *s2, size_t num, COMPLETE, DEBUG_INFO);

  void *pool_memcpy (PPOOL dstPool, PPOOL srcPool, void *dst, void *src, size_t n, COMPLETE);
  void *pool_memcpy_debug (PPOOL dstPool, PPOOL srcPool, void *dst, void *src, size_t n, COMPLETE, DEBUG_INFO);

  void *pool_memmove (PPOOL dstPool, PPOOL srcPool, void *dst, void *src, size_t n, COMPLETE);
  void *pool_memmove_debug (PPOOL dstPool, PPOOL srcPool, void *dst, void *src, size_t n, COMPLETE, DEBUG_INFO);

  void *pool_memset (PPOOL sPool, void *s, int c, size_t n, COMPLETE);
  void *pool_memset_debug (PPOOL sPool, void *s, int c, size_t n, COMPLETE, DEBUG_INFO);

  char *pool_strcat (PPOOL dstPool, PPOOL srcPool, char *d, char *s, COMPLETE);
  char *pool_strcat_debug (PPOOL dstPool, PPOOL srcPool, char *d, char *s, COMPLETE, DEBUG_INFO);

  char *pool_strchr (PPOOL sPool, char *s, int c, COMPLETE);
  char *pool_strchr_debug (PPOOL sPool, char *s, int c, COMPLETE, DEBUG_INFO);

  int pool_strcmp (PPOOL str1Pool, PPOOL str2Pool, char *str1, char *str2, COMPLETE);
  int pool_strcmp_debug (PPOOL str1Pool, PPOOL str2Pool, char *str1, char *str2, COMPLETE, DEBUG_INFO);

  int pool_strcoll (PPOOL str1Pool, PPOOL str2Pool, char *str1, char *str2, COMPLETE);
  int pool_strcoll_debug (PPOOL str1Pool, PPOOL str2Pool, char *str1, char *str2, COMPLETE, DEBUG_INFO);

  char *pool_strcpy (PPOOL dstPool, PPOOL srcPool, char *dst, char *src, COMPLETE);
  char *pool_strcpy_debug (PPOOL dstPool, PPOOL srcPool, char *dst, char *src, COMPLETE, DEBUG_INFO);

  size_t pool_strcspn (PPOOL s1p,PPOOL s2p, char *s1, char *s2, COMPLETE);
  size_t pool_strcspn_debug (PPOOL s1p,PPOOL s2p, char *s1, char *s2, COMPLETE, DEBUG_INFO);

    // strdup ()

    // strerror ()

    // strerror_r ()

  size_t pool_strlen (PPOOL stringPool, char *string, COMPLETE);
  size_t pool_strlen_debug (PPOOL stringPool, char *string, COMPLETE, DEBUG_INFO);

  char *pool_strncat (PPOOL dstPool, PPOOL srcPool, char *d, char *s, size_t n, COMPLETE);
  char *pool_strncat_debug (PPOOL dstPool, PPOOL srcPool, char *d, char *s, size_t n, COMPLETE, DEBUG_INFO);

  int pool_strncmp (PPOOL s1p,PPOOL s2p, char *s1, char *s2, size_t num, COMPLETE);
  int pool_strncmp_debug (PPOOL s1p,PPOOL s2p, char *s1, char *s2, size_t num, COMPLETE, DEBUG_INFO);

  char *pool_strncpy (PPOOL dstPool, PPOOL srcPool, char *dst, char *src, size_t n, COMPLETE);
  char *pool_strncpy_debug (PPOOL dstPool, PPOOL srcPool, char *dst, char *src, size_t n, COMPLETE, DEBUG_INFO);

  char *pool_strpbrk (PPOOL sPool, PPOOL aPool, char *s, char *a, COMPLETE);
  char *pool_strpbrk_debug (PPOOL sPool, PPOOL aPool, char *s, char *a, COMPLETE, DEBUG_INFO);

  char *pool_strrchr (PPOOL sPool, char *s, int c, COMPLETE);
  char *pool_strrchr_debug (PPOOL sPool, char *s, int c, COMPLETE, DEBUG_INFO);

  size_t pool_strspn (PPOOL s1p, PPOOL s2p, char *s1, char *s2, COMPLETE);
  size_t pool_strspn_debug (PPOOL s1p,PPOOL s2p, char *s1, char *s2, COMPLETE, DEBUG_INFO);

  char *pool_strstr (PPOOL s1Pool, PPOOL s2Pool, char *s1, char *s2, COMPLETE);
  char *pool_strstr_debug (PPOOL s1Pool, PPOOL s2Pool, char *s1, char *s2, COMPLETE, DEBUG_INFO);

    // strtok ()

    // strtok_r ()

  size_t pool_strxfrm (PPOOL dp, PPOOL sp, char *d, char *s, size_t n, COMPLETE);
  size_t pool_strxfrm_debug (PPOOL dp, PPOOL sp, char *d, char *s, size_t n, COMPLETE, DEBUG_INFO);

  // Extensions to <string.h>

#ifdef HAVE_MEMPCPY
  void *pool_mempcpy (PPOOL dstPool, PPOOL srcPool, void *dst, void *src, size_t n, COMPLETE);
  void *pool_mempcpy_debug (PPOOL dstPool, PPOOL srcPool, void *dst, void *src, size_t n, COMPLETE, DEBUG_INFO);
#endif

#ifdef HAVE_STRCASESTR
  char *pool_strcasestr (PPOOL s1Pool, PPOOL s2Pool, char *s1, char *s2, COMPLETE);
  char *pool_strcasestr_debug (PPOOL s1Pool, PPOOL s2Pool, char *s1, char *s2, COMPLETE, DEBUG_INFO);
#endif

#ifdef HAVE_STPCPY
  char *pool_stpcpy (PPOOL dstPool, PPOOL srcPool, char *dst, char *src, COMPLETE);
  char *pool_stpcpy_debug (PPOOL dstPool, PPOOL srcPool, char *dst, char *src, COMPLETE, DEBUG_INFO);
#endif

#ifdef HAVE_STRNLEN
  size_t pool_strnlen (PPOOL stringPool, char *string, size_t maxlen, COMPLETE);
  size_t pool_strnlen_debug (PPOOL stringPool, char *string, size_t maxlen, COMPLETE, DEBUG_INFO);
#endif

  // Functions from <strings.h>

  int pool_bcmp (PPOOL aPool, PPOOL bPool, void *a, void *b, size_t n, COMPLETE);
  int pool_bcmp_debug (PPOOL aPool, PPOOL bPool, void *a, void *b, size_t n, COMPLETE, DEBUG_INFO);

  void pool_bcopy (PPOOL aPool, PPOOL bPool, void *a, void *b, size_t n, COMPLETE);
  void pool_bcopy_debug (PPOOL aPool, PPOOL bPool, void *a, void *b, size_t n, COMPLETE, DEBUG_INFO);

  void pool_bzero (PPOOL sPool, void *s, size_t n, COMPLETE);
  void pool_bzero_debug (PPOOL sPool, void *s, size_t n, COMPLETE, DEBUG_INFO);

  char *pool_index (PPOOL sPool, char *s, int c, COMPLETE);
  char *pool_index_debug (PPOOL sPool, char *s, int c, COMPLETE, DEBUG_INFO);

  char *pool_rindex (PPOOL sPool, char *s, int c, COMPLETE);
  char *pool_rindex_debug (PPOOL sPool, char *s, int c, COMPLETE, DEBUG_INFO);

  int pool_strcasecmp (PPOOL str1Pool, PPOOL str2Pool, char *str1, char *str2, COMPLETE);
  int pool_strcasecmp_debug (PPOOL str1Pool, PPOOL str2Pool, char *str1, char *str2, COMPLETE, DEBUG_INFO);

  int pool_strncasecmp (PPOOL s1p,PPOOL s2p, char *s1, char *s2, size_t num, COMPLETE);
  int pool_strncasecmp_debug (PPOOL s1p,PPOOL s2p, char *s1, char *s2, size_t num, COMPLETE, DEBUG_INFO);

  // Functions from <stdio.h>

  char * pool_fgets (PPOOL, char * s, int n, FILE * stream, COMPLETE);
  char * pool_fgets_debug (PPOOL, char * s, int n, FILE * stream, COMPLETE, DEBUG_INFO);

  int pool_fputs (PPOOL, char * , FILE * , COMPLETE);
  int pool_fputs_debug (PPOOL, char *, FILE *, COMPLETE, DEBUG_INFO);

  char * pool_gets (PPOOL, char *, COMPLETE);
  char * pool_gets_debug (PPOOL, char *, COMPLETE, DEBUG_INFO);

  int pool_puts (PPOOL, char *, COMPLETE);
  int pool_puts_debug (PPOOL, char *, COMPLETE, DEBUG_INFO);

  size_t pool_fread (PPOOL, void *, size_t, size_t, FILE *, COMPLETE);
  size_t pool_fread_debug (PPOOL, void *, size_t, size_t, FILE *, COMPLETE, DEBUG_INFO);

  size_t pool_fwrite (PPOOL, void *, size_t, size_t, FILE *, COMPLETE);
  size_t pool_fwrite_debug (PPOOL, void *, size_t, size_t, FILE *, COMPLETE, DEBUG_INFO);

  char * pool_tmpnam (PPOOL, char *, COMPLETE);
  char * pool_tmpnam_debug (PPOOL, char *, COMPLETE, DEBUG_INFO);

  // Functions from <unistd.h>

  char * pool_getcwd (PPOOL, char *, size_t, COMPLETE);
  char * pool_getcwd_debug (PPOOL, char *, size_t, COMPLETE, DEBUG_INFO);

  // Functions from <dirent.h>

  int pool_readdir_r (PPOOL, PPOOL, void *, void *, void *, COMPLETE);
  int pool_readdir_r_debug (PPOOL, PPOOL, void *, void *, void *, COMPLETE, DEBUG_INFO);

  // System calls

  ssize_t pool_read (PPOOL, void *, int, size_t, COMPLETE);
  ssize_t pool_read_debug (PPOOL, void *, int, size_t, COMPLETE, DEBUG_INFO);

  ssize_t pool_recv (PPOOL, void *, int, size_t, int, COMPLETE);
  ssize_t pool_recv_debug (PPOOL, void *, int, size_t, int, COMPLETE, DEBUG_INFO);

  ssize_t pool_recvfrom (PPOOL, void *, int, size_t, int, struct sockaddr *, socklen_t *, COMPLETE);
  ssize_t pool_recvfrom_debug (PPOOL, void *, int, size_t, int, struct sockaddr *, socklen_t *, COMPLETE, DEBUG_INFO);
  
  ssize_t pool_write (PPOOL, void *, int, size_t, COMPLETE);
  ssize_t pool_write_debug (PPOOL, void *, int, size_t, COMPLETE, DEBUG_INFO);

  ssize_t pool_send (PPOOL, void *, int, size_t, int, COMPLETE);
  ssize_t pool_send_debug (PPOOL, void *, int, size_t, int, COMPLETE, DEBUG_INFO);

  ssize_t pool_sendto (PPOOL, void *, int, size_t, int, const struct sockaddr *, socklen_t, COMPLETE);
  ssize_t pool_sendto_debug (PPOOL, void *, int, size_t, int, const struct sockaddr *, socklen_t, COMPLETE, DEBUG_INFO);

  ssize_t pool_readlink (PPOOL, PPOOL, const char *path, char *buf, size_t bufsiz, COMPLETE);
  ssize_t pool_readlink_debug (PPOOL, PPOOL, const char *path, char *buf, size_t bufsiz, COMPLETE, DEBUG_INFO);

  char * pool_realpath (PPOOL, PPOOL, const char *path, char *buf, COMPLETE);
  char * pool_realpath_debug (PPOOL, PPOOL, const char *path, char *buf, COMPLETE, DEBUG_INFO);
}

#undef PPOOL
#undef TAG
#undef SRC_INFO
#undef COMPLETE
#undef DEBUG_INFO
#endif
