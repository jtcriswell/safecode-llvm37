// RUN: test.sh -s "call void @fastlscheck_debug" -p -t %t %s
//
// TEST: byval-002
//
// Description:
//  Test BBC works well on a function that has more than one argument
//  with byval attribute.
//

// #include "stdio.h"

struct bvt {
  int i;
  int j;
  char str[10];
};

void byval_test(struct bvt x, struct bvt y)
{

  x.i += 10;
  x.j += 20;
  x.str[0] = 'H';

  // printf("x = {%d, %d, %s}.\n", x.i, x.j, x.str);

  y.i += 10;
  y.j += 20;
  y.str[0] = 'W';

  // printf("y = {%d, %d, %s}.\n", y.i, y.j, y.str);

  return;
}

int main(int argc, char ** argv)
{
  struct bvt x = {0, 1, "hello"};
  struct bvt y = {2, 3, "world"};

  // printf("x = {%d, %d, %s}.\n", x.i, x.j, x.str);
  // printf("y = {%d, %d, %s}.\n", y.i, y.j, y.str);
   
  byval_test(x, y);
   
  // printf("x = {%d, %d, %s}.\n", x.i, x.j, x.str);
  // printf("y = {%d, %d, %s}.\n", y.i, y.j, y.str);

  return(0);
}
