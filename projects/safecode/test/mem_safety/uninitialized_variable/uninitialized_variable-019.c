/* Uninitialized static pointer. */

#include <string.h>

int main()
{
  static char *ptr;
  strcpy(ptr, "string");
  return 0;
}
