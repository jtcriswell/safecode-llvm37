/* Off-by-one memmove() */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main()
{
  char string1[] = "A String";
  char *ptr;
  ptr = malloc(sizeof(string1));
  strcpy(ptr, string1);
  memmove(&ptr[1], ptr, sizeof(string1));
  free(ptr);
  return 0;
}
