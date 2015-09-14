/* Do some structure assignments then free the structure.
   Try to write to the freed structure. */

#include <stdlib.h>

typedef struct
{
  int i;
  int *p;
} S;

int main()
{
  S *s;
  s = malloc(sizeof(S));
  s->i = 65;
  s->p = &s->i;
  free(s);
  *s->p = 66;
  return 0;
}
