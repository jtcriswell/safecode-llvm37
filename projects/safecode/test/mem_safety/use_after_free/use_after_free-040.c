/* Retrieve a float from a void* array that has been free'd. */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

float second_float(void *array)
{
  float f = ((float *)array)[1];
  return f;
}

int main()
{
  int64_t *array;
  array = malloc(sizeof(float) * 2 + sizeof(int64_t) * 2);
  free(array);
  printf("%f\n", (double) second_float(array));
  return 0;
}
