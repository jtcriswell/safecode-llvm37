//===- FloatConversion.h -- Routines for float-to-string conversion     ---===//
// 
//                     The LLVM Compiler Infrast`ructure
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file declares functions used by the runtime format string function
// wrappers for converting floating point numbers into strings.
//
//===----------------------------------------------------------------------===//


#ifndef _FLOAT_CONVERSION_H
#define _FLOAT_CONVERSION_H

extern "C"
{
  extern char *__dtoa(double, int, int, int *, int *, char **);
  extern char *__ldtoa(long double *, int, int, int *, int *, char **);
  extern char *__hdtoa(double, const char *, int, int *, int *, char **);
  extern char *__hldtoa(long double, const char *, int, int *, int *, char **);
  extern void  __freedtoa(char *);
}

#endif
