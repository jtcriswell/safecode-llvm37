/* Uninitialized malloced struct. */

#include <stdlib.h>
#include <stdio.h>

typedef struct
{
  int *ptr;
  char str[10];
} alloced_struct;

int main()
{
  alloced_struct *a;
  a = malloc(sizeof(alloced_struct));
  printf("%i\n", *a->ptr);
  return 0;
}
