/* memmove() with the size of the destination smaller than given */

#include <string.h>

int main()
{
  char buf1[50];
  strcpy(buf1, "This is between 20 and 30");
  memmove(&buf1[30], buf1, 30);
  return 0;
}
