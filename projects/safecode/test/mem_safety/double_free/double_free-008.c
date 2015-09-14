/* Decompose a pointer into two components and
   free it conventionally as well as freeing
   it again by combining those two components. */
#include <stdlib.h>
#include <stdint.h>

#define SIZE 100

void f(uint64_t a, uint16_t b)
{
  free((void*) (a | b));
}

int main()
{
  uint16_t component_1;
  uint64_t component_2;
  char *ptr;
  ptr = malloc(sizeof(char) * SIZE);
  component_2 = (uint64_t) ptr;
  component_1 = component_2 & 0xffff;
  component_2 ^= component_1;
  free(ptr);
  f(component_2, component_1);
  return 0;
}
