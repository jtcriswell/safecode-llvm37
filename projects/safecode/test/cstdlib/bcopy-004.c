// RUN: test.sh -p -t %t %s

// Example of the correct use of bcopy().

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

int main()
{
  char buf[] = "hello world";
  char *str1 = malloc(100);
  bcopy(buf, str1, sizeof(buf));
  assert(strcmp(str1, "hello world") == 0);
  free(str1);
  return 0;
}
