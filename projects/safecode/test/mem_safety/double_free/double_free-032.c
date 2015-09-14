/* Double free a union with the free() function called from
   a function pointer within the union. */

#include <stdlib.h>

union test {
  void (*free_func)(void *);
  int value;
};

int main()
{
  union test *t;

  t = malloc(sizeof(union test));
  t->free_func = free;
  t->free_func(t);
  free(t);
  return 0;
}
