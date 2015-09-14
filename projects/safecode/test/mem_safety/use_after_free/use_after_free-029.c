/* Use after free resetting pointer value to itself. */
#include <stdlib.h>

int main()
{
  int *ptr = malloc(100);
  free(ptr);
  *ptr = *ptr;
  return 0;
}
