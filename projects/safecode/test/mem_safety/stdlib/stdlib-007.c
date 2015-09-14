/* memcpy() with the size of the destination smaller than given */

#include <string.h>

int main()
{
  char buf1[20], buf2[30];
  strcpy(buf2, "This is between 20 and 30");
  memcpy(buf1, buf2, 30);
  return 0;
}
