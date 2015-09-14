/* Double free each element in a circular linked list of structs. */

#include <stdlib.h>
#include <stdio.h>

typedef struct _test {
  struct _test *prev, *next;
} test;

int main()
{
  test *a, *b;
  a = calloc(sizeof(test), 1);
  b = calloc(sizeof(test), 1);
  a->next = a->prev = b;
  b->next = b->prev = a;
  free(a->next);
  free(a->next->next);
  free(b->next);
  free(b->next->next);
  return 0;
}
