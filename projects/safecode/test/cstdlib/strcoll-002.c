// RUN: test.sh -p -t %t %s
#include <string.h>
#include <locale.h>
#include <assert.h>

// Example of the correct usage of strcoll().

int main()
{
  char s1[] = "s1";
  char s2[] = "s2";
  setlocale(LC_ALL, "C");
  int result;
  result = strcoll(&s1[0], &s2[0]);
  assert(result < 0);
  return 0;
}
