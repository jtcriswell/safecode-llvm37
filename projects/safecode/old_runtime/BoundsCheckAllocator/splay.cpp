// Note: This file is obtained from
// http://www.cs.utk.edu/~cs140/spring-2005/notes/Splay/
// FIXME: This may not be the most efficient version of splay implementation
// This may be updated with a different splay implementation in near future
#include <stdio.h>
#include "splay.h"
#include <stdlib.h>
void rotate(Splay *node)
{
  Splay *parent, *grandparent;

  if (node->parent->is_sentinel) return;
  
  parent = node->parent;
  grandparent = parent->parent;

  if (parent->left == node) {
    parent->left = node->right;
    if (parent->left != NULL) parent->left->parent = parent;
    node->right = parent;
  } else if (parent->right == node) {
    parent->right = node->left;
    if (parent->right != NULL) parent->right->parent = parent;
    node->left = parent;
  } else {
    fprintf(stderr, "rotate: error: parent's children are not right\n");
    exit(1);
  }

  parent->parent = node;
  node->parent = grandparent;

  if (grandparent->is_sentinel) {
    grandparent->parent = node;
  } else if (grandparent->left == parent) {
    grandparent->left = node;
  } else if (grandparent->right == parent) {
    grandparent->right = node;
  } else {
    fprintf(stderr, "rotate: error: grandparent's children are not right\n");
    exit(1);
  }
}

 void splay(Splay *node)
{
  Splay *parent, *grandparent;
  
  if (node->is_sentinel) return;

  while(1) {
    if (node->parent->is_sentinel) return;
  
    parent = node->parent;
    grandparent = parent->parent;

    /* If the node's parent is the root of the tree, do one rotation */

    if (grandparent->is_sentinel) {
      rotate(node);

    /* If we have a zig-zig, then rotate my parent, then rotate me */

    } else if ((parent->left  == node && grandparent->left  == parent) ||
               (parent->right == node && grandparent->right == parent)) {
      rotate(parent);
      rotate(node);
    
    /* If we have a zig-zag, then rotate me twice */

    } else {
      rotate(node);
      rotate(node);
    }
  }
}

Splay *new_splay()
{
  Splay *tree;

  tree = (Splay *) malloc(sizeof(struct splay));
  tree->key = 0;
  tree->val = 0;
  tree->is_sentinel = 1;
  tree->flink = tree;
  tree->blink = tree;
  tree->left = NULL;
  tree->right = NULL;
  tree->parent = NULL;
  return tree;
}

Splay *splay_root(Splay *tree)
{
  return tree->parent;
}

Splay *splay_first(Splay *tree)
{
  return tree->flink;
}

Splay *splay_last(Splay *tree)
{
  return tree->blink;
}

Splay *splay_next(Splay *node)
{
  return node->flink;
}

Splay *splay_prev(Splay *node)
{
  return node->blink;
}

Splay *splay_nil(Splay *tree)
{
  return tree;
}

void free_splay(Splay *tree)
{
  Splay *ptr;

  while (1) {
    ptr = splay_first(tree);
    if (!ptr->is_sentinel) {
      splay_delete_node(ptr);
    } else {
      free(ptr);
      return;
    }
  }
}

 Splay *splay_find_nearest_ptr(Splay *tree, unsigned long key, int *cmpval) 
{
  Splay *s, *last;
  int cmp;

  last = tree;
  s = splay_root(tree); 
  cmp = 1;

  while(s != NULL) {
    last = s;
    if (key == s->key) {
      *cmpval = 0;
      return s;
    } else if (key < (s->key)) {
      s = s->left;
      cmp = -1;
    } else {
      if (key < (s->val + s->key)) {
	*cmpval = 0;
	return s;
      }
      s = s->right;
      cmp = 1;
    }
  }

  *cmpval = cmp;
  return last;
}
 

Splay *splay_find_ptr(Splay *tree, unsigned long key)
{
  int cmpval;
  Splay *s;

  s = splay_find_nearest_ptr(tree, key, &cmpval);
  splay(s);
  if (cmpval == 0) return s; else return NULL;
}


Splay *splay_insert(Splay *tree, Jval key, Jval val, Splay *parent, int cmpval)
{
  Splay *s;

  s = (Splay *) malloc(sizeof(struct splay));
  s->is_sentinel = 0;
  s->parent = parent;
  s->left = NULL;
  s->right = NULL;
  s->key = key;
  s->val = val;

                       /* Set the parent's correct child pointer.  The only
                          subtle case here is when the key is already in 
                          the tree -- then we need to find a leaf node 
                          to use as a parent */

                       /* When we're done here, parent should point to the
                          new node's successor in the linked list */

  if (parent->is_sentinel) {
    parent->parent = s;
  } else {
    if (cmpval == 0) {   /* If the key is already in the
                                 tree, try to insert a new one as the
                                 node's right child.  If the node already
                                 has a right child, then try to insert the
                                 new one as a left child.  If there is already
                                 a left child, then go to parent-flink and 
                                 insert the node as its left child.  */
      if (parent->right == NULL) {
        cmpval = 1;
      } else if (parent->left == NULL) {
        cmpval = -1;
      } else {
        parent = parent->flink;
        s->parent = parent;
        cmpval = -1;
      }
    }
    if (cmpval > 0) {   /* Insert as right child */
      if (parent->right != NULL) {
        fprintf(stderr, "splay_insert error: parent->right != NULL");
        exit(1);
      }
      parent->right = s;
      parent = parent->flink;
    } else {
      if (parent->left != NULL) {
        fprintf(stderr, "splay_insert error: parent->left != NULL");
        exit(1);
      }
      parent->left = s;
    }
  }

  s->flink = parent;
  s->blink = parent->blink;
  s->flink->blink = s;
  s->blink->flink = s;
  splay(s);
  return s;
}

Splay *splay_insert_ptr(Splay *tree, unsigned long key, Jval val)
{
  Splay *parent, *s;
  int cmpval;

  parent = splay_find_nearest_ptr(tree, key, &cmpval);
  return splay_insert(tree, key, val, parent, cmpval);
}

extern void splay_delete_node(Splay *node)
{
  Splay *left, *right, *tree, *newroot;

  splay(node);

  tree = node->parent;

  left = node->left;
  right = node->right;
  newroot = node->flink;

  node->flink->blink = node->blink;
  node->blink->flink = node->flink;

  free(node);

  if (right == NULL && left == NULL) {
    tree->parent = NULL;
  } else if (right == NULL) {
    tree->parent = left;
    left->parent = tree;
  } else if (left == NULL) {
    tree->parent = right;
    right->parent = tree;
  } else {
    tree->parent = right;
    right->parent = tree;
    splay(newroot);
    newroot->left = left;
    left->parent = newroot;
  }
}
 Splay *finish_gte(int cmpval, Splay *s, int *found)
{

  if (cmpval == 0) {
    *found = 1;
    return s;
  } else if (cmpval < 0) {
    *found = 0;
    return s;
  } else {
    *found = 1;
    return s->flink;
  }
}
/*
Splay *splay_find_gte_str(Splay *tree, char *key, int *found)
{
  int cmpval;
  Splay *s;

  s = splay_find_nearest_str(tree, key, &cmpval);
  return finish_gte(cmpval, s, found);
}
*/

Splay *splay_find_gte_ptr(Splay *tree, unsigned long key, int *found)
{
  int cmpval;
  Splay *s;

  s = splay_find_nearest_ptr(tree, key, &cmpval);
  return finish_gte(cmpval, s, found);
}


