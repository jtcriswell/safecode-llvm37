/* memcpy() source too short */

#include <stdlib.h>
#include <string.h>

int main()
{
  char *ptr;
  char buffer[500];
  ptr = malloc(20);
  memcpy(buffer, ptr, 30);
  return 0;
}
