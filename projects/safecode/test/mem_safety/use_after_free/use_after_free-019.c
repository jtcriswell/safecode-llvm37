/* Dereference a node in a circular linked list of unions. */
#include <stdlib.h>
#include <stdio.h>

typedef union _link {
  int end;
  union _link *next;
} link;

int main()
{
  link *ptr;
  ptr = malloc(sizeof(link));
  ptr->next = malloc(sizeof(link));
  ptr->next->next = malloc(sizeof(link));
  ptr->next->next->next = malloc(sizeof(link));
  ptr->next->next->next->next = ptr;
  free(ptr->next->next->next->next);
  ptr->end = 100;
  printf("%i\n", ptr->end);
  return 0;
}
