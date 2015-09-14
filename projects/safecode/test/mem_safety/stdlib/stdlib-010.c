/* memset() with the size of the destination smaller than given */

#include <string.h>

int main()
{
  char buf1[20];
  memset(buf1, 0, 30);
  return 0;
}
