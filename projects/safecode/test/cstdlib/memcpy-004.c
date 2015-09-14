// RUN: test.sh -p -t %t %s
#include <string.h>
#include <assert.h>

// Example of a memcpy() with zero length and bad input pointers

int main()
{
  char src[] = "aaaaaaaaaab";
  char dst[100];
  volatile char * p;
  p = memcpy(0, 0, 0);
  printf ("%p\n", p);
  return 0;
}
