/* Use after free of an allocated integer
   in an array of pointers to integers. */
#include <stdlib.h>
#define ARRSZ 100

int main()
{
  int *arr[ARRSZ], i;
  arr[6] = malloc(sizeof(int));
  *arr[6] = 20;
  free(arr[*arr[6] - 14]);
  *arr[6] = 100;
  return 0;
}
