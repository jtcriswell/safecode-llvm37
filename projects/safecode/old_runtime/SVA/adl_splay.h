#ifdef __cpluscplus
extern "C" {
#endif
  extern void adl_splay_insert(void** tree, void* key, unsigned len, void* tag);
  extern void adl_splay_delete(void** tree, void* key);
  extern void adl_splay_delete_tag(void** tree, void* tag); /*expensive */

  extern int  adl_splay_find(void** tree, void* key);
  extern int  adl_splay_retrieve(void** tree, void** key, unsigned* len, void** tag);
  extern int  adl_splay_size(void** tree);
  extern void* adl_splay_any(void** tree);
  
  extern void adl_splay_libinit(void* (nodealloc)(unsigned) );
  extern void adl_splay_libfini(void (nodefree)(void*) );

  extern void adl_splay_foreach(void** tree, void (f)(void*, unsigned, void*));

#ifdef __cpluscplus
}
#endif

/* #define USE_RB */
