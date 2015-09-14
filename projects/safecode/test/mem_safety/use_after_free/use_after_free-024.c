/* Use of function pointer in free'd array */

#include <stdlib.h>
#include <stdio.h>

typedef void (*fptr)(void *);

void func(void *ptr)
{
  printf("%p\n", ptr);
  free(ptr);
}

#define ARRSZ 30

int main()
{
  fptr *array;
  int i;
  array = malloc(sizeof(fptr) * ARRSZ);
  for (i = 0; i < ARRSZ; i++)
    array[i] = func;
  array[0](array);
  array[1](NULL);
  return 0;
}
