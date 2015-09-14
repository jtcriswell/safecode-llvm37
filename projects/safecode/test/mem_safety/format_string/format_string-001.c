/* Format string problems. */

#include <stdio.h>

char *message[] =
{ "Message 1.\n",
  "Message 2.\n",
  "%i %i %n\n" };

int main()
{
  int i;
  for (i = 0; i < 3; i++)
    printf(message[i]);
  return 0;
}
