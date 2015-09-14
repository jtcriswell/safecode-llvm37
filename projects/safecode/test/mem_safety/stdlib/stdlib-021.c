/* strncpy() into zero length buffer */

#include <string.h>

int main()
{
  char buf[0];
  strncpy(buf, "a", 1);
  return 0;
}
