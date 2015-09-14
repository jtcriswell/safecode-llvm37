/* Iterate 1000000 times freeing and allocating a pointer
   each time. Double free another pointer at the end. */

#include <stdlib.h>
#define ASZ 1000

int main()
{
  char *a, *b;
  int i, j;
  a = malloc(sizeof(char));
  free(a);
  for (i = 0; i < ASZ; i++)
  {
    b = a;
    for (j = 0; j < ASZ; j++)
    {
      b = malloc(sizeof(char));
      free(b);
    }
  }
  free(a);
  return 0;
}
