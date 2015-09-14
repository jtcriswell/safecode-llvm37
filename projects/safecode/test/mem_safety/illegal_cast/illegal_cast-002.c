/* Attempt to use an integer as a pointer
   in a union. */
#include <string.h>

typedef union
{
  char *string;
  int amt;
} bad_union;

void process(bad_union *u)
{
  strcpy(u->string, "string");
}

int main()
{
  bad_union b;
  b.amt = 100;
  process(&b);
  return 0;
}
