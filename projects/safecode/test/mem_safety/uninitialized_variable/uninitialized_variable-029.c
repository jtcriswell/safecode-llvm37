/* Write to allocated structures with uninitialized components. */

#include <stdlib.h>
#include <string.h>

struct test
{
  char buffer[1000];
  struct test *next;
};

int main()
{
  struct test *t;
  t = malloc(sizeof(struct test));
  memset(t->next->buffer, 0, 1000);
  free(t);
  return 0;
}
