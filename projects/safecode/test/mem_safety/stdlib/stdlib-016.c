/* Strncpy off-by-one */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int main()
{
  char string1[] = "My string";
  char *string2;
  string2 = malloc(sizeof(string1) - 1);
  strncpy(string2, string1, sizeof(string1));
  printf("%s\n", string2);
  free(string2);
  return 0;
}
