/* memmove() with invalid source */

#include <string.h>

int main()
{
  char buffer[200];
  memmove(buffer, &buffer[200], 100);
  return 0;
}
