/* Double free a union.
   The operation y->c = &y->b below overwrites the value of y->b when it
   is done, so that y->c now points to the address of y. Freeing y->c
   therefore also frees y. */
 
#include <stdlib.h>

union triple
{
  int *b;
  int **c;
};

int main()
{
  union triple *y;
  y = malloc(sizeof(union triple));
  y->b = malloc(sizeof(int));
  y->c = &y->b; /* Memory leak intentional */
  free(*y->c);
  free(y);
  return 0;
}
