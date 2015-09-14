/* Construct an array of pointers to integers.
   Every 7th item in the array has the same value
   as the first item in the array. Iterate over the
   array and free all pointers, thus causing double
   free multiple times. */

#include <stdlib.h>
#define ARRSZ 100

int main()
{
  int *arr[ARRSZ], i;
  for (i = 0; i < ARRSZ; i++)
  {
    if (i > 0 && i % 7 == 0)
      arr[i] = arr[0];
    else
    {
      arr[i] = malloc(sizeof(int));
      *arr[i] = i;
    }
  }
  for (i = 0; i < ARRSZ; i++)
    free(arr[i]);
  return 0;
}
