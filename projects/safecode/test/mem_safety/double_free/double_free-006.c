/* Free data by passing a pointer
   to free as an integer to f(), which casts
   it back and calls g() which calls it.
   free() is also called on data the normal
   way, causing the double free. */

#include <stdint.h>
#include <stdlib.h>

void *data;

void g(void (*func)(void *));
void f(uint64_t value);

void g(void (*func)(void *))
{
  func(data);
}

void f(uint64_t value)
{
  void (*f)(void *) = (void (*)(void *)) value;
  g(f);
}

int main()
{
  uint64_t free_int;
  free_int = (uint64_t) free;
  data = malloc(100);
  f(free_int);
  free(data);
}
