/* Double free of members in union. */
#include <stdlib.h>
#include <stdio.h>

union test {
  union {
    char *ptr1;
    char *ptr2;
  } u1;
  union {
    int *iptr1;
    int *iptr2;
  } u2;
};

int main()
{
  union test t;
  void *ptr;
  ptr = malloc(100);
  t.u1.ptr1 = ptr;
  t.u2.iptr2 = (int *) t.u1.ptr1;
  free(ptr);
  free(t.u2.iptr2);
  return 0;
}
