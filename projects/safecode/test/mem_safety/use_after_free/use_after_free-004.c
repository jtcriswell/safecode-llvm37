/* Compare a string that has been freed and
   one that is valid. */

#include <stdlib.h>
#include <string.h>

#define BUFSZ (100000 * sizeof(char))

int main()
{
  char *buf1, *buf2;
  buf1 = malloc(BUFSZ);
  strcpy(buf1, "A string");
  free(buf1);
  buf2 = malloc(BUFSZ);
  strcpy(buf2, "Another string");
  if (strcmp(buf1, buf2) == 0)
    return 1;
  free(buf2);
  return 0;
}
