/* Append an allocated pointer to a list of NULL pointers.
   Free every item in the list. Free the pointer again,
   causing a double free. */

#include <stdlib.h>
#include <string.h>

#define SZ 1000

struct T
{
  char *array[SZ];
};

int main()
{
  char *m;
  struct T a;
  int i;
  
  m = malloc(sizeof(char) * 1000);
  for (i = 0; i < SZ; i++)
    a.array[i] = NULL;
  memcpy(&a.array[SZ - 1], &m, sizeof(char *));
  for (i = 0; i < SZ; i++)
    free(a.array[i]);
  free(m);
  return 0;
}
