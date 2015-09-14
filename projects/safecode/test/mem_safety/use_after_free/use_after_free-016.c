/* Allocate and free an array. Do that 10 times inside a loop.
   After the loop is done, try to write to the first allocated item. */

#include <stdlib.h>

#define SZ 1000

int main()
{
  int *a, *b, i;
  a = malloc(sizeof(int) * SZ);
  free(a);
  for (i = 0; i < 10; i++)
  {
    b = malloc(sizeof(int) * SZ);
    free(b);
  }
  *a = i;
  return 0;
}
