/* strcpy() with negative pointer index */

#include <string.h>

int main()
{
  char buf[10];
  strcpy(&buf[-1], "String");
  return 0;
}
