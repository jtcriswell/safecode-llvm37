/* Cast a union to a struct and free the union via using a pointer
   in the struct as well as using the pointer to the union. */

#include <stdlib.h>

typedef struct {
  void *ptr;
} test_struct;

typedef union {
  void *ptr;
} test_union;

int main()
{
  test_union *u;
  test_struct *s;

  u = malloc(sizeof(test_union) + sizeof(test_struct));
  u->ptr = (void *) u;
  s = (test_struct *) u;
  free(s->ptr);
  free(u);
  return 0;
}
