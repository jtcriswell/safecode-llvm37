/* Format string errors. */

#include <stdio.h>

#define SZ 100

int main()
{
  char string[SZ];
  snprintf(string, SZ, "%i %i\n", SZ);
  return 0;
}
