/* writev() source from a free'd buffer */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/uio.h>
#include <stdio.h>

#define BUFFERS 30
#define READBUF 100

int main()
{
  struct iovec buffers[BUFFERS];
  int  fd[2], i, amt, total_read, total_wrote;
  char string[] = "String", dest[READBUF];
  pipe(fd);
  for (i = 0; i < BUFFERS; i++)
  {
    buffers[i].iov_base = malloc( sizeof(string) - 1 );
    memcpy(buffers[i].iov_base, string, sizeof(string) - 1);
    buffers[i].iov_len = sizeof(string) - 1;
  }
  free(buffers[10].iov_base); // free before use
  total_wrote = writev(fd[1], buffers, BUFFERS);
  close(fd[1]);
  total_read = 0;
  do
  {
    amt = read(fd[0], dest, READBUF - 1);
    if (amt != -1)
    {
      dest[amt]   = '\0';
      total_read += amt;
      printf("%s", dest);
    }
  } while (total_read != total_wrote);
  printf("\n");
  for (i = 0; i < BUFFERS; i++)
    if (i != 10) // don't double free
      free(buffers[i].iov_base);
  close(fd[0]);
  return 0;
}
