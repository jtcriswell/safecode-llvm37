/* Use a %n without corresponding argument in format
   string. */

#include <stdio.h>

#define BUFSZ 100

void print(char *string)
{
  char buf[BUFSZ];
  snprintf(buf, BUFSZ, string);
}

int main()
{
  print("1234");
  print("%n");
  return 0;
}
