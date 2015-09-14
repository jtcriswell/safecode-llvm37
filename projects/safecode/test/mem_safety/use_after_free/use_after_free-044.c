/* Call a function from a linked list of pointers that has a free'd
   element. */

#include <stdio.h>
#include <stdlib.h>

typedef int (*pfptr)(const char *, ...);

typedef struct _link {
  struct _link *next;
  pfptr  function;
} link;

int main()
{
  link *start, *next, *current;
  int i;

  current = start = malloc(sizeof(link));
  for (i = 0; i < 10; i++)
  {
    current->next = malloc(sizeof(link));
    current->function = printf;
    next = current->next;
    if (i == 4)
      free(current);
    current = next;
  }
  current->next = NULL;
  start->next->next->next->next->next->next->function("hello world\n");
  return 0;
}
