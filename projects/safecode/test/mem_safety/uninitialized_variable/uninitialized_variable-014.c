/* Pointer to uninitialized pointer. */

#include <stdlib.h>
#include <string.h>

int main()
{
  char **ptr;
  ptr = malloc(sizeof(char*));
  strcpy(*ptr, "string");
  free(ptr);
  return 0;
}
