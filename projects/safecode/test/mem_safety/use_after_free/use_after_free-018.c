/* calloc() an array and access it after it is freed. */
#include <stdlib.h>
#include <stdint.h>

union _test {
  int32_t amt;
  int64_t amt2;
};
  

int main()
{
  union _test *ptrs;
  ptrs = calloc(10, sizeof(union _test));
  free(ptrs);
  ptrs[0].amt = 99;
  return 0;
}
