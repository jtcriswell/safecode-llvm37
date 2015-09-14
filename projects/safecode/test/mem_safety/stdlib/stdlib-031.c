/* Read into a buffer that is too small to hold data. */

#include <unistd.h>
#include <setjmp.h>
#include <stdio.h>

void do_io(int fds[2])
{
  jmp_buf b;
  char string[] = "This is more than 10.";
  char buffer[10];

  if (setjmp(b) != 0)
  {
    read(fds[0], buffer, sizeof(string));
    printf("%s\n", buffer);
    return;
  }
  else
  {
    write(fds[1], string, sizeof(string));
    longjmp(b, 1);
  }
}

int main()
{
  int fds[2];

  pipe(fds);
  do_io(fds);
  close(fds[0]);
  close(fds[1]);
  return 0;
}
