/* Write the address of a pointer into the array pointed by itself. Free
   the pointer through casting it and then use the pointer. */

#include <stdlib.h>
#include <string.h>

int main()
{
  char *ptr;
  ptr = malloc(10 * sizeof(char*) * sizeof(char));
  memcpy(ptr, &ptr, sizeof(char *));
  free(((char**)ptr)[0]);
  *ptr = '0';
  return 0;
}
