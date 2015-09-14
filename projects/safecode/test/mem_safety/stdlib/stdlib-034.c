/* Read from NULL pointer. */

#include <unistd.h>

int main()
{
  int fds[2];
  char *string = "String";
  pipe(fds);
  write(fds[1], string, sizeof(string));
  read(fds[0], NULL, sizeof(string));
  close(fds[0]);
  close(fds[1]);
  return 0;
}
