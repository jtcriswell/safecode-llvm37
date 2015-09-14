/* Uninitialized variable in an array. */

#include <string.h>

int main()
{
  char *array[20];
  strcpy(array[3], "String");
  return 0;
}
