/* sscanf() writes to a free'd pointer */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

void scan_3_numbers(const char *string, int *count, int numbers[3])
{
  sscanf(string, "%i %i %i%n", &numbers[0], &numbers[1], &numbers[2], count);
}

int main()
{
  int *ptr;
  int array[3];
  char string[] = "0 1 2 3 4 5";
  ptr = malloc(sizeof(int));
  free(ptr);
  scan_3_numbers(string, ptr, array);
  printf("%i %i %i\n", array[0], array[1], array[2]);
  return 0;
}
