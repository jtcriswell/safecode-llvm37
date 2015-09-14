// RUN: test.sh -p -t %t %s
#include <string.h>
#include <assert.h>

// Call memchr() on a string, and the character to find is not in the n bytes
// to look at.

int main()
{
  char ch[] = "This is a string.\n";
  char *pt;
  pt = memchr(&ch[0], '\0', 10);
  assert(pt == NULL);
  return 0;
}
