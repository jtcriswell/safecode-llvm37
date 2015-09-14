/* Cast free between function pointer types and integer types */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

uint64_t ptr;
char (*func)(int, int);

int main()
{
  char *p;
  p = malloc(100);
  ptr = (uint64_t) free;
  func = (char(*)(int, int)) ptr;
  ((void(*)(void *)) func)(p);
  free(p);
  return 0;
}
