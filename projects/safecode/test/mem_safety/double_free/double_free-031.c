/* Call free cast from a decomposed integer */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

uint32_t high;
uint32_t low;

void call(void *data)
{
  void (*func)(void *);
  func = (void (*)(void *)) ((((uint64_t) high) << 32) | low);
  printf("%p\n", func);
  printf("%p\n", free);
  func(data);
  free(data);
}

int main()
{
  void *data;
  high = (uint32_t) ((((uint64_t) &free) & 0xffffffff00000000L) >> 32);
  low  = (uint32_t)  &free;
  data = malloc(1000);
  call(data);
  return 0;
}
