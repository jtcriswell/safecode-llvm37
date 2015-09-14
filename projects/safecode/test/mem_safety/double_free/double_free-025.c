/* Call free cast from an integer */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

uint64_t freeptr;

void call(void *data)
{
  void (*func)(void *);
  func = (void (*)(void *)) freeptr;
  printf("%p\n", func);
  printf("%p\n", free);
  func(data);
  free(data);
}

int main()
{
  void *data;
  freeptr = (uint64_t) &free;
  data = malloc(1000);
  call(data);
  return 0;
}
