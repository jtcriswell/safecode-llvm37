/* sprintf() overflow */

#include <stdio.h>
#include <string.h>

int main()
{
  char buffer[1000];
  char dest[20];
  memset(buffer, 'a', 999);
  buffer[999] = '\0';
  sprintf(dest, "%s", buffer);
  return 0;
}
