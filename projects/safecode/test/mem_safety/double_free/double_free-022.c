/* Decompose an allocated pointer into an array of integers,
   which each integer representing a single bit. Free the allocated
   pointer once and then reconstruct the pointer from the
   array of bits and free it again. */

#include <stdlib.h>
#include <stdint.h>

#define BITS (sizeof(char*) * 8)

void f(uint64_t x)
{
  char *p = (char*) x;
  free(p);
}

uint64_t reconstruct(int *bits)
{
  int i;
  uint64_t tmp = (uint64_t) 0;
  for (i = BITS - 1; i >= 0; i--)
    tmp = (tmp << 1) | ((uint64_t) bits[i]);
  return tmp;
}

void start(int *bits)
{
  char *a;
  int i;
  a = malloc(sizeof(char) * 1000);
  for (i = 0; i < BITS; i++)
    bits[i] = (((uint64_t) a) & (((uint64_t) 0x1) << i)) != (uint64_t) 0;
  free(a);
}

uint64_t getbits()
{
  int bits[BITS];
  start(bits);
  return reconstruct(bits);
}

int main()
{
  f(getbits());
  return 0;
}
