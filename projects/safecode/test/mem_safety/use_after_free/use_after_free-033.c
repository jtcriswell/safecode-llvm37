/* Use after free of item cast to void * */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

void print_second_byte(void *n)
{
  unsigned int i;
  i = ((uint8_t*)n)[1];
  printf("%u\n", i);
}

int main()
{
  int16_t *ptr;
  ptr = malloc(sizeof(int16_t));
  *ptr = 0xffff;
  free(ptr);
  print_second_byte(ptr);
  return 0;
}
