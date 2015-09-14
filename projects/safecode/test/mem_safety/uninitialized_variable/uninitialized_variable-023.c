/* Uninitialized pointer in union */

#include <string.h>

union test
{
  int u;
  char *array[10][10];
};

int main()
{
  union test t;
  strcpy(t.array[4][4], "String");
  return 0;
}
