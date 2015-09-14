/* Uninitialized pointers in malloced multidimensional arrays. */

#include <stdlib.h>
#include <string.h>

int main()
{
  char ***array;
  array = malloc(sizeof(char**) * 10);
  array[0] = malloc(sizeof(char *) * 10);
  strcpy(array[0][1], "string");
  free(array[0]);
  free(array);
  return 0;
}
