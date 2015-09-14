/* Write a free'd pointer into a pipe and use it to write data on the other end. */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

void read_ptr_from_fd(int fd)
{
  char *ptr;
  read(fd, &ptr, sizeof(char*));
  strcpy(ptr, "String");
}

int main()
{
  char *ptr1;
  int fd[2];
  pipe(fd);
  ptr1 = malloc(1000);
  write(fd[1], &ptr1, sizeof(char*));
  free(ptr1);
  read_ptr_from_fd(fd[0]);
  close(fd[0]);
  close(fd[1]);
  return 0;
}
