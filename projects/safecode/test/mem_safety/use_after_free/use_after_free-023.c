/* Use of function pointer inside freed structure. */

#include <stdlib.h>
#include <stdio.h>

typedef struct
{
  int value;
  void (*(*f))(void);
} func_container;

void func()
{
  printf("function\n");
}

int main()
{
  func_container *c = malloc(sizeof(func_container));
  c->f = malloc(sizeof(void (*)(void)));
  *c->f = func;
  free(c);
  (*c->f)();
  free(c->f);
  return 0;
}
