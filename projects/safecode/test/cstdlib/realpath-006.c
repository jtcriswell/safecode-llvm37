// RUN: test.sh -e -t %t %s

#include <stdlib.h>
#include <unistd.h>

//
// Buffer overflow caused by realpath().
//

int main()
{
  char * buffer = malloc (4);
  char * p = realpath ("/etc/passwd", buffer);
  printf ("%s\n", p);
  return 0;
}
