/* Double free a pointer to a pointer to the free() function. */
#include <stdlib.h>

int main()
{
  void (**ptr1)(void *), (***ptr2)(void *);
  ptr1 = malloc(sizeof(void (*)(void *)));
  *ptr1 = free;
  ptr2 = &ptr1;
  (**ptr1)(ptr1);
  free(*ptr2);
  return 0;
}
