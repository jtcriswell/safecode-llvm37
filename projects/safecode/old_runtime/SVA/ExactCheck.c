/*===- ExactCheck.cpp - Implementation of exactcheck functions ------------===*/
/*                                                                            */
/*                       The LLVM Compiler Infrastructure                     */
/*                                                                            */
/* This file was developed by the LLVM research group and is distributed      */
/* under the University of Illinois Open Source License. See LICENSE.TXT for  */
/* details.                                                                   */
/*                                                                            */
/*===----------------------------------------------------------------------===*/
/*                                                                            */
/* This file implements the exactcheck family of functions.                   */
/*                                                                            */
/*===----------------------------------------------------------------------===*/

#include "PoolCheck.h"
#include "PoolSystem.h"
#include "adl_splay.h"

/* Flag whether to print error messages on bounds violations */
int ec_do_fail = 0;

/* Flags whether we're ready to do run-time checks */
extern int pchk_ready;

extern int stat_exactcheck;
extern int stat_exactcheck2;
extern int stat_exactcheck3;

extern int stat_poolcheck;

void * exactcheck2(signed char *base, signed char *result, unsigned size) {
  ++stat_exactcheck2;
  if ((result < base) || (result >= base + size )) {
    if (ec_do_fail) {
      __sva_report("exactcheck2: base=%p result=%p size=0x%x\n",
          base, result, size);
    }
  }
  return result;
}

void funccheck (unsigned num, void *f, void *t1, void *t2, void *t3,
                                       void *t4, void *t5, void *t6) {
  if ((t1) && (f == t1)) return;
  if ((t2) && (f == t2)) return;
  if ((t3) && (f == t3)) return;
  if ((t4) && (f == t4)) return;
  if ((t5) && (f == t5)) return;
  if ((t6) && (f == t6)) return;
  if (ec_do_fail) __sva_report ("funccheck failed: fp=%p\n", f);
  return;
}

struct node {
  void* left;
  void* right;
  char* key;
  char* end;
  void* tag;
};

void * getBegin (void * node) {
  return ((struct node *)(node))->key;
}

void * getEnd (void * node) {
  return ((struct node *)(node))->end;
}
