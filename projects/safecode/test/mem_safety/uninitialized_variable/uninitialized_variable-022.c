/* Uninitialized pointers in structure arrays. */

#include <string.h>

struct test
{
  char *ptrs[10];
};

int main()
{
  struct test tests[100];
  memcpy(tests[56].ptrs[3], "String", 3);
  return 0;
}
