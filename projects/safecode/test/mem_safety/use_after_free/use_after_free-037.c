/* Similary to test 36, except with a struct instead of a union. */

#include <stdlib.h>
#include <stdio.h>

struct test {
  char *ptr;
  int value;
};

int main()
{
  int *array;
  struct test *t;
  array = malloc(sizeof(int) + sizeof(struct test));
  t = (struct test *) array;
  t[0].ptr = (char *) array;
  free(t[0].ptr);
  printf("%i\n", array[0]);
  return 0;
}
