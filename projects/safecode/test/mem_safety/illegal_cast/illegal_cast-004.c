/* Call function pointer from union without proper assignment */
#include <stdlib.h>

union ab
{
  int a;
  int (*b)();
};

int func()
{
  return 9;
}

int main()
{
  union ab abc;
  abc.a = 100;
  abc.a = abc.b();
  return 0;
}
