// RUN: test.sh -e -t %t %s

// bcopy() with the destination object being too small to hold the data.

#include <strings.h>
#include <stdint.h>

int main()
{
  int32_t src[100];
  int8_t  dst[400];

  bcopy(src, &dst[300], 400);
  return 0;
}
