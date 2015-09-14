/* Attempt linked list like access */

#include <stdio.h>

typedef union _link
{
  int value;
  union _link *next;
} link;

int main()
{
  link l;
  l.value = 100;
  printf("%i\n", l.next->value);
  return 0;
}
