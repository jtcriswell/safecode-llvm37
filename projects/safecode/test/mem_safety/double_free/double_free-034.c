/* Double free of data used as a different type. */

#include <stdlib.h>

typedef void (*fptr)(void *);

#define SIZE 50

#define FPTR(ptr)  ((fptr *)ptr)
#define VOID(ptr)  ((void **)ptr)

int main()
{
  short *p;

  p = calloc(SIZE, sizeof(fptr) + sizeof(short) + sizeof(void*));
  FPTR(p)[0] = free;
  VOID(&FPTR(p)[1])[0] = p;
  FPTR(p)[0]( VOID(&FPTR(p)[1])[0] );
  free(p);
  return 0;
}
