/* Write to a file and then attempt to read
   using a buffer that is not long enough. */

#include <stdio.h>
#include <string.h>

#define BUFSZ 1024

int main()
{
  char buf[2*BUFSZ];
  char read_buf[BUFSZ];
  FILE *f;
  f = tmpfile();
  memset(buf, 'A', 2*BUFSZ);
  fwrite(buf, sizeof(char), 2*BUFSZ, f);
  fseek(f, 0L, SEEK_SET);
  fread(read_buf, sizeof(char), BUFSZ*2, f);
  fclose(f);
  return 0;
}
