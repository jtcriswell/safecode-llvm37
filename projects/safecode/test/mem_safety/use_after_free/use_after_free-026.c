/* Dereference a node in a linked list that has been free'd
   but points to and is pointed to by validly allocated nodes. */

#include <stdlib.h>
#include <stdio.h>

typedef struct _nested
{
  int level;
  struct _nested *next;
} nested;

#define LEVEL 20

int main()
{
  nested *n_array[LEVEL];
  int i;
  for (i = 0; i < LEVEL; i++)
  {
    n_array[i] = malloc(sizeof(nested));
    n_array[i]->level = i;
    if (i > 0)
      n_array[i-1]->next = n_array[i];
  }
  free(n_array[3]);
  printf("%i\n", n_array[0]->next->next->next->next->level);
  for (i = 0; i < LEVEL; i++)
  {
    if (i != 3)
      free(n_array[i]);
  }
  return 0;
}
