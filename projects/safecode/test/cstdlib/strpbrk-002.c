// RUN: test.sh -e -t %t %s

// strpbrk() on an unterminated set of characters to search for.

#include <string.h>

int main()
{
  char set[] = { 'a', 'b', 'c' };
  strpbrk("string", set);
  return 0;
}
