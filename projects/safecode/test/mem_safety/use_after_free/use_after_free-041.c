/* Use a free'd struct that has been cast to a union */

#include <stdlib.h>
#include <stdio.h>

struct test_struct {
  void *ptr1, *ptr2;
  int value;
};

union test_union {
  void *ptr1, *ptr2[2];
};

int main()
{
  struct test_struct *t;
  union test_union *u;

  t = malloc(sizeof(struct test_struct));

  t->ptr1 = t;
  t->value = 500;
  u = (union test_union *) t;
  free(u->ptr1);
  printf("%i\n", t->value);
  return 0;
}
