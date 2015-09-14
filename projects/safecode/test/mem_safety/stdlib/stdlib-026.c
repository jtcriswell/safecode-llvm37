/* strncat() until overflow occurs */

#include <string.h>

int main()
{
  int pos;
  char buffer[100];
  memset(buffer, 0, 100);
  while (pos < 10000)
    strncat(&buffer[pos++], "", 1);
  return 0;
}
