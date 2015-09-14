/* Double free using qsort().
   The comparison functions frees every value it is passed. */

#include <stdlib.h>

int comp(const void *a, const void *b)
{
  free(*((int**)a));
  return **((int**)a) - **((int**)b);
}

#define SZ 100

int main()
{
  int *arr[SZ], i;

  for (i = 0; i < SZ; i++)
  {
    arr[i] = malloc(sizeof(int));
    *arr[i] = SZ - i;
  }
  qsort(arr, SZ, sizeof(int*), &comp);

  for (i = 0; i < SZ; i++)
    free(arr[i]);

  return 0;
}
