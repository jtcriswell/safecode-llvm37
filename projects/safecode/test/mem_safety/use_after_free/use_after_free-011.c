/* Allocate a 2d array. While freeing the 2d array the middle array is
 * always used.  Thus after more than 1/2 the array is freed the middle
 * array will also be written to after it has been freed. */

#include <stdlib.h>

#define SZ 100

int main()
{
  int **i, *r, x;
  i = malloc(sizeof(int*) * SZ);
  for (x = 0; x < SZ; x++)
    i[x] = malloc(sizeof(int) * SZ);
  r = i[SZ / 2];
  for (x = 0; x < SZ; x++)
  {
    r[x] = x;
    free(i[x]);
  }
  free(i);
  return 0;
}
