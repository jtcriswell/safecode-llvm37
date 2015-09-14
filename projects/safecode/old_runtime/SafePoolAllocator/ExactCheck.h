/*===- ExactCheck.h - Exact check runtime interface file ------------------===*/
/*                                                                            */
/*                       The LLVM Compiler Infrastructure                     */
/*                                                                            */
/* This file was developed by the LLVM research group and is distributed      */
/* under the University of Illinois Open Source License. See LICENSE.TXT for  */
/* details.                                                                   */
/*                                                                            */
/*===----------------------------------------------------------------------===*/
/*                                                                            */
/*                                                                            */
/*===----------------------------------------------------------------------===*/

#ifndef EXACTCHECK_RUNTIME_H
#define EXACTCHECK_RUNTIME_H

#include "safecode/Config/config.h"

#ifdef __cpluscplus
extern "C" {
#endif
  void * exactcheck(int a, int b, void * result) __attribute__ ((weak));
  void * exactcheck2(signed char *base, signed char *result, unsigned size) __attribute__ ((weak));
  void * exactcheck2_debug (signed char *base, signed char *result, unsigned size, void *, unsigned) __attribute__ ((weak));
  void * exactcheck2a(signed char *base, signed char *result, unsigned size) __attribute__ ((weak));
  void * exactcheck3(signed char *base, signed char *result, signed char * end)__attribute__ ((weak));
#ifdef __cpluscplus
}
#endif

#endif
