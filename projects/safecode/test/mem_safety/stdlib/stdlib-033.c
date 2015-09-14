/* Read into uninitialized variable */

#include <unistd.h>

int main()
{
  char *ptr;
  int fd[2];
  pipe(fd);
  write(fd[1], "String", 7);
  read(fd[0], ptr, 7);
  return 0;
}
