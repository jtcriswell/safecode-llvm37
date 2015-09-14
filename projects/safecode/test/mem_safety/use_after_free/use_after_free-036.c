/* Use after free - an array of unions is free'd using a pointer stored in the array
   and then cast and read as an integer array. */

#include <stdlib.h>
#include <stdio.h>

union u {
  char *ptr;
  int  value;
};

int main()
{
  int *array;
  union u *uarray;
  array = malloc(sizeof(int) + sizeof(union u));
  uarray = (union u *) array;
  uarray[0].ptr = (char *) array;
  free(uarray[0].ptr);
  printf("%i\n", array[0]);
  return 0;
}
