/* Write the address of a free'd pointer into a buffer
   using snprintf() then free it again. */
#include <stdlib.h>
#include <stdio.h>

#define BUFSZ 1000

char buffer[BUFSZ];

void f()
{
  char *ptr;
  sscanf(buffer, "%p", &ptr);
  free(ptr);
  return;
}

int main()
{
  char *string;
  string = malloc(10000);
  free(string);
  snprintf(buffer, BUFSZ, "%p", string);
  f();
  return 0;
}
