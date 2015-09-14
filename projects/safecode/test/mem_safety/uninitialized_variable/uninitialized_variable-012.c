/* Uninitialized union variable in an array. */

#include <string.h>

typedef union
{
  char *ptr;
  int amt;
} array_union;

int main()
{
  array_union au[100];
  strcpy(au[4].ptr, "String");
  return 0;
}
