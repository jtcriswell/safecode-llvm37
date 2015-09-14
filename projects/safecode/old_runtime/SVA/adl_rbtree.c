#include "adl_splay.h"

#ifdef USE_RB

#include "rb_kernel.h"
#include "PoolSystem.h"

typedef struct tree_node Tree;
struct tree_node {
  rb_node_t node;
  char* key;
  char* end;
  void* tag;
};


/* Memory management functions */

static Tree* freelist = 0;
static void* (*ext_alloc)(unsigned) = 0;
unsigned int externallocs = 0;
unsigned int allallocs = 0;

static Tree initmem[1024];
static int use = 0;

static void *
internal_malloc (unsigned int size)
{
  static unsigned char * page = 0;
  static unsigned char * loc = 0;
  unsigned char * retvalue;

  if (size > 4096)
    poolcheckfatal ("LLVA: internal_malloc: Size", size);

  /*
   * Allocate a new page if we've never had a page or there isn't any room
   * in the current page.
   */
  if (!page)
  {
    loc = page = ext_alloc (0);
    ++externallocs;
  }

  if ((loc+size) > (page + 4096))
  {
    loc = page = ext_alloc (0);
    ++externallocs;
  }

  /*
   * Allocate the space and return a pointer to it.
   */
  retvalue = loc;
  loc += size;
  return retvalue;
}

static inline Tree* tmalloc() {
  ++allallocs;
  if(freelist) {
    Tree* t = freelist;
    freelist = (Tree*)freelist->key;
    return t;
  } else if(use < 1024) {
    ++use;
    return &initmem[use-1];
  } else {
    Tree * tmp = internal_malloc(sizeof(Tree));
    if (!tmp)
      poolcheckfatal ("LLVA: tmalloc: Failed to allocate\n", 0);
    return (Tree*) tmp;
  }
}

static inline void tfree(Tree* t) {
  t->key = (char*)freelist;
  freelist = t;
}


#define key_lt(_key, _t) (((char*)_key < (char*)_t->key))

#define key_gt(_key, _t) (((char*)_key > (char*)_t->end))

static Tree *my_search(rb_root_t *root, char *k)
{
  rb_node_t *node = root->rb_node;
  
  while (node) {
    Tree* data = rb_entry(node, Tree, node);

    if (key_lt(k,data))
      node = node->rb_left;
    else if (key_gt(k,data))
      node = node->rb_right;
    else
      return data;
  }
  return 0;
}

static int my_insert(rb_root_t *root, char* key, unsigned len, void* tag)
{
  Tree* data = 0;
  rb_node_t **new = &(root->rb_node), *parent = 0;
  
  /* Figure out where to put new node */
  while (*new) {
    Tree* this = rb_entry(*new, Tree, node);
    
    parent = *new;
    if (key_lt(key, this))
      new = &((*new)->rb_left);
    else if (key_gt(key, this))
      new = &((*new)->rb_right);
    else
      return 0;
  }
  
  data = tmalloc();
  data->key = key;
  data->end = key + (len - 1);
  data->tag = tag;

  /* Add new node and rebalance tree. */
  rb_link_node(&data->node, parent, new);
  rb_insert_color(&data->node, root);
  
  return 1;
}

static void my_delete(rb_root_t* root, char* k) {
  Tree *data = my_search(root, k);
  
  if (data) {
    rb_erase(&data->node, root);
    tfree(data);
  }
}

static int count(rb_node_t* t) {
  if (t)
    return 1 + count(t->rb_left) + count(t->rb_right);
  return 0;
}

/* return any node with the matching tag */
static Tree* find_tag(rb_node_t* n, void* tag) {
  if (n) {
    Tree *t = rb_entry(n, Tree, node);
    if (t->tag == tag) return t;
    if ((t = find_tag(n->rb_left, tag))) return t;
    if ((t = find_tag(n->rb_right, tag))) return t;
  }
  return 0;
}

/* interface */

void adl_splay_insert(void** tree, void* key, unsigned len, void* tag)
{
  my_insert((rb_root_t*)tree, (char*)key, len, tag);
}

void adl_splay_delete(void** tree, void* key)
{
  my_delete((rb_root_t*)tree, (char*)key);
}

void adl_splay_delete_tag(void** tree, void* tag) {
  rb_root_t* t = (rb_root_t*)tree;
  Tree* n = find_tag(t->rb_node, tag);
  while (n) {
    my_delete(t, n->key);
    n = find_tag(t->rb_node, tag);
  }
}

int  adl_splay_find(void** tree, void* key) {
  Tree* t = my_search((rb_root_t*)tree, (char*)key);
  return (t && 
          !key_lt(key, t) && 
          !key_gt(key, t));
}

int adl_splay_retrieve(void** tree, void** key, unsigned* len, void** tag) {
  void* k = *key;
  Tree* t = my_search((rb_root_t*)tree, (char*)k);
  if (t && 
      !key_lt(k, t) && 
      !key_gt(k, t)) {
    *key = t->key;
    if (len) *len = (t->end - t->key) + 1;
    if (tag) *tag = t->tag;
    return 1;
  }
  return 0;
}

int  adl_splay_size(void** tree) {
  return (count(((rb_root_t*)tree)->rb_node));
}

void* adl_splay_any(void** tree) {
  if (((rb_root_t*)tree)->rb_node) {
    Tree *t = rb_entry(((rb_root_t*)tree)->rb_node, Tree, node);
    return t->key;
  }
  return 0;
}

void adl_splay_libinit(void* (nodealloc)(unsigned) ) {
  ext_alloc = nodealloc;
}

void adl_splay_libfini(void (nodefree)(void*) ) {
  while (freelist) {
    Tree* n = (Tree*)freelist->key;
    nodefree(freelist);
    freelist = n;
  }
}


#ifdef TEST_TREE
#include <stdio.h>
#include <stdlib.h>

int main() {
  adl_splay_libinit((void* (*)(int))malloc);
  void* t = 0;
  long x;
  for (x = 0; x < 100; ++x) {
    adl_splay_insert(&t, (void*)x, 10, 0);
  }

  printf("Size after 100 inserts of size 10 (overlap): %d\n", adl_splay_size(&t));

  for (x = 0; x < 100; ++x) {
    int f = adl_splay_find(&t, (void*)x);
    if (!f) printf("Failed find!\n");
  }
  for (x = 0; x < 100; x += 20) {
    int f = adl_splay_find(&t, (void*)x);
    if (!f) printf("Failed find!\n");
  }

  for (x = 0; x < 100; ++x) {
    adl_splay_delete(&t, (void*)x);
  }

  printf("Size should be 0: %d\n", adl_splay_size(&t));
  return 0;
}

void poolcheckfatal (const char * msg, int x) {
  printf("%s %d\n", msg, x);
}

#endif

#endif
