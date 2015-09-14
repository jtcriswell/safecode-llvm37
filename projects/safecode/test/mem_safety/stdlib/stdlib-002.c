/* fgets invalid use */

#include <stdio.h>
#include <string.h>

#define BUFSZ 1024

int main()
{
  char buf[BUFSZ];
  fgets(buf, BUFSZ, NULL);
  return strlen(buf) % 255;
}
