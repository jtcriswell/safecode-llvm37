/* write() and read() out of bounds */

#include <unistd.h>
#include <string.h>
#include <assert.h>

#define BUFSZ 10

int main()
{
  int fds[2];
  char buffer1[BUFSZ], buffer2[BUFSZ];
  pipe(fds);
  memset(buffer2, 'a', BUFSZ);
  buffer2[BUFSZ - 1] = '\0';
  write(fds[1], buffer2, BUFSZ + 1);
  read(fds[0],  buffer1, BUFSZ + 1);
  assert(strcmp(buffer1, buffer2) == 0);
  close(fds[0]);
  close(fds[1]);
  return 0;
}
