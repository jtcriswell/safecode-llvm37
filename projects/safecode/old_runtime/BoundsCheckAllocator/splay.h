// Note: This file is obtained from
// http://www.cs.utk.edu/~cs140/spring-2005/notes/Splay/
// FIXME: This may not be the most efficient version of splay implementation
// This may be updated with a different splay implementation in near future
#ifndef _SPLAY_
#define SPLAY

typedef unsigned long Jval ;

/* Node identities */

#define SPLAY_SENTINEL 0
#define SPLAY_OTHER 1

typedef struct splay {
  Jval key;
  Jval val;
  int is_sentinel;
  struct splay *left;
  struct splay *right;
  struct splay *flink;
  struct splay *blink;
  struct splay *parent;
} Splay;

extern "C" {
 Splay *new_splay();
 void free_splay(Splay *);
 Splay *splay_insert_ptr(Splay *tree, unsigned long key, Jval val);
 Splay *splay_find_ptr(Splay *tree, unsigned long key);
 Splay *splay_find_gte_ptr(Splay *tree, unsigned long key, int *found);

 Splay *splay_root(Splay *tree);
 Splay *splay_first(Splay *tree);
 Splay *splay_last(Splay *tree);
 Splay *splay_next(Splay *node);
 Splay *splay_prev(Splay *node);
 Splay *splay_nil(Splay *tree);

 void splay_delete_node(Splay *node);
}

#define splay_traverse(ptr, list) \
  for (ptr = splay_first(list); ptr != splay_nil(list); ptr = splay_next(ptr))
#define splay_rtraverse(ptr, list) \
  for (ptr = splay_last(list); ptr != splay_nil(list); ptr = splay_prev(ptr))

#endif
