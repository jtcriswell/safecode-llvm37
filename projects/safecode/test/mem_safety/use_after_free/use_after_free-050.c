/* Use of a freed pointer copied into a union from a struct. */

#include <stdlib.h>
#include <string.h>

union test_union
{
  int *ptr;
};

struct test_struct
{
  int *ptr;
};

int main()
{
  union test_union u;
  struct test_struct s;

  s.ptr = malloc(1000);
  free(s.ptr);
  memcpy(&u, &s, sizeof(u));

  u.ptr[0] = 10;
  return 0;
}
