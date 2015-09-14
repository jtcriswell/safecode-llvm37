/*===- BoundsCheck.h - Bounds check runtime interface file ----------------===*/
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

#ifndef BOUNDSCHECK_RUNTIME_H
#define BOUNDSCHECK_RUNTIME_H

#ifdef __cpluscplus
extern "C" {
#endif

    /* callback when boundary check for an indirect call fails */
  void bchk_ind_fail(void *target) __attribute__ ((weak));

#ifdef __cpluscplus
}
#endif

#endif
