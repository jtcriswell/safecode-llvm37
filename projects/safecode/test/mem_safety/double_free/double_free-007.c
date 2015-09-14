/* free_cointainers() casts the integral data
   of a struct into a pointer and frees it.
   When the integral data is the address of the
   struct that contains the integer, this
   results in a double free after the
   struct is also freed conventionally. */
   
#include <stdlib.h>
#include <stdint.h>

typedef struct
{
  uint64_t x;
} IntContainer;

void free_container(IntContainer *ic);

void free_container(IntContainer *ic)
{
  IntContainer *u;
  u = (IntContainer *) ic->x;
  free(u);
}

int main()
{
  IntContainer *z;
  uint64_t val;
  z = malloc(sizeof(IntContainer));
  z->x = (uint64_t) z;
  free_container(z);
  free(z);
  return 0;
}
