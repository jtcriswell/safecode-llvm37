/* Keep reading until buffer overflow */

#include <unistd.h>
#include <stdio.h>
#include <stdint.h>

#define BUFSZ 7

void transfer_array(int fd)
{
  int8_t buffer[BUFSZ];
  int8_t *bufptr;
  bufptr = &buffer[0];
  while (read(fd, bufptr, sizeof(int8_t)) > 0)
    printf("Read %i\n", *bufptr++);
}

int main()
{
  int8_t array[] = { 2, 3, 5, 7, 11, 13, 17 };
  int i;
  int fds[2];
  
  pipe(fds);
  for (i = 0; i < sizeof(array)/sizeof(int8_t); i++)
    write(fds[1], &array[i], sizeof(int8_t));
  close(fds[1]);

  transfer_array(fds[0]);

  close(fds[0]);
  return 0;
}
