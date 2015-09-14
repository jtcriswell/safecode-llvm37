/* readv() into a free'd buffer */

#include <stdlib.h>
#include <sys/uio.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define VECTORS 6
#define BUFSZ   3

int main()
{
  struct iovec vectors[VECTORS];
  int i, fd[2];
  char string[] = "This is length 18.", buf[BUFSZ + 1];
  for (i = 0; i < VECTORS; i++)
  {
    vectors[i].iov_base = malloc(BUFSZ);
    vectors[i].iov_len  = BUFSZ;
  }
  free(vectors[0].iov_base); // free before use
  pipe(fd);
  write(fd[1], string, sizeof(string) - 1);
  while (readv(fd[0], vectors, VECTORS) <= 0)
    ;
  for (i = 0; i < VECTORS; i++)
  {
    memcpy(buf, vectors[i].iov_base, BUFSZ);
    buf[BUFSZ] = '\0';
    printf("%s", buf);
    if (i != 0) // don't double free
      free(vectors[i].iov_base);
  }
  printf("\n");
  close(fd[0]);
  close(fd[1]);
  return 0;
}
