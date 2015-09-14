/* strcat() with the size of the destination string smaller
   than required. */

#include <string.h>

int main()
{
  char buf1[20], buf2[30];
  strcpy(buf2, "This is between 20 and 30");
  buf1[0] = '\0';
  strcat(buf1, buf2);
  return 0;
}
