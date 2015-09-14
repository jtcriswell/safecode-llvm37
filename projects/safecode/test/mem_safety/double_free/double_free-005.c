/* Free an allocated array of function pointers */

#include <stdlib.h>

typedef void (*fptr)(void *);

fptr *items;

void _free_things(void *ptr)
{
  free(ptr);
}

#define ARRAY_SIZE 100

int main()
{
  items = malloc(ARRAY_SIZE * sizeof(fptr));
  int i;
  for (i = 0; i < ARRAY_SIZE; i++)
    items[i] = _free_things;
  for (i = 0; i < ARRAY_SIZE; i++)
    items[i](items);
  return 0;
}
