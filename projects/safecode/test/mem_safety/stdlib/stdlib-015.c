/* memset() of array with one byte overwrite */

#include <string.h>
#include <stdint.h>

int main()
{
  int32_t buf[10];
  memset(buf, 41, 'a');
  return 0;
}
