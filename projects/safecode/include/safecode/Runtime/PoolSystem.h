/*===- PoolSystem.h - Pool check runtime interface to the system ----------===*/
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

#ifndef POOLSYSTEM_RUNTIME_H
#define POOLSYSTEM_RUNTIME_H

#ifdef __cplusplus
extern "C" {
#endif
  /* Functions that need to be provided by the pool allocation run-time */
  void poolcheckfail (const char * msg, int, void*) __attribute__ ((weak));
  void poolcheckfatal (const char * msg, int) __attribute__ ((weak));
  void poolcheckinfo (const char * msg, int) __attribute__ ((weak));
  void poolcheckinfo2 (const char * msg, int, int) __attribute__ ((weak));
  void * poolcheckmalloc (unsigned int size) __attribute__ ((weak));
  void printpoolinfo (void *Pool) __attribute__ ((weak));
  void poolcheckglobals ();
#ifdef __cplusplus
}
#endif

#endif
