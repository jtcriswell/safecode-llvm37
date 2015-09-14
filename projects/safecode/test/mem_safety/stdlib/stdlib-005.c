/* strncpy with bounds greater than the destination strings' size */

#include <string.h>

int main()
{
  char buf1[20], buf2[30];
  strcpy(buf2, "This is between 20 and 30");
  strncpy(buf1, buf2, 30);
  return 0;
}
