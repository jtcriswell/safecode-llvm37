/* strncat with allocated memory */

#include <stdlib.h>
#include <string.h>

int main()
{
  char *ptr;
  ptr = malloc(10);
  ptr[0] = '\0';
  strncat(ptr, "1234567890", 11);
  free(ptr);
}
