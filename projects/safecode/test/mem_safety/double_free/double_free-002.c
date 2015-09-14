/* Allocate 1000 pointers. Double free one. */
#include <stdlib.h>

int main()
{
  void *ptr;
  int i;
  for (i = 0; i < 1000; i++)
  {
    ptr = malloc(sizeof(int) * 20);
    if (i == 98)
      free(ptr);
    else if (i % 57 == 0)
    {
      free(ptr);
      ptr = malloc(sizeof(int) * 10);
    }
    free(ptr);
  }
  return 0;
}
