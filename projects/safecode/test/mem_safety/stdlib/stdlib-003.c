/* Transfer a buffer that is too large using sscanf() */

#include <stdio.h>

#define BUFSZ 1000

int main()
{
  char buf1[BUFSZ], buf2[BUFSZ / 2];
  int i;
  for (i = 0; i < BUFSZ - 1; i++)
    buf1[i] = 'a';
  buf1[BUFSZ - 1] = '\0';
  sscanf(buf1, "%s", buf2);
  return 0;
}
