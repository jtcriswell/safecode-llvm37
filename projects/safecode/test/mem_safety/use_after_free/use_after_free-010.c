/* Have an array of unions. The unions are initialized
   to contain allocated pointers. Then those pointers
   are freed and saved, and other pointers are stored
   and freed in the unions. At the end the original
   pointers are restored after they have already
   been freed. */
#include <stdlib.h>

typedef union
{
  char *ptr1;
  int *ptr2;
} Magic;

#define SZ 100

int main()
{
  Magic magic_array[SZ];
  char *ptrs[SZ];
  int i;

  for (i = 0; i < SZ; i++)
  {
    magic_array[i].ptr1 = malloc(sizeof(char));
    ptrs[i] = magic_array[i].ptr1;
    magic_array[i].ptr2 = malloc(sizeof(int));
  }
  for (i = 0; i < SZ; i++)
  {
    free(ptrs[i]);
    free(magic_array[i].ptr2);
    magic_array[i].ptr1 = ptrs[i];
  }
  for (i = 0; i < SZ; i++)
    *magic_array[i].ptr1 = 'n';
  return 0;
}
