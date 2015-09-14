/* Uninitialized allocated function pointer. */

#include <stdlib.h>

typedef int (*fptr)();

int main()
{
  fptr *items;
  items = malloc(100 * sizeof(fptr));
  return items[0]();
}
