/* Print the middle element of an array after it has already been freed.
 */
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#define ARSZ 1000

typedef struct {
  int16_t *array;
  int sz;
} test;

void print_middle_element(test *t)
{
  printf("%i\n", t->array[t->sz / 2]);
}

int main()
{
  int16_t *array;
  test t;
  int i;

  array = calloc(ARSZ, sizeof(int16_t));
  for (i = 0; i < ARSZ; i++)
    array[i] = i;
  t.array = array;
  t.sz = ARSZ;
  free(array);
  print_middle_element(&t);
  return 0;
}
