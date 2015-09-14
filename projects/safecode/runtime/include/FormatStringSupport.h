//===- FormatStringSupport.h -- Format String Runtime Interface -----------===//
// 
//                     The LLVM Compiler Infrast`ructure
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file defines the interface of the runtime library to replace functions
// that take format strings.
//
//===----------------------------------------------------------------------===//

#ifndef _FORMAT_STRING_SUPPORT_H
#define _FORMAT_STRING_SUPPORT_H

#include "DebugRuntime.h"

#include <stdint.h>

extern "C"
{
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
}

#endif
