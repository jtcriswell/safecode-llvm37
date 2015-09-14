/* print_3_ahead() takes a linked list and prints the value 3 links ahead,
   if the value exists. Pass a list with a free'd element to that
   function. */

#include <stdlib.h>
#include <stdio.h>

typedef struct _link {
  struct _link *next;
  int value;
} link;

void print_3_ahead(link *l)
{
  int i;
  for (i = 0; i < 3; i++)
  {
    if (l == NULL)
      return;
    l = l->next;
  }
  printf("%i\n", l->value);
}

int main()
{
  link *l, *s, *p;
  int i;
  s = l = malloc(sizeof(link));

  for (i = 0; i < 5; i++)
  {
    l->next = malloc(sizeof(link));
    l->value = i;
    p = l->next;
    if (i == 3)
      free(l);
    l = p;
  }
  l->next = NULL;

  print_3_ahead(s);
  return 0;
}
