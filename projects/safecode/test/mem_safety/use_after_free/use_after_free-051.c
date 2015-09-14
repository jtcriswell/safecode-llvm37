/* Use unlink() to delete a file with a free'd file name argument. */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

int main()
{
  char *file_name;
  int fd;

  file_name = malloc(100);
  strcpy(file_name, "/tmp/XXXXXX");
  fd = mkstemp(file_name);
  close(fd);
  printf("file name: %s\n", file_name);
  free(file_name);
  unlink(file_name);
  return 0;
}
