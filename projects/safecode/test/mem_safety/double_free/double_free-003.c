/* Double free a structure */
#include <stdlib.h>

typedef struct
{
  char string[100];
} X;

void *X_init()
{
  return malloc(sizeof(X));
}

void X_free(X* x)
{
  free(x);
}

int main()
{
  X *x;
  char *c;

  x = X_init();
  c = x->string;
  X_free(x);
  free(c);
  return 0;
}
