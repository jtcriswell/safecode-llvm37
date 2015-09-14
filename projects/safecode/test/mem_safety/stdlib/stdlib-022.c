/* strcat() to buffer of length 1 */

#include <string.h>

int main()
{
  char buf[1];
  buf[0] = '\0';
  strcat(buf, "s");
  return 0;
}
