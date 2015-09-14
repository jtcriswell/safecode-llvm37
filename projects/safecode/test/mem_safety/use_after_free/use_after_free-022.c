/* Access a validly allocated pointer through a freed one. */

#include <stdlib.h>
#include <string.h>

typedef struct
{
  char *string;
  int u;
} example;

void access(example *e)
{
  strcpy(e->string, "String");
}

int main()
{
  example *e;
  e = malloc(sizeof(example));
  e->string = malloc(sizeof(char) * 1000);
  free(e);
  access(e);
  return 0;
}
