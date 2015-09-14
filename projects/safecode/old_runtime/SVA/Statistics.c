/*===- Stats.cpp - Statistics held by the runtime -------------------------===*/
/*                                                                            */
/*                       The LLVM Compiler Infrastructure                     */
/*                                                                            */
/* This file was developed by the LLVM research group and is distributed      */
/* under the University of Illinois Open Source License. See LICENSE.TXT for  */
/* details.                                                                   */
/*                                                                            */
/*===----------------------------------------------------------------------===*/
/*                                                                            */
/* This file implements functions that can be used to hold statistics         */
/* information                                                                */
/*                                                                            */
/*===----------------------------------------------------------------------===*/

#include "PoolSystem.h"

/* The number of stack to heap promotions executed dynamically */
static int stack_promotes = 0;

int stat_exactcheck  = 0;
int stat_exactcheck2 = 0;
int stat_exactcheck3 = 0;

extern int stat_poolcheck;
extern int stat_poolcheckarray;
extern int stat_poolcheckarray_i;
extern int stat_boundscheck;
extern int stat_boundscheck_i;
extern int stat_regio;
extern int stat_poolcheckio;
extern unsigned int externallocs;
extern unsigned int allallocs;

void
stackpromote()
{
  ++stack_promotes;
  return;
}

int
getstackpromotes()
{
  __sva_report ("getstackpromotes=%d\n", stack_promotes);
  __sva_report ("stat_exactcheck=%d\n", stat_exactcheck);
  __sva_report ("stat_exactcheck2=%d\n", stat_exactcheck2);
  __sva_report ("stat_exactcheck3=%d\n", stat_exactcheck3);
  __sva_report ("stat_poolcheck=%d\n", stat_poolcheck);
  __sva_report ("stat_poolcheckarray=%d\n", stat_poolcheckarray);
  __sva_report ("stat_poolcheckarray_i=%d\n", stat_poolcheckarray_i);
  __sva_report ("stat_boundscheck=%d\n", stat_boundscheck);
  __sva_report ("stat_boundscheck_i=%d\n", stat_boundscheck_i);
  __sva_report ("external allocs=%d\n", externallocs);
  __sva_report ("all      allocs=%d\n", allallocs);
  __sva_report ("io registrations=%d\n", stat_regio);
  __sva_report ("io poolchecks=%d\n", stat_poolcheckio);
  return stack_promotes;
}

