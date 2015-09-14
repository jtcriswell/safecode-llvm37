/* Read from a free'd file descriptor. */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

int main()
{
  int *pipes;
  char string[] = "String";
  char buffer[sizeof(string)];
  pipes = malloc(sizeof(int) * 2);
  pipe(pipes);
  write(pipes[1], string, sizeof(string));
  free(pipes);
  read(pipes[0], buffer, sizeof(buffer));
  printf("%s\n", buffer);
  return 0;
}
