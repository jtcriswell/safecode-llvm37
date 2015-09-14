/* Off-by-one error leading to double free of last pointer in array. */

#include <stdlib.h>

#define ARSZ 100

int main()
{
  int **array, *freed, i;
  array = calloc(sizeof(char *), ARSZ);
  freed = calloc(sizeof(int), ARSZ);
  for (i = 0; i <= ARSZ - 1; i++)
  {
    array[i] = calloc(sizeof(int), 10);
    free(array[i]);
  }
  for (i = 0; i < ARSZ - 1; i++)
    freed[i] = 1;
  for (i = 0; i < ARSZ; i++)
    if (freed[i] == 0)
      free(array[i]);
  free(array);
  free(freed);
  return 0;
}
