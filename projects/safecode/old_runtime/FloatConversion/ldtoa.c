#include <limits.h>
#include <stdint.h>
#include <math.h>

#include "gdtoa.h"

/* IEEE extended precision floating point format, little endian byte ordering */
union ieee_ext
{
  struct
  {
    uint32_t mt0:32;
    uint32_t mt1:32;
    uint32_t exp:15;
    uint32_t sgn:1;
    uint32_t etc:16;
  } ieee;
  long double d;
};

/*

   __ldtoa: wrapper for long double conversion using gdtoa()
  
   NOTE: This only works for IEEE extended precision long doubles.
  
   Input:
     ld       - a pointer to the value to convert

     The rest of the parameters are the analogous to the corresponding
     parameters to gdtoa().
  
   Returns:
     This function returns a string that is to be free'd with freedtoa().

*/
char *
__ldtoa(long double *ld,
        int mode,
        int ndigits,
        int *decpt,
        int *sign,
        char **rve)
{
  FPI fpi =
  {
    64,                 /* nbits */
    1 - 16383 - 63,     /* emin  */
    32766 - 16383 - 63, /* emax  */
    1,                  /* rounding */
    0                   /* sudden_underflow */
  };
  union ieee_ext *l;
  uint32_t bits[2];
  int exp, kind;
  char *ret;

  l = (union ieee_ext *) ld;
  *sign = l->ieee.sgn;
  exp = l->ieee.exp;
  bits[0] = l->ieee.mt0;
  bits[1] = l->ieee.mt1;

  if (isnan(*ld))        /* NaN                */
      kind = STRTOG_NaN;
  else if (isinf(*ld))   /* Infinity           */
      kind = STRTOG_Infinite;
  else if (exp == 0)     /* Denormalized       */
  {
    kind = STRTOG_Denormal;
    exp = 1;
  }
  else                   /* Normalized or zero */
  {
    if (bits[0] | bits[1])
      kind = STRTOG_Normal;
    else
      kind = STRTOG_Zero;
  }

  exp -= 16383 + 63;

  ret = gdtoa(&fpi, exp, &bits[0], &kind, mode, ndigits, decpt, rve);

  if (*decpt == -32768)
    *decpt = INT_MAX;

  return ret;
}
