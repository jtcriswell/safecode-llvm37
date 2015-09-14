/* Double free a pointer written onto the array that it points to. */

#include <stdlib.h>

int main()
{
  char *array;
  char **ptr;
  array = calloc(sizeof(char*), 1);
  ptr = (char**) array;
  ptr[0] = array;
  free(((char**)array)[0]);
  free(array);
  return 0;
}
