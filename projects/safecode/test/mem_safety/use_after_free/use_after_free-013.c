/* Allocate an array of strings. Free every 6th string.
   Then write into all the strings which causes a use-after-free error. */

#include <stdlib.h>
#include <string.h>

#define SZ 1000

int main()
{
  char *buf[SZ];
  int i;

  for (i = 0; i < SZ; i++)
  {
    buf[i] = malloc(100);
    if (i % 6 == 0)
      free(buf[i]);
  }

  for (i = 0; i < SZ; i++)
    strcpy(buf[i], "some string");

  for (i = 0; i < SZ; i++)
  {
    if (i % 6 != 0)
      free(buf[i]);
  }

  return 0;
}
