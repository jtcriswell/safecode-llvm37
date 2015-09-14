/* Double free a struct with the free() function called within the
 * struct. */

#include <stdlib.h>

struct test {
  void (*free_func)(void *);
  struct test *ptr;
};

int main()
{
  struct test *t;

  t = malloc(sizeof(struct test));
  t->free_func = free;
  t->ptr = t;
  t->free_func(t->ptr);
  free(t);
  return 0;
}
