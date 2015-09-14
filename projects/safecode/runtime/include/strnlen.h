#ifndef _STRNLEN_H
#define _STRNLEN_H

#include <cstddef>

namespace
{
  // This function is identical to strnlen(), which is not found on Darwin.
  inline size_t _strnlen(const char *s, size_t maxlen) {
    size_t i;
    for (i = 0; i < maxlen && s[i]; ++i)
      ;
    return i;
  }
}

#endif
