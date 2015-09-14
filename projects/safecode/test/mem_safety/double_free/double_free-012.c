/* Free two different elements in a union which hold the same pointer data. */

#include <stdlib.h>

int main()
{
  union {
    int *A, *B;
  } u;
  u.A = malloc(sizeof(int));
  free(u.A);
  free(u.B);
  return 0;
}
