/* strncat() overflow */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int main()
{
  char buf1[1];
  char string[] = "A string";
  buf1[0] = '\0';
  strncat(buf1, string, sizeof(string));
  printf("%s\n", buf1);
  return 0;
}
