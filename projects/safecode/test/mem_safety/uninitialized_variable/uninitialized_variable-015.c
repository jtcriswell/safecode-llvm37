/* Uninitialized pointer in multidimensional arrays. */

#include <string.h>

int main()
{
  char *array[100][100];
  strcpy(array[2][3], "string");
  return 0;
}
