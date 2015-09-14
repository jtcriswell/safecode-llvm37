/* read out of bounds */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

char *string = "String";

int main()
{
  char *str;
  int fds[2];
  pipe(fds);
  str = malloc(sizeof(string));
  write(fds[1], string, sizeof(string));
  read(fds[0], &str[-1], sizeof(string));
  printf("%s\n", str);
  close(fds[0]);
  close(fds[1]);
  return 0;
}
