/* Unitialized pointers in nested unions. */
#include <stdlib.h>
#include <stdio.h>

typedef struct
{
  union {
    union {
      char *str;
      int i;
    } union_a;
    union {
      int *arr;
      int z;
    } union_b;
  } big_union;
} big_struct;

int main()
{
  big_struct B;
  printf("%c\n", B.big_union.union_a.str[1]);
  return 0;
}
