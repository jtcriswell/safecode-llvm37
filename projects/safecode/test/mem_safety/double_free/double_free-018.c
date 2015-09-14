/* Pass a pointer through a pipe. Double free the pointer. */
#include <stdlib.h>
#include <unistd.h>

int main()
{
  char *buf, *buf2;
  int fd[2];

  buf = malloc(1000);
  pipe(fd);
  write(fd[1], &buf, sizeof(char *));
  read(fd[0], &buf2, sizeof(char *));
  free(buf2);
  free(buf);
  close(fd[0]);
  close(fd[1]);
  return 0;
}
