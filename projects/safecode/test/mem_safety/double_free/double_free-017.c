/* Free the internals of an allocated structure twice. */

#include <stdlib.h>

struct test {
  char *ptr1;
  char *ptr2;
};

void free_struct(struct test *t)
{
  free(t->ptr1);
  free(t->ptr2);
  free(t);
}

int main()
{
  struct test *t;
  t = malloc(sizeof(struct test));
  t->ptr1 = malloc(100);
  t->ptr2 = NULL;
  free(t->ptr1);
  free_struct(t);
  return 0;
}
