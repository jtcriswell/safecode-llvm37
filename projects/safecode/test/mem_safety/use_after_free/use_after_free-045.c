/* Call a free'd function pointer in a linked list of unions. */

#include <stdlib.h>
#include <stdio.h>

typedef int (*pfptr)(const char *, ...);

typedef union _link
{
  pfptr *ptr;
  union _link *next;
} link;

void call_next(link *l)
{
  (*l->next->ptr)("hello world\n");
}


int main()
{
  link *l;
  l = malloc(sizeof(link));
  l->next = malloc(sizeof(link));
  l->next->ptr = malloc(sizeof(pfptr));
  *l->next->ptr = printf;

  free(l->next->ptr);
  call_next(l);
  free(l->next);
  free(l);
  return 0;
}
