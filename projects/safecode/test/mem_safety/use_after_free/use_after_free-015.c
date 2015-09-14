/* Save a pointer to a string via sprintf()
   After the pointer is free'd restore it
   with sscanf() and use it. */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define BUFSZ 1000

char buffer[BUFSZ];

void f();

void f()
{
  char *ptr;
  sscanf(buffer, "%p", &ptr);
  strcpy(ptr, "Use after free");
}

int main()
{
  char *m;
  m = malloc(100);
  snprintf(buffer, BUFSZ, "%p", m);
  free(m);
  f();
  return 0;
}
