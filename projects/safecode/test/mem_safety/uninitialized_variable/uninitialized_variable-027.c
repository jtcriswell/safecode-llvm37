/* Uninitialized linked list */

#include <stdio.h>

typedef struct _link {
  int value;
  struct _link *next;
} link;

int main()
{
  link l;
  printf("%i\n", l.next->next->next->next->value);
  return 0;
}
