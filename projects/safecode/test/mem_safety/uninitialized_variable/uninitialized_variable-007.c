/* Uninitialized pointer saved in string. */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

char buffer[1000];

void f()
{
  char *ptr;
  sscanf(buffer, "%p", &ptr);
  strcpy(ptr, "String");
}

int main()
{
  char **ptr;
  ptr = malloc(sizeof(char *));
  snprintf(buffer, 1000, "%p", *ptr);
  f();
  return 0;
}
