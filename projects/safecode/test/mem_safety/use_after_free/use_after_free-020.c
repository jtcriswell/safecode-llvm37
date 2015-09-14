/* Write an array from a pointer to an offset
   in the array after it has been free'd */

#include <stdlib.h>
#include <stdio.h>

#define ARRAY_SZ 30

void *ptr;

int compare(const void *a, const void *b)
{
  return *(int*)a - *(int*)b;
}

int main()
{
  int *array, i;
  array = malloc(sizeof(int) * ARRAY_SZ);
  for (i = 0; i < ARRAY_SZ; i++)
    array[i] = ARRAY_SZ - i;
  ptr = &array[10];
  free(array);
  qsort(ptr, ARRAY_SZ - 10, sizeof(int), compare);
  for (i = 0; i < ARRAY_SZ; i++)
    printf("%i ", array[i]);
  printf("\n");
  return 0;
}
