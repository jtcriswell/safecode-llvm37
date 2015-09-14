/* Free a pointer to a function in the
   function that the pointer references.
   Then free again. Results in double free. */
#include <stdlib.h>

void freeptr(void *ptr)
{
  free(ptr);
}

int main()
{
  void (**ptr)(void *);
  int i;

  ptr = malloc(sizeof(void (*)(void*)));
  *ptr = freeptr;
  (**ptr)(ptr);
  free(ptr);
  return 0;
}
