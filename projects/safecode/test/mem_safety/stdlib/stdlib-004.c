/* strcpy into buffer of insufficient size. */

#include <string.h>

int main()
{
  char buf1[20], buf2[30];
  strcpy(buf2, "This is between 20 and 30");
  strcpy(buf1, buf2);
  return 0;
}
