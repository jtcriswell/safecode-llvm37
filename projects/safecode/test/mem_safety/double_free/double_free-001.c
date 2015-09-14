/* Construct array of 1000 allocated pointers.
   Free them sequentially. Then free the 10th one again. */

#include <stdlib.h>

#define BUFSZ 1000

int main()
{
  char *ptrs[BUFSZ];
  int i;

  for (i = 0; i < BUFSZ; i++)
    ptrs[i] = malloc(sizeof(char) * 2);
  for (i = 0; i < BUFSZ; i++)
    free(ptrs[i]);
  free(ptrs[10]);
  return 0;
}
