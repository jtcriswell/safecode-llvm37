/* free() a struct cast to a union using data found in that union */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

struct teststruct {
  void *sptr1, *sptr2;
};

union testunion {
  void *uptr1, *uptr2[2];
};

int main()
{
  struct teststruct *t;

  t = malloc(sizeof(struct teststruct) + sizeof(union testunion));
  t->sptr1 = t;
  printf("t: %p, uptr1: %p\n", t, ((union testunion *)t)->uptr1);
  free(((union testunion *) t)->uptr1);
  free(t);
  return 0;
}
