/* Write pointers into a char * array. Cast the array and
   free all associated pointers while writing into them. */

#include <stdlib.h>
#include <string.h>

#define ARSZ 20

int main()
{
  char *strings[ARSZ];
  char *ptrs;
  int i;

  ptrs = malloc(ARSZ * sizeof(char*));
  for (i = 0; i < ARSZ; i++)
  {
    ((char**)ptrs)[i] = strings[i] = malloc(sizeof(char*) * 100);
    strcpy(strings[i], "String");
  }
  for (i = 0; i < ARSZ; i++)
    free(((char**)ptrs)[i]);
  for (i = 0; i < ARSZ; i++)
    strings[i][0] = '\0';
  return 0;
}
