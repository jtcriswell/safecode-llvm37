/* Return the value of a pointer after freeing it. */

#include <stdlib.h>
#include <stdint.h>

int main()
{
  int *ptr = NULL;

  ptr = malloc(sizeof(int));
  *ptr = 0;
  if ((uint64_t)ptr & (uint64_t)ptr)
    free(ptr);

  return *ptr;
}
