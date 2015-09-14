/* strlen() on invalid pointer */

#include <stdio.h>
#include <string.h>

int main()
{
  char buffer[10];
  buffer[6] = '\0';
  printf("%i\n", strlen(&buffer[-1]));
  return 0;
}
