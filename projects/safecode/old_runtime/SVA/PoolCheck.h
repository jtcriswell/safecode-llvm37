/*===- PoolCheck.h - Pool check runtime interface file --------------------===*/
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

#ifndef POOLCHECK_RUNTIME_H
#define POOLCHECK_RUNTIME_H

#include "safecode/Config/config.h"

extern unsigned long __sva_save_iflag(void);
extern void __sva_restore_iflag(unsigned long enable);


static inline unsigned long
disable_irqs ()
{
  unsigned long is_set;
  is_set = __sva_save_iflag ();
  __sva_restore_iflag (0);
  return is_set;
}

static inline void
enable_irqs (unsigned long is_set)
{
  __sva_restore_iflag (is_set);
}

typedef unsigned long __sva_rt_lock_t;

static inline void __sva_rt_lock (__sva_rt_lock_t * lock) {
  *lock = disable_irqs();
}
 
static inline void __sva_rt_unlock (__sva_rt_lock_t * lock) {
  enable_irqs(*lock);
}

#define PCLOCK() unsigned pc_i = disable_irqs();
#define PCLOCK2() pc_i = disable_irqs();
#define PCUNLOCK() enable_irqs(pc_i);


typedef struct MetaPoolTy {
  /* A splay of Pools, useful for registration tracking */
  void* Slabs;

  /* A splay for registering global objects and heap objects */
  void* Objs;

  /* A splay for registering function pointers */
  void * Functions;

  /* A splay of rewritten Obj Pointers */
  void* OOB;

  /* Next invalid Ptr for rewriting */
  void* profile;

  /*cache space */
  void* cache0;
  void* cache1;
  void* cache2;
  void* cache3;

#if 1
  unsigned int cindex;
  unsigned char * start[4];
  unsigned int length[4];
  void * cache[4];
#endif
 

#ifdef SVA_IO
  /* A splay for I/O objects */
  void * IOObjs;
#endif

#ifdef SVA_MMU 
  unsigned TK;
#endif

} MetaPoolTy;

typedef struct funccache {
  unsigned int index;
  void * cache[4];
} funccache;

#ifdef __cpluscplus
extern "C" {
#endif
  /* initialize library */
  void pchk_init(void);

  /* Registration functions                                                   */
  /* These are written such that a weaker version (without MP) can be         */
  /* inserted in the kernel by the programer and we can trivial rewrite them  */
  /* to include the metapool */
  void pchk_reg_slab(MetaPoolTy* MP, void* PoolID, void* addr, unsigned len);
  void pchk_drop_slab(MetaPoolTy* MP, void* PoolID, void* addr);
  void pchk_reg_obj(MetaPoolTy* MP, unsigned char * addr, unsigned len);
  void pchk_drop_obj(MetaPoolTy* MP, unsigned char * addr);
  void pchk_reg_pool(MetaPoolTy* MP, void* PoolID, void* MPLoc);
  void pchk_drop_pool(MetaPoolTy* MP, void* PoolID);
  void pchk_reg_pages (MetaPoolTy* MP, void* addr, unsigned order);
  void pchk_drop_pages (MetaPoolTy* MP, void* addr);

  /* Register and Deregister Integer State buffers */
  void pchk_reg_int (void* addr);
  void pchk_drop_int (void * addr);

  /* check that addr exists in pool MP */
  void * poolcheck   (MetaPoolTy* MP, void* addr) __attribute__ ((pure));
  void * poolcheck_i (MetaPoolTy* MP, void* addr) __attribute__ ((pure));

  /* check that src and dest are same obj or slab */
  void poolcheckarray(MetaPoolTy* MP, void* src, void* dest);

  /* check that src and dest are same obj or slab */
  /* if src and dest do not exist in the pool, pass */
  void poolcheckarray_i(MetaPoolTy* MP, void* src, void* dest);

  /* I/O Poolchecks */
  void * poolcheckio   (MetaPoolTy* MP, void* addr);
  void * poolcheckio_i (MetaPoolTy* MP, void* addr);

  /* if src is an out of object pointer, get the original value */
  void* pchk_getActualValue(MetaPoolTy* MP, void* src);

  /* check bounds and return result ptr, which may have been rewritten */
  void* pchk_bounds(MetaPoolTy* MP, void* src, void* dest);
  void* pchk_bounds_i(MetaPoolTy* MP, void* src, void* dest);

  void * exactcheck(int a, int b, void * result) __attribute__ ((weak));
  void * exactcheck2(signed char *base, signed char *result, unsigned size) __attribute__ ((weak));
  void * exactcheck2a(signed char *base, signed char *result, unsigned size) __attribute__ ((weak));
  void * exactcheck3(signed char *base, signed char *result, signed char * end)__attribute__ ((weak));

  void funccheck (unsigned num, void *f, void *t1, void *t2, void *t3, void *t4, void * t5, void * t6) __attribute__ ((weak));
  void funccheck_t (unsigned num, void * f, void ** table) __attribute__ ((weak));
  void funccheck_g (MetaPoolTy * MP, void * f) __attribute__ ((weak));
  void pchk_reg_ic (int s, int a, int b, int c, int d, int e, int f, void* ad);
  void pchk_drop_ic (void* addr);
  void pchk_iccheck (void * addr);
  void * getBegin (void * node) __attribute__ ((weak));
  void * getEnd (void * node) __attribute__ ((weak));

  unsigned int pchk_check_int (void * addr);

  void   pchk_declarestack (void * MP, unsigned char * addr, unsigned size);
  void   pchk_releasestack (void * addr);
  void * pchk_checkstack   (void * addr, unsigned int * size);
  void pchk_profile(MetaPoolTy* MP, void* pc, long time);

#ifdef __cpluscplus
}
#endif

#endif
