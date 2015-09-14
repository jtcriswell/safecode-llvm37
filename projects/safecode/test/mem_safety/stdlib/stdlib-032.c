/* Read into a buffer smaller than given size. */

#include <unistd.h>
#include <string.h>

void func(int id)
{
  char *str = "This is between 20 and 30";
  write(id, str, sizeof(str));
}

int main()
{
  char buf[10];
  int fd[2];
  pipe(fd);
  func(fd[1]);
  read(fd[0], buf, 15);
  close(fd[0]);
  close(fd[1]);
  return 0;
}
