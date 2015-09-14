/*===- UserPoolSystem.cpp - Implementation of callbacks needed by runtimes ===*/
/*                                                                            */
/*                            SAFECode Compiler                               */
/*                                                                            */
/* This file was developed by the LLVM research group and is distributed      */
/* under the University of Illinois Open Source License. See LICENSE.TXT for  */
/* details.                                                                   */
/*                                                                            */
/*===----------------------------------------------------------------------===*/
/*                                                                            */
/* This file implements the callbacks for userspace code that are required by */
/* the various SAFECode runtime libraries.                                    */
/*                                                                            */
/*===----------------------------------------------------------------------===*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>

/* Linux and *BSD tend to have these flags named differently. */
#if defined(MAP_ANON) && !defined(MAP_ANONYMOUS)
# define MAP_ANONYMOUS MAP_ANON
#endif /* defined(MAP_ANON) && !defined(MAP_ANONYMOUS) */

void
poolcheckfail (const char * msg, int i, void* p)
{
  fprintf (stderr, "poolcheckfail: %s: %x : %p\n", msg, i, p);
  fflush (stderr);
}

void
poolcheckfatal (const char * msg, int i)
{
  fprintf (stderr, "poolcheckfatal: %s: %x\n", msg, i);
  fflush (stderr);
  exit (1);
}

void
poolcheckinfo (const char * msg, int i)
{
  printf ("poolcheckinfo: %s %x\n", msg, i);
  fflush (stdout);
  return;
}

void
poolcheckinfo2 (const char * msg, int a, int b)
{
  printf ("poolcheckinfo: %s %x %x\n", msg, a, b);
  fflush (stdout);
  return;
}

static volatile unsigned pcmsize = 0;
void *
poolcheckmalloc (unsigned int power)
{
  void * Addr;
  Addr = mmap(0, 4096*(1U << power), PROT_READ|PROT_WRITE,
                                     MAP_SHARED|MAP_ANONYMOUS, -1, 0);
  if (Addr != (void *)-1) pcmsize += 4096*(1U << power);
  return (Addr == (void *)(-1)) ? 0 : Addr;
}

void *
sp_malloc (unsigned int size)
{
  return malloc (size);
}

void
printpoolinfo (void *Pool)
{
  return;
}

int
llva_load_lif (int i)
{
  return 0;
}

int
llva_save_lif ()
{
  return 0;
}

int
llva_save_tsc ()
{
  return 0;
}

