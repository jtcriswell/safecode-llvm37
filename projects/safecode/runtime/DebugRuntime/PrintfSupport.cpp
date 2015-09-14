//===- PrintfSupport.cpp - Secure printf() replacement --------------------===//
// 
//                            The SAFECode Compiler
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file implements a secure runtime replacement for printf() and similar
// functions.
//
//===----------------------------------------------------------------------===//

//
// This code is derived from OpenBSD's vfprintf.c; original license follows:
//
// $OpenBSD: vfprintf.c,v 1.60 2010/12/22 14:54:44 millert Exp
// 
// Copyright (c) 1990 The Regents of the University of California.
// All rights reserved.
//
// This code is derived from software contributed to Berkeley by
// Chris Torek.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
// 3. Neither the name of the University nor the names of its contributors
//    may be used to endorse or promote products derived from this software
//    without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.
//

//
// Actual printf innards.
//
// This code is large and complicated...
//

#include "safecode/Config/config.h"
#include "FormatStrings.h"

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>

#include <algorithm>
#include <iostream>

using std::cerr;
using std::endl;
using std::min;
using std::max;
using std::map;

//
// This structure holds a single buffer to be printed.
//
struct siov
{
  // Start of buffer
  const char *iov_base;
  // Number of bytes to print
  size_t iov_len;
};

//
// This structure holds an array of buffers which are printed at one time.
//
struct suio
{
  // Array of buffers
  struct siov *uio_iov;
  // Number of buffers
  int uio_iovcnt;
  // Total number of bytes to print
  size_t uio_resid;
};

//
// do_output()
//
// Flushes out all the vectors defined by the given uio, then resets it so that
// it can be reused.
//
// Inputs:
//  c       - a pointer to the relevant call_info structure
//  p       - a pointer to the output_parameter structure describing the output
//            location
//  uio     - a pointer to an suio structure that contains the buffers to be
//            written
//
// Returns nonzero on error, zero on success.
//
static int
do_output(call_info *c, output_parameter *p, struct suio *uio)
{
  //
  // Do a write to an output file if that's the output destination.
  //
  if (p->output_kind == output_parameter::OUTPUT_TO_FILE)
  {
    FILE *out = p->output.file;
    for (int i = 0; i < uio->uio_iovcnt; ++i)
    {
      size_t amt, sz = uio->uio_iov[i].iov_len;
      //
      // Call fwrite_unlocked() for performance since the output stream should
      // already be locked by this thread.
      //
      // Use fwrite() on platforms without fwrite_unlocked().
      //
#ifndef HAVE_FWRITE_UNLOCKED
#define fwrite_unlocked fwrite
#endif
      amt = fwrite_unlocked(&uio->uio_iov[i].iov_base[0], 1, sz, out);
      if (amt < sz)
        return 1; // Output error
    }
    uio->uio_resid = 0;
    uio->uio_iovcnt = 0;
    return 0;
  }
  //
  // Write into a (fixed) buffer.
  //
  else if (p->output_kind == output_parameter::OUTPUT_TO_STRING)
  {
    char *dest  = p->output.string.string;
    size_t &pos = p->output.string.pos;
    size_t n    = p->output.string.n;
    size_t msz  = p->output.string.maxsz;
    size_t amt;
    if (pos > n) // Don't write anything if we've reached the limit.
    {
      uio->uio_resid = 0;
      uio->uio_iovcnt = 0;
      return 0;
    }
    for (int i = 0; i < uio->uio_iovcnt; ++i)
    {
      //
      // Get the amount of data that we would *like* to write into the buffer.
      //
      amt = uio->uio_iov[i].iov_len;

      //
      // If the user-imposed limit is within the memory bounds limit, then
      // check to see if we reach the user-imposed limit.  If so, fill the
      // string to the allowed capacity and return.
      //
      if ((n - pos) <= (msz - pos))
      {
        if (amt > n - pos)
        {
          memcpy(&dest[pos], &uio->uio_iov[i].iov_base[0], n - pos);
          pos = n;
          uio->uio_resid  = 0;
          uio->uio_iovcnt = 0;
          return 0;
        }
      }

      //
      // Check for out of bounds write. Report an out of bounds write only once.
      //
      if (pos < msz && amt > msz - pos)
      {
        pointer_info *info = p->output.string.info;
        cerr << "Destination string not long enough!" << endl;
        write_out_of_bounds_error(c, info, msz, pos + amt);
      }

      //
      // Check to see if the we'll reach the user imposed size.
      // If so, fill the string to the allowed capacity and return.
      //
      if (amt > n - pos)
      {
        memcpy(&dest[pos], &uio->uio_iov[i].iov_base[0], n - pos);
        pos = n;
        uio->uio_resid  = 0;
        uio->uio_iovcnt = 0;
        return 0;
      }
      //
      // Otherwise, copy over the buffer and continue.
      //
      memcpy(&dest[pos], &uio->uio_iov[i].iov_base[0], amt);
      pos += amt;
    }
    uio->uio_resid  = 0;
    uio->uio_iovcnt = 0;
    return 0;
  }
  //
  // Write into a buffer that's dynamically allocated to fit the entire write.
  //
  else // if (p->output_kind == output_parameter::OUTPUT_TO_ALLOCATED_STRING)
  {
    char  *&dest  = p->output.alloced_string.string;
    size_t &pos   = p->output.alloced_string.pos;
    size_t &bufsz = p->output.alloced_string.bufsz;
    size_t amt;

    if (dest == 0)
      return 1; // Output error
    //
    // Allocate a new string if the old one isn't large enough.
    //
    if (uio->uio_resid > bufsz - pos)
    {
      do bufsz *= 2; while (uio->uio_resid > bufsz - pos);
      dest = (char *) realloc(dest, bufsz);
      if (dest == 0)
        return 1; // Output error
    }
    //
    // Copy the characters over.
    //
    for (int i = 0; i < uio->uio_iovcnt; ++i)
    {
      amt = uio->uio_iov[i].iov_len;
      memcpy(&dest[pos], &uio->uio_iov[i].iov_base[0], amt);
      pos += amt;
    }
    uio->uio_resid  = 0;
    uio->uio_iovcnt = 0;
    return 0;
  }
}

//
// An element to hold the value of a positional argument in the positional
// argument table.
//
union arg
{
  int     intarg;
  unsigned int    uintarg;
  long      longarg;
  unsigned long   ulongarg;
  long long   longlongarg;
  unsigned long long  ulonglongarg;
  ptrdiff_t   ptrdiffarg;
  size_t      sizearg;
  ssize_t     ssizearg;
  intmax_t    intmaxarg;
  uintmax_t   uintmaxarg;
  void        *pvoidarg;
  wint_t      wintarg;

#ifdef FLOATING_POINT
  double      doublearg;
  long double   longdoublearg;
#endif

};

static inline int
find_arguments(const char *fmt0,
               va_list ap,
               union arg **argtable,
               size_t *argtablesiz,
               unsigned vargc);

static inline int
grow_type_table(unsigned char **typetable, size_t *tablesize, size_t minsz);

#ifdef FLOATING_POINT
#include "../include/FloatConversion.h"

// The default floating point precision
#define DEFPREC   6 

static int exponent(char *, int, int);
#endif // FLOATING_POINT

//
// The size of the buffer we use as scratch space for integer
// conversions, among other things.  Technically, we would need the
// most space for base 10 conversions with thousands' grouping
// characters between each pair of digits.  100 bytes is a
// conservative overestimate even for a 128-bit uintmax_t.
// 
#define BUF 100

#define STATIC_ARG_TBL_SIZE 32 // Size of static argument table.


//
// Macros for converting digits to letters and vice versa
//
#define to_digit(c) ((c) - '0')
#define is_digit(c) ((unsigned)to_digit(c) <= 9)
#define to_char(n)  ((n) + '0')

//
// Flags used during conversion.
//
#define ALT       0x0001    // alternate form
#define LADJUST   0x0004    // left adjustment
#define LONGDBL   0x0008    // long double
#define LONGINT   0x0010    // long integer
#define LLONGINT  0x0020    // long long integer
#define SHORTINT  0x0040    // short integer
#define ZEROPAD   0x0080    // zero (as opposed to blank) pad
#define FPT       0x0100    // Floating point number
#define PTRINT    0x0200    // (unsigned) ptrdiff_t
#define SIZEINT   0x0400    // (signed) size_t
#define CHARINT   0x0800    // 8 bit integer
#define MAXINT    0x1000    // largest integer size (intmax_t)

//
// handle_s_directive()
//
// Prepare a string for printing.
//
// Inputs:
//  ci         - a pointer to the relevant call_info structure
//  options    - options passed to the format string function
//  p          - a pointer to the pointer_info structure that contains the
//               string to print
//  flags      - the relevant flags from the parsed format string
//  cp         - a pointer into which is written the location of the string to
//               print
//  len        - a pointer to a size_t object, into which is written the number
//               of bytes to print
//  mbstr      - a pointer into which will be written the location of any string
//               allocated for purposes of converting a multibyte sequence
//  prec       - an integer representing the precision
//
// Returns:
//  The function returns no value. Instead, *cp is set to point to the buffer
//  containing the string of which the first *len bytes should be written.
//  If this string is the result of a wide character to multibyte conversion,
//  it is an allocated string and *mbstr is also set to point to it. Otherwise
//  *mbstr is set to NULL.
//
static inline void
handle_s_directive(call_info *ci,
                   options_t options,
                   pointer_info *p,
                   int flags,
                   const char **cp,
                   size_t *len,
                   char **mbstr,
                   int prec)
{
  //
  // Free any previously allocated buffers.
  //
  if (*mbstr)
    free(*mbstr);
  //
  // Load the object boundaries.
  //
  find_object(ci, p);
  //
  // A negative precision indicates the precision has not been set. Set it to
  // the maximum allowed value and continue.
  //
  if (prec < 0)
    prec = INT_MAX;
  if (!(flags & LONGINT))
  {
    //
    // Print out a regular string.
    //
    // maxbytes is the maximum number of bytes that can be read safely from
    // the input while respecting the precision requirements.
    //
    size_t maxbytes = (p->flags & HAVEBOUNDS) ? 
      min((size_t) prec, object_len(p)) :
      (size_t) prec;
    char *str = (char *) p->ptr;
    char *r = (char *) memchr(str, 0, maxbytes);
    *cp = str;
    *mbstr = 0;
    if (r)
      *len = r - str;
    else if ((unsigned)prec <= maxbytes)
      *len = prec;
    else
    {
      *len = maxbytes;
      cerr << "Reading string out of bounds!" << endl;
      out_of_bounds_error(ci, p, maxbytes);
    }
    return;
  }
  else
  {
    //
    // Print out a wide character string converted into multibyte
    // characters.
    //
    // We can't tell how long the resulting multibyte character string will
    // be without converting it character by character.
    //
    // maxbytes represents the maximum number of bytes we can read from the
    // wide character string in a safe manner.
    //
    size_t maxbytes  = (p->flags & HAVEBOUNDS) ? object_len(p) : SIZE_MAX;
    mbstate_t ps;
    wchar_t *input  = (wchar_t *) p->ptr;
    size_t bytesread = 0; // the current number of bytes read from the wide
                          // character array
    bool   overread  = false; // whether the wide character array is read out
                              // of bounds
    size_t destpos   = 0;     // the current index of the converted character
                              // string
    size_t destsize;          // the current size of the converted character
                              // string object
    size_t convlen   = 0;     // the length of the most recent converted
                              // character
    wchar_t nextch;           // the next character to convert
    char buffer[MB_LEN_MAX];  // buffer for holding character conversions
    //
    // Set up the destination.
    //
    *mbstr = (char *) malloc((destsize = 64));
    memset(&ps, 0, sizeof(mbstate_t));
    bytesread = 0;

    if (*mbstr == 0)
    {
      *cp = "(error)";
      *len = 7;
      return;
    }
    //
    // Read the wide character string and convert it to a multibyte character
    // string using wcrtomb().
    //
    while (1)
    {
      //
      // If the next wide character to be read will cause us to read out of
      // bounds of the array, issue a SAFECode error.
      //
      if (sizeof(wchar_t) > (maxbytes - bytesread) && overread == false)
      {
        overread = true;
        cerr << "Reading wide character string out of bounds!" << endl;
        out_of_bounds_error(ci, p, maxbytes);
      }
      //
      // Read the next wide character from the input. If it's the string
      // terminator, terminate the conversion.
      //
      nextch = input[bytesread / sizeof(wchar_t)];
      if (nextch == L'\0')
        break;
      bytesread += sizeof(wchar_t);
      //
      // Convert the wide character into a multibyte character.
      //
      convlen = wcrtomb(&buffer[0], nextch, &ps);
      //
      // Check for conversion error. If so, terminate the conversion
      // at this point.
      //
      if (convlen == (size_t) -1)
        break;
      //
      // Check if 1) appending the destination character keeps us within the
      // allowed precision, and 2) if appending the destination character
      // requires making the conversion buffer larger.
      //
      else if (convlen > (size_t) prec - destpos)
        break;
      else if (convlen > destsize - destpos)
      {
        do destsize *= 2; while (convlen > destsize - destpos);
        *mbstr = (char *) realloc(*mbstr, destsize);
        if (*mbstr == 0)
        {
          *cp  = "(error)";
          *len = 7;
          return;
        }
      }
      //
      // Finally, append the converted character.
      //
      memcpy(&(*mbstr)[destpos], &buffer[0], convlen);
      destpos += convlen;
      if (destpos == (size_t) prec)
        break;
    }
    *cp  = *mbstr;
    *len = destpos;
    return;
  }
}

//
// The main logic for printf() style functions
//
// Inputs:
//   options   - options controlling some aspects of execution
//   output    - a reference to the output_parameter structure describing
//               where to do the write
//   cinfo     - a reference to the call_info structure which contains
//               information about the va_list
//   fmt0      - the format string
//   ap        - the variable argument list
//
// Returns:
//   This function returns the number of characters that would have been
//   written had the output been unbounded on success, and a negative number on
//   failure.
//
// IMPORTANT IMPLEMENTATION LIMITATIONS
//   - Floating point number printing is not thread safe
//   - No support for (nonstandard) locale-defined thousands grouping
//     (the "'" flag)
//
int
internal_printf(const options_t options,
                output_parameter &output,
                call_info &cinfo,
                const char *fmt0,
                va_list ap)
{
  const char *fmt;      // format string
  int ch;               // character from fmt
  int n, n2;            // handy integers (short term usage)
  const char *cp;       // handy const char pointer (short term usage)
  char *bp;             // handy char pointer
  struct siov *iovp;    // for PRINT macro
  int flags;            // flags as above
  int ret;              // return value accumulator
  int width;            // width from format (%8d), or 0
  int prec;             // precision from format; <0 for N/A
  char sign;            // sign prefix (' ', '+', '-', or \0)

#ifdef FLOATING_POINT
  //
  // We can decompose the printed representation of floating
  // point numbers into several parts, some of which may be empty:
  //
  // [+|-| ] [0x|0X] MMM . NNN [e|E|p|P] [+|-] ZZ
  //    A       B     ---C---      D       E   F
  //
  // A: 'sign' holds this value if present; '\0' otherwise
  // B: ox[1] holds the 'x' or 'X'; '\0' if not hexadecimal
  // C: cp points to the string MMMNNN.  Leading and trailing
  //    zeros are not in the string and must be added.
  // D: expchar holds this character; '\0' if no exponent, e.g. %f
  // F: at least two digits for decimal, at least one digit for hex
  //
  char *decimal_point = localeconv()->decimal_point;
  int signflag;     // true if float is negative
  union             // floating point arguments %[aAeEfFgG]
  {
    double dbl;
    long double ldbl;
  } fparg;
  int expt;         // integer value of exponent
  char expchar = 0; // exponent character: [eEpP\0]
  char *dtoaend;    // pointer to end of converted digits
  int expsize = 0;  // character count for expstr
  int lead = 0;     // sig figs before decimal or group sep
  int ndig = 0;     // actual number of digits returned by dtoa
  // Set this to the maximum number of digits in an exponent.
  // This is a large overestimate.
#define MAXEXPDIG 32
  char expstr[MAXEXPDIG+2]; // buffer for exponent string: e+ZZZ
  char *dtoaresult = 0;
#endif

  uintmax_t _umax;              // integer arguments %[diouxX]
  enum { OCT, DEC, HEX } base;  // base for %[diouxX] conversion
  int dprec;         // a copy of prec if %[diouxX], 0 otherwise
  int realsz;        // field size expanded by dprec
  int size;          // size of converted field or string
  const char *xdigs; // digits for %[xX] conversion

  const int NIOV = 8;
  struct suio uio;       // output information: summary
  struct siov iov[NIOV]; // ... and individual io vectors
  char buf[BUF];         // buffer with space for digits of uintmax_t
  char ox[2];            // space for 0x; ox[1] is either x, X, or \0
  union arg *argtable;   // args, built due to positional arg
  union arg statargtable[STATIC_ARG_TBL_SIZE]; // initial argument table
  size_t argtablesiz;    // number of elements in the positional arg table
  int nextarg;           // 1-based argument index
  va_list orgap;         // original argument pointer
  pointer_info *p;       // handy pointer_info structure
  wchar_t wc;            // the input character to process
  char *mbstr;           // a string that is a result of multibyte conversion
  mbstate_t ps;          // conversion state

  //
  // The next four strings are used by the printing macros.
  //
  // Choose PADSIZE to trade efficiency vs. size.  If larger printf
  // fields occur frequently, increase PADSIZE and make the initialisers
  // below longer.
  //
#define PADSIZE 16    // pad chunk size
  static char blanks[PADSIZE + 1] = "                ";
  static char zeroes[PADSIZE + 1] = "0000000000000000";
  static char xdigs_lower[] = "0123456789abcdef";
  static char xdigs_upper[] = "0123456789ABCDEF";

  xdigs = 0;

  const unsigned vargc = cinfo.vargc; // number of arguments in the va_list

  //
  // This map is used if POINTERS_UNWRAPPED is set in the options, to wrap
  // pointers in the va_list that do not have a pointer_info structure around
  // them.
  //
  map<void *, pointer_info *> ptr_infos;

  //
  // Printing macros
  //
  // BEWARE, these `goto error' on error, and PAD uses `n'.
  //

//
// Add the first 'len' bytes of 'ptr' to the output queue.
//
#define PRINT(ptr, len) do {                                                   \
  iovp->iov_base = (ptr);                                                      \
  iovp->iov_len = (len);                                                       \
  uio.uio_resid += (len);                                                      \
  iovp++;                                                                      \
  if (++uio.uio_iovcnt >= NIOV)                                                \
  {                                                                            \
    if (do_output(&cinfo, &output, &uio))                                      \
      goto error;                                                              \
    iovp = iov;                                                                \
  }                                                                            \
} while (0)

//
// Output 'howmany' bytes from a pad chunk 'with'.
//
#define PAD(howmany, with) do {                                                \
  if ((n = (howmany)) > 0)                                                     \
  {                                                                            \
    while (n > PADSIZE)                                                        \
    {                                                                          \
      PRINT((with), PADSIZE);                                                  \
      n -= PADSIZE;                                                            \
    }                                                                          \
    PRINT((with), n);                                                          \
  }                                                                            \
} while (0)

//
// Output a string of exactly len bytes, consisting of the string found in the
// range [p, ep), right-padded by the pad characters in 'with', if necessary.
//
#define PRINTANDPAD(p, ep, len, with) do {                                     \
  n2 = (ep) - (p);                                                             \
  if (n2 > (len))                                                              \
    n2 = (len);                                                                \
  if (n2 > 0)                                                                  \
    PRINT((p), n2);                                                            \
  PAD((len) - (n2 > 0 ? n2 : 0), (with));                                      \
} while(0)

//
// Flush any output buffers yet to be printed.
//
#define FLUSH() do {                                                           \
  if (uio.uio_resid && do_output(&cinfo, &output, &uio))                       \
    goto error;                                                                \
  uio.uio_iovcnt = 0;                                                          \
  iovp = iov;                                                                  \
} while (0)


  //
  // Integer retrieval macros
  //
  // To extend shorts properly, we need both signed and unsigned
  // argument extraction methods.
  //

//
// Retrieve a signed argument.
//
#define SARG() ((intmax_t)                                                     \
  ((flags&MAXINT   ? GETARG(intmax_t)   :                                      \
    flags&LLONGINT ? GETARG(long long)  :                                      \
    flags&LONGINT  ? GETARG(long)       :                                      \
    flags&PTRINT   ? GETARG(ptrdiff_t)  :                                      \
    flags&SIZEINT  ? GETARG(ssize_t)    :                                      \
    flags&SHORTINT ? (short)GETARG(int) :                                      \
    flags&CHARINT  ? (signed char)GETARG(int) :                                \
    GETARG(int))))

//
// Retrieve an unsigned argument.
//
#define UARG() ((uintmax_t)                                                    \
  ((flags&MAXINT   ? GETARG(uintmax_t)            :                            \
    flags&LLONGINT ? GETARG(unsigned long long)   :                            \
    flags&LONGINT  ? GETARG(unsigned long)        :                            \
    flags&PTRINT   ? (uintptr_t)GETARG(ptrdiff_t) : /* XXX */                  \
    flags&SIZEINT  ? GETARG(size_t)               :                            \
    flags&SHORTINT ? (unsigned short)GETARG(int)  :                            \
    flags&CHARINT  ? (unsigned char)GETARG(int)   :                            \
    GETARG(unsigned int))))

//
// Append a digit to a value and check for overflow.
//
#define APPEND_DIGIT(val, dig) do {                                            \
  if ((val) > INT_MAX / 10)                                                    \
    goto overflow;                                                             \
  (val) *= 10;                                                                 \
  if ((val) > INT_MAX - to_digit((dig)))                                       \
    goto overflow;                                                             \
  (val) += to_digit((dig));                                                    \
} while (0)


  //
  // Macros for getting arguments
  //

//
// Get * arguments, including the form *nn$, into val.  Preserve the nextarg
// that the argument can be gotten once the type is determined.
//
#define GETASTER(val)                                                          \
  n2 = 0;                                                                      \
  cp = fmt;                                                                    \
  while (is_digit(*cp))                                                        \
  {                                                                            \
    APPEND_DIGIT(n2, *cp);                                                     \
    cp++;                                                                      \
  }                                                                            \
  if (*cp == '$')                                                              \
  {                                                                            \
    int hold = nextarg;                                                        \
    if (argtable == 0)                                                         \
    {                                                                          \
      argtable = statargtable;                                                 \
      if (find_arguments(fmt0, orgap, &argtable, &argtablesiz, vargc) == -1)   \
        goto error;                                                            \
    }                                                                          \
    nextarg = n2;                                                              \
    val = GETARG(int);                                                         \
    nextarg = hold;                                                            \
    fmt = ++cp;                                                                \
  }                                                                            \
  else                                                                         \
    val = GETARG(int);

//
// Get the actual argument indexed by nextarg. If the argument table is
// built, use it to get the argument.  If its not, get the next
// argument (and arguments must be gotten sequentially).
//
#define GETARG(type) (                                                         \
  varg_check(&cinfo, options, nextarg++),                                      \
  (argtable != 0) ? *((type *) &argtable[nextarg-1]) : va_arg(ap, type) )

//
// Get the pointer_info structure associated with the next argument.
//
#define GETPTRARG() (                                                          \
  (pointer_info *) wrap_pointer(options, GETARG(void *), ptr_infos) )

//
// Write the current number of bytes that have been written into the next
// argument, which should be a pointer to 'type'.
//
#define WRITECOUNTAS(type)                                                     \
  do {                                                                         \
    p = GETPTRARG();                                                           \
    write_check(&cinfo, options, p, sizeof(type));                             \
    *(type *)(p->ptr) = ret;                                                   \
  } while (0)

  fmt = fmt0;
  argtable = 0;
  nextarg = 1;
  va_copy(orgap, ap);
  uio.uio_iov = iovp = iov;
  uio.uio_resid = 0;
  uio.uio_iovcnt = 0;
  ret = 0;
  mbstr = 0;

  memset(&ps, 0, sizeof(mbstate_t));

  //
  // Scan the format for conversions (`%' character).
  //
  for (;;)
  {
    cp = fmt;
    while ((n = mbrtowc(&wc, fmt, MB_CUR_MAX, &ps)) > 0)
    {
      fmt += n;
      if (wc == '%')
      {
        fmt--;
        break;
      }
    }
    if (fmt != cp)
    {
      ptrdiff_t m = fmt - cp;
      if (m < 0 || m > INT_MAX - ret)
        goto overflow;
      PRINT(cp, m);
      ret += m;
    }
    if (n <= 0)
      goto done;
    fmt++;    // skip over '%'

    flags = 0;
    dprec = 0;
    width = 0;
    prec = -1;
    sign = '\0';
    ox[1] = '\0';

    //
    // This is the main directive parsing section.
    //
rflag:    ch = *fmt++;
reswitch:
    switch (ch)
    {
    //
    // This section parses flags, precision, and field width values.
    //
    // ' ' flag
    //
    case ' ':
      //
      // ``If the space and + flags both appear, the space
      // flag will be ignored.''
      //  -- ANSI X3J11
      //
      if (!sign)
        sign = ' ';
      goto rflag;
    //
    // '#' flag
    //
    case '#':
      flags |= ALT;
      goto rflag;
    case '\'':
      //
      // The grouping flag is recognized but is not implemented.
      //
      goto rflag;
    //
    // Parse field width given in an argument
    //
    case '*':
      //
      // ``A negative field width argument is taken as a
      // - flag followed by a positive field width.''
      //  -- ANSI X3J11
      // They don't exclude field widths read from args.
      //
      GETASTER(width);
      if (width >= 0)
        goto rflag;
      if (width == INT_MIN)
        goto overflow;
      width = -width;
      // FALLTHROUGH
    case '-':
      flags |= LADJUST;
      goto rflag;
    case '+':
      sign = '+';
      goto rflag;
    //
    // Parse the precision.
    //
    case '.':
      if ((ch = *fmt++) == '*')
      {
        GETASTER(n);
        prec = n < 0 ? -1 : n;
        goto rflag;
      }
      n = 0;
      while (is_digit(ch))
      {
        APPEND_DIGIT(n, ch);
        ch = *fmt++;
      }
      //
      // If the number ends with a $, this indicates a positional argument.
      // So parse the whole format string for positional arguments.
      //
      if (ch == '$')
      {
        nextarg = n;
        if (argtable == 0)
        {
          argtable = statargtable;
          find_arguments(fmt0, orgap, &argtable, &argtablesiz, vargc);
        }
        goto rflag;
      }
      prec = n;
      goto reswitch;
    //
    // '0' flag
    //
    case '0':
      //
      // ``Note that 0 is taken as a flag, not as the
      // beginning of a field width.''
      //  -- ANSI X3J11
      //
      flags |= ZEROPAD;
      goto rflag;
    //
    // Reading a number here indicates either a field width or a positional
    // argument. They are distinguished since positional arguments end in $.
    //
    case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      n = 0;
      do
      {
        APPEND_DIGIT(n, ch);
        ch = *fmt++;
      } while (is_digit(ch));
      //
      // If the number ends with a $, this indicates a positional argument.
      // So parse the whole format string for positional arguments.
      //
      if (ch == '$')
      {
        nextarg = n;
        if (argtable == 0)
        {
          argtable = statargtable;
          find_arguments(fmt0, orgap, &argtable, &argtablesiz, vargc);
        }
        goto rflag;
      }
      width = n;
      goto reswitch;
    //
    // This section parses length modifiers.
    //

#ifdef FLOATING_POINT
    case 'L':
      flags |= LONGDBL;
      goto rflag;
#endif

    case 'h':
      if (*fmt == 'h')
      {
        fmt++;
        flags |= CHARINT;
      }
      else
        flags |= SHORTINT;
      goto rflag;
    case 'j':
      flags |= MAXINT;
      goto rflag;
    case 'l':
      if (*fmt == 'l')
      {
        fmt++;
        flags |= LLONGINT;
      }
      else
        flags |= LONGINT;
      goto rflag;
    case 'q':
      flags |= LLONGINT;
      goto rflag;
    case 't':
      flags |= PTRINT;
      goto rflag;
    case 'z':
      flags |= SIZEINT;
      goto rflag;
    //
    // This section parses conversion specifiers and gives instructions on how
    // to print the output.
    //

    //
    // 'c' specifier
    //
    case 'c':
      sign = '\0';
      if (!(flags & LONGINT))
      {
        buf[0] = GETARG(int);
        cp = buf;
        size = 1;
      }
      //
      // Handle wide characters correctly.
      //
      else
      {
        wint_t wc = GETARG(wint_t);
        mbstate_t st;
        size_t sz;
        if (mbstr)
          free(mbstr);
        mbstr = (char *) malloc(MB_LEN_MAX);
        if (mbstr == 0)
        {
          cp = "(error)";
          size = 8;
          break;
        }
        else if ((wchar_t) wc == L'\0')
        {
          //
          // Print nothing.
          //
          cp = "";
          size = 0;
          break;
        }
        memset(&st, 0, sizeof(mbstate_t));
        sz = wcrtomb(&mbstr[0], (wchar_t) wc, &st);
        //
        // Handle printing errors.
        //
        if (sz == (size_t) -1)
        {
          cp = "";
          size = 0;
          break;
        }
        cp = mbstr;
        size = (int) sz;
      }
      break;
    //
    // %D, %d, %i specifiers
    //
    case 'D':
      flags |= LONGINT;
      // FALLTHROUGH
    case 'd':
    case 'i':
      _umax = SARG();
      if ((intmax_t)_umax < 0)
      {
        _umax = -_umax;
        sign = '-';
      }
      base = DEC;
      goto number;

#ifdef FLOATING_POINT
    //
    // %a, %A specifiers
    //
    case 'a':
    case 'A':
      if (ch == 'a')
      {
        ox[1] = 'x';
        xdigs = xdigs_lower;
        expchar = 'p';
      }
      else
      {
        ox[1] = 'X';
        xdigs = xdigs_upper;
        expchar = 'P';
      }
      if (prec >= 0)
        prec++;
      if (dtoaresult)
        __freedtoa(dtoaresult);
      if (flags & LONGDBL)
      {
        fparg.ldbl = GETARG(long double);
        cp = dtoaresult =
          __hldtoa(fparg.ldbl, xdigs, prec, &expt, &signflag, &dtoaend);
        if (dtoaresult == 0)
        {
          errno = ENOMEM;
          goto error;
        }
      }
      else
      {
        fparg.dbl = GETARG(double);
        cp = dtoaresult =
          __hdtoa(fparg.dbl, xdigs, prec, &expt, &signflag, &dtoaend);
        if (dtoaresult == 0)
        {
          errno = ENOMEM;
          goto error;
        }
      }
      if (prec < 0)
        prec = dtoaend - cp;
      if (expt == INT_MAX)
        ox[1] = '\0';
      goto fp_common;
    //
    // %e, %E specifiers
    //
    // This is the form [-]d.ddde[+/-]dd.
    // The number of digits after the decimal point is the precision.
    //
    case 'e':
    case 'E':
      expchar = ch;
      if (prec < 0) // account for digit before decpt
        prec = DEFPREC + 1;
      else
        prec++;
      goto fp_begin;
    //
    // %f, %F specifiers
    //
    // This is the form ddd.dddd. The number of digits after the decimal point
    // is the precision.
    //
    case 'f':
    case 'F':
      expchar = '\0';
      goto fp_begin;
    //
    // %g, %G specifiers
    //
    // 'e' or 'f' style, depending on the precision and exponent.
    //
    case 'g':
    case 'G':
      expchar = ch - ('g' - 'e');
      if (prec == 0)
        prec = 1;
fp_begin:
      if (prec < 0)
        prec = DEFPREC;
      if (dtoaresult)
        __freedtoa(dtoaresult);
      if (flags & LONGDBL)
      {
        fparg.ldbl = GETARG(long double);
        cp = dtoaresult =
        __ldtoa(&fparg.ldbl, expchar ? 2 : 3, prec, &expt, &signflag, &dtoaend);
        if (dtoaresult == 0)
        {
          errno = ENOMEM;
          goto error;
        }
      }
      else
      {
        fparg.dbl = GETARG(double);
        //
        // There is very sparse documentation for this function call. I'll
        // attempt to explain what is going on.
        //
        // char *
        // dtoa(double v, int mode, int prec, int *expt, int *sign, char **end);
        //
        // The dtoa function returns a (char *) pointer of internally allocated
        // memory which is the double value converted into a decimal string.
        // The value should be free'd with freedtoa.
        //
        // In mode 2, which is needed for the form [-]d.ddde[+/-]dd, the
        // function returns a string with 'prec' significant digits.
        //
        // In this mode if XXXXX is the converted string, the number is equal to
        // 0.XXXXX * (10 ^ *expt).
        //
        // In mode 3, the function returns a string which is the decimal
        // representation going as far as 'prec' digits beyond the decimal
        // point, with trailing zeroes suppressed.
        //
        // In this mode *expt can be interpreted as the position of the
        // decimal point.
        //
        // Other arguments:
        // - signflag is set to true when the number is negative, 0 if not
        // - dtoaend is set to point to the end of the returned string.
        //
        // The calls to similar floating point conversion functions, ie.
        // __ldtoa(), __hdtoa(), and __hldtoa() are more or less analogous to
        // the call to __dtoa().
        //
        cp = dtoaresult =
          __dtoa(fparg.dbl, expchar ? 2 : 3, prec, &expt, &signflag, &dtoaend);
        if (dtoaresult == 0)
        {
          errno = ENOMEM;
          goto error;
        }
        //
        // If the number was bad, expt is set to 9999.
        //
        if (expt == 9999)
          expt = INT_MAX;
      }
fp_common:
      if (signflag)
        sign = '-';
      if (expt == INT_MAX)  // INF or NaN
      {
        if (*cp == 'N')
        {
          cp = (ch >= 'a') ? "nan" : "NAN";
          sign = '\0';
        }
        else
          cp = (ch >= 'a') ? "inf" : "INF";
        size = 3;
        flags &= ~ZEROPAD;
        break;
      }
      flags |= FPT;
      ndig = dtoaend - cp;
      if (ch == 'g' || ch == 'G')
      {
        if (expt > -4 && expt <= prec)
        {
          // Make %[gG] smell like %[fF]
          expchar = '\0';
          if (flags & ALT)
            prec -= expt;
          else
            prec = ndig - expt;
          if (prec < 0)
            prec = 0;
        }
        else
        {
          // Make %[gG] smell like %[eE], but trim trailing zeroes if no # flag.
          if (!(flags & ALT))
            prec = ndig;
        }
      }
      if (expchar)
      {
        expsize = exponent(expstr, expt - 1, expchar);
        size = expsize + prec;
        if (prec > 1 || flags & ALT)
          ++size;
      }
      else
      {
        // space for digits before decimal point
        if (expt > 0)
          size = expt;
        else  // "0"
          size = 1;
        // space for decimal pt and following digits
        if (prec || flags & ALT)
          size += prec + 1;
        lead = expt;
      }
      break;
#endif // FLOATING_POINT

    //
    // %n specifier
    //
    case 'n':
      if (flags & LLONGINT)
        WRITECOUNTAS(long long);
      else if (flags & LONGINT)
        WRITECOUNTAS(long);
      else if (flags & SHORTINT)
        WRITECOUNTAS(short);
      else if (flags & CHARINT)
        WRITECOUNTAS(signed char);
      else if (flags & PTRINT)
        WRITECOUNTAS(ptrdiff_t);
      else if (flags & SIZEINT)
        WRITECOUNTAS(ssize_t);
      else if (flags & MAXINT)
        WRITECOUNTAS(intmax_t);
      else
        WRITECOUNTAS(int);
      continue; // no output
    //
    // %O, %o specifiers
    //
    case 'O':
      flags |= LONGINT;
      // FALLTHROUGH
    case 'o':
      _umax = UARG();
      base = OCT;
      goto nosign;
    //
    // %p specifier
    //
    case 'p':
      //
      // ``The argument shall be a pointer to void.  The
      // value of the pointer is converted to a sequence
      // of printable characters, in an implementation-
      // defined manner.''
      //  -- ANSI X3J11
      //
      p = GETPTRARG();
      _umax = (uintmax_t) unwrap_pointer(&cinfo, options, (void *) p);
      base = HEX;
      xdigs = xdigs_lower;
      ox[1] = 'x';
      goto nosign;
    //
    // %s specifier
    //
    case 's':
      sign = '\0';
      //
      // Get the pointer_info structure associated with the next argument.
      //
      p = GETPTRARG();
      //
      // If the structure is NULL or not found in the whitelist, then the
      // current argument is not a string.
      //
      if (p == 0 || !is_in_whitelist(&cinfo, options, p))
      {
        cp = "(not a string)";
        size = 14;
        break;
      }
      //
      // Handle printing null pointers.
      //
      else if (p->ptr == 0)
      {
        cp = "(null)";
        size = 6;
        break;
      }
      //
      // Handle printing a string.
      //
      else
      {
        size_t sz;
        handle_s_directive(&cinfo, options, p, flags, &cp, &sz, &mbstr, prec);
        if (sz > INT_MAX)
          goto overflow;
        else
          size = (int) sz;
      }
      break;
    //
    // %U, %u specifiers
    //
    case 'U':
      flags |= LONGINT;
      // FALLTHROUGH
    case 'u':
      _umax = UARG();
      base = DEC;
      goto nosign;
    //
    // %X, %x specifiers
    //
    case 'X':
      xdigs = xdigs_upper;
      goto hex;
    case 'x':
      xdigs = xdigs_lower;
hex:
      _umax = UARG();
      base = HEX;
      // leading 0x/X only if non-zero
      if (flags & ALT && _umax != 0)
        ox[1] = ch;
nosign:
      // unsigned conversions
      sign = '\0';
number:
      //
      // ``... diouXx conversions ... if a precision is
      // specified, the 0 flag will be ignored.''
      //  -- ANSI X3J11
      //
      if ((dprec = prec) >= 0)
        flags &= ~ZEROPAD;
      //
      // ``The result of converting a zero value with an
      // explicit precision of zero is no characters.''
      //  -- ANSI X3J11
      //
      bp = buf + BUF;
      if (_umax != 0 || prec != 0)
      {
        //
        // Unsigned mod is hard, and unsigned mod
        // by a constant is easier than that by
        // a variable; hence this switch.
        //
        switch (base)
        {
          case OCT:
            do
            {
              *--bp = to_char(_umax & 7);
              _umax >>= 3;
            } while (_umax);
            // handle octal leading 0
            if (flags & ALT && *bp != '0')
              *--bp = '0';
            break;

          case DEC:
            // many numbers are 1 digit
            while (_umax >= 10)
            {
              *--bp = to_char(_umax % 10);
              _umax /= 10;
            }
            *--bp = to_char(_umax);
            break;

          case HEX:
            do
            {
              *--bp = xdigs[_umax & 15];
              _umax >>= 4;
            } while (_umax);
            break;

          default:
            cp = "bug in *printf: bad base";
            size = 24;
            goto skipsize;
          }
      }
      cp   = bp;
      size = buf + BUF - bp;
      if (size > BUF) // should never happen
        abort();
    skipsize:
      break;
    default:  // "%?" prints ?, unless ? is NUL
      //
      // %m specifier
      // (syslog() includes a %m flag which prints sterror(errno).)
      //
      if (ch == 'm' && options & USE_M_DIRECTIVE)
      {
        cp = (char *) strerror(errno);
        size = strlen(cp);
        break;
      }
      else if (ch == '\0')
        goto done;
      // pretend it was %c with argument ch
      buf[0] = ch;
      cp = buf;
      size = 1;
      sign = '\0';
      break;
    }

    //
    // All reasonable formats wind up here.  At this point, `cp'
    // points to a string which (if not flags&LADJUST) should be
    // padded out to `width' places.  If flags&ZEROPAD, it should
    // first be prefixed by any sign or other prefix; otherwise,
    // it should be blank padded before the prefix is emitted.
    // After any left-hand padding and prefixing, emit zeroes
    // required by a decimal %[diouxX] precision, then print the
    // string proper, then emit zeroes required by any leftover
    // floating precision; finally, if LADJUST, pad with blanks.
    //
    // Compute actual size, so we know how much to pad.
    // size excludes decimal prec; realsz includes it.
    //

    realsz = dprec > size ? dprec : size;
    if (sign)
      realsz++;
    if (ox[1])
      realsz+= 2;

    // right-adjusting blank padding
    if ((flags & (LADJUST|ZEROPAD)) == 0)
      PAD(width - realsz, blanks);

    // prefix
    if (sign)
      PRINT(&sign, 1);
    if (ox[1])
    {
      // ox[1] is either x, X, or \0
      ox[0] = '0';
      PRINT(ox, 2);
    }

    // right-adjusting zero padding
    if ((flags & (LADJUST|ZEROPAD)) == ZEROPAD)
      PAD(width - realsz, zeroes);

    // leading zeroes from decimal precision
    PAD(dprec - size, zeroes);

    // the string or number proper

#ifdef FLOATING_POINT
    if ((flags & FPT) == 0)
      PRINT(cp, size);
    else
    {
      // glue together f_p fragments
      if (!expchar)
      {
        // %[fF] or sufficiently short %[gG]
        if (expt <= 0)
        {
          PRINT(zeroes, 1);
          if (prec || flags & ALT)
            PRINT(decimal_point, 1);
          PAD(-expt, zeroes);
          // already handled initial 0's
          prec += expt;
        }
        else
        {
          PRINTANDPAD(cp, dtoaend, lead, zeroes);
          cp += lead;
          if (prec || flags & ALT)
            PRINT(decimal_point, 1);
        }
        PRINTANDPAD(cp, dtoaend, prec, zeroes);
      }
      else
      {
        // %[eE] or sufficiently long %[gG]
        if (prec > 1 || flags & ALT)
        {
          buf[0] = *cp++;
          buf[1] = *decimal_point;
          PRINT(buf, 2);
          PRINT(cp, ndig-1);
          PAD(prec - ndig, zeroes);
        } else
        {
          // XeYYY
          PRINT(cp, 1);
        }
        PRINT(expstr, expsize);
      }
    }
#else
    PRINT(cp, size);
#endif

    // left-adjusting padding (always blank)
    if (flags & LADJUST)
      PAD(width - realsz, blanks);

    // finally, adjust ret
    if (width < realsz)
      width = realsz;
    if (width > INT_MAX - ret)
      goto overflow;
    ret += width;

    FLUSH();  // copy out the I/O vectors
  }
done:
  FLUSH();
error:
  va_end(orgap);
  //if (__sferror(fp))
  //  ret = -1;
  goto finish;

overflow:
  errno = ENOMEM;
  ret = -1;

finish:

#ifdef FLOATING_POINT
  //
  // Free the buffers allocated for floating point number conversion.
  //
  if (dtoaresult)
    __freedtoa(dtoaresult);
#endif

  //
  // Free the buffers allocated for multibyte character string conversion.
  //
  if (mbstr != 0)
    free(mbstr);
  //
  // If the argument table had to be expanded, free it as well.
  //
  if (argtable != statargtable)
    free(argtable);
  //
  // Free all contents of ptr_infos, if necessary.
  //
  for (map<void *, pointer_info *>::iterator pos = ptr_infos.begin(),
       end = ptr_infos.end();
       pos != end;
       ++pos)
  {
    delete pos->second;
  }

  return ret;
}

//
// Type ids for argument type table.
//
#define T_UNUSED      0
#define T_SHORT       1
#define T_U_SHORT     2
#define TP_SHORT      3
#define T_INT         4
#define T_U_INT       5
#define TP_INT        6
#define T_LONG        7
#define T_U_LONG      8
#define TP_LONG       9
#define T_LLONG       10
#define T_U_LLONG     11
#define TP_LLONG      12
#define T_DOUBLE      13
#define T_LONG_DOUBLE 14
#define TP_CHAR       15
#define TP_VOID       16
#define T_PTRINT      17
#define TP_PTRINT     18
#define T_SIZEINT     19
#define T_SSIZEINT    20
#define TP_SSIZEINT   21
#define T_MAXINT      22
#define T_MAXUINT     23
#define TP_MAXINT     24
#define T_CHAR        25
#define T_U_CHAR      26
#define T_WINT        27

//
// find_arguments()
//
// Find all arguments when a positional parameter is encountered.  Returns a
// table, indexed by argument number, of pointers to each arguments.  The
// initial argument table should be an array of STATIC_ARG_TBL_SIZE entries.
//
// Inputs:
//  fmt0     - a pointer to the format string
//  ap       - the list of arguments
//  argtable - a pointer to the initial argument table
//  argtablesz - a pointer to a parameter which will be filled with the size
//               of the argument table
//  vargc    - the maximum number of arguments to access
//
static int
find_arguments(const char *fmt0,
               va_list ap,
               union arg **argtable,
               size_t *argtablesiz,
               unsigned vargc)
{
  const char *fmt;   // format string
  int ch;            // character from fmt
  int n, n2;         // handy integer (short term usage)
  const char *cp;    // handy char pointer (short term usage)
  int flags;         // flags as above
  unsigned char *typetable; // table of types
  unsigned char stattypetable[STATIC_ARG_TBL_SIZE];
  size_t tablesize;  // current size of type table
  unsigned tablemax; // largest used index in table
  unsigned nextarg;  // 1-based argument index
  unsigned i;        // handy unsigned integer
  int ret = 0;       // return value
  wchar_t wc;
  mbstate_t ps;

//
// Add an argument type to the table, expanding if necessary.
//
#define ADDTYPE(type)                                                          \
  ((nextarg >= tablesize) ?                                                    \
    grow_type_table(&typetable, &tablesize, 1 + nextarg) : 0),                 \
  ((nextarg > tablemax) ? tablemax = nextarg : 0),                             \
  (typetable != 0 ? typetable[nextarg++] = type : 0)

#define ADDSARG()                                                              \
      ((flags&MAXINT)   ? ADDTYPE(T_MAXINT)   :                                \
      ((flags&PTRINT)   ? ADDTYPE(T_PTRINT)   :                                \
      ((flags&SIZEINT)  ? ADDTYPE(T_SSIZEINT) :                                \
      ((flags&LLONGINT) ? ADDTYPE(T_LLONG)    :                                \
      ((flags&LONGINT)  ? ADDTYPE(T_LONG)     :                                \
      ((flags&SHORTINT) ? ADDTYPE(T_SHORT)    :                                \
      ((flags&CHARINT)  ? ADDTYPE(T_CHAR)     : ADDTYPE(T_INT))))))))

#define ADDUARG()                                                              \
      ((flags&MAXINT)   ? ADDTYPE(T_MAXUINT) :                                 \
      ((flags&PTRINT)   ? ADDTYPE(T_PTRINT)  :                                 \
      ((flags&SIZEINT)  ? ADDTYPE(T_SIZEINT) :                                 \
      ((flags&LLONGINT) ? ADDTYPE(T_U_LLONG) :                                 \
      ((flags&LONGINT)  ? ADDTYPE(T_U_LONG)  :                                 \
      ((flags&SHORTINT) ? ADDTYPE(T_U_SHORT) :                                 \
      ((flags&CHARINT)  ? ADDTYPE(T_U_CHAR)  : ADDTYPE(T_U_INT))))))))

//
// Add * arguments to the type array.
//
#define ADDASTER()                                                             \
  n2 = 0;                                                                      \
  cp = fmt;                                                                    \
  while (is_digit(*cp))                                                        \
  {                                                                            \
    APPEND_DIGIT(n2, *cp);                                                     \
    cp++;                                                                      \
  }                                                                            \
  if (*cp == '$')                                                              \
  {                                                                            \
    unsigned hold = nextarg;                                                   \
    nextarg = n2;                                                              \
    ADDTYPE(T_INT);                                                            \
    nextarg = hold;                                                            \
    fmt = ++cp;                                                                \
  }                                                                            \
  else                                                                         \
    ADDTYPE(T_INT);

  fmt = fmt0;
  typetable = stattypetable;
  tablesize = STATIC_ARG_TBL_SIZE;
  tablemax = 0;
  nextarg = 1;
  memset(typetable, T_UNUSED, STATIC_ARG_TBL_SIZE);
  memset(&ps, 0, sizeof(mbstate_t));

  //
  // Scan the format for conversions (`%' character).
  //
  for (;;)
  {
    cp = fmt;
    while ((n = mbrtowc(&wc, fmt, MB_CUR_MAX, &ps)) > 0)
    {
      fmt += n;
      if (wc == '%')
      {
        fmt--;
        break;
      }
    }
    if (n <= 0)
      goto done;
    fmt++;    // skip over '%'

    flags = 0;
  //
  // Scan the format directive for argument type information.
  //
rflag:    ch = *fmt++;
reswitch:
  switch (ch)
  {
    case ' ':
    case '#':
    case '\'':
      goto rflag;
    case '*':
      ADDASTER();
      goto rflag;
    case '-':
    case '+':
      goto rflag;
    case '.':
      if ((ch = *fmt++) == '*')
      {
        ADDASTER();
        goto rflag;
      }
      while (is_digit(ch))
        ch = *fmt++;
      goto reswitch;
    case '0':
      goto rflag;
    case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      n = 0;
      do
      {
        APPEND_DIGIT(n ,ch);
        ch = *fmt++;
      } while (is_digit(ch));
      if (ch == '$')
      {
        nextarg = n;
        goto rflag;
      }
      goto reswitch;

#ifdef FLOATING_POINT
    case 'L':
      flags |= LONGDBL;
      goto rflag;
#endif

    case 'h':
      if (*fmt == 'h')
      {
        fmt++;
        flags |= CHARINT;
      } else
        flags |= SHORTINT;
      goto rflag;
    case 'l':
      if (*fmt == 'l')
      {
        fmt++;
        flags |= LLONGINT;
      }
      else
        flags |= LONGINT;
      goto rflag;
    case 'q':
      flags |= LLONGINT;
      goto rflag;
    case 't':
      flags |= PTRINT;
      goto rflag;
    case 'z':
      flags |= SIZEINT;
      goto rflag;
    case 'c':
      //
      // Handle wide character arguments correctly.
      //
      if (flags & LONGINT)
        ADDTYPE(T_WINT);
      else
        ADDTYPE(T_INT);
      break;
    case 'D':
      flags |= LONGINT;
      // FALLTHROUGH
    case 'd':
    case 'i':
      ADDSARG();
      break;

#ifdef FLOATING_POINT
    case 'a':
    case 'A':
    case 'e':
    case 'E':
    case 'f':
    case 'F':
    case 'g':
    case 'G':
      if (flags & LONGDBL)
        ADDTYPE(T_LONG_DOUBLE);
      else
        ADDTYPE(T_DOUBLE);
      break;
#endif // FLOATING_POINT

    case 'n':
      if (flags & LLONGINT)
        ADDTYPE(TP_LLONG);
      else if (flags & LONGINT)
        ADDTYPE(TP_LONG);
      else if (flags & SHORTINT)
        ADDTYPE(TP_SHORT);
      else if (flags & PTRINT)
        ADDTYPE(TP_PTRINT);
      else if (flags & SIZEINT)
        ADDTYPE(TP_SSIZEINT);
      else if (flags & MAXINT)
        ADDTYPE(TP_MAXINT);
      else
        ADDTYPE(TP_INT);
      continue; // no output
    case 'O':
      flags |= LONGINT;
      // FALLTHROUGH
    case 'o':
      ADDUARG();
      break;
    case 'p':
      ADDTYPE(TP_VOID);
      break;
    case 's':
      ADDTYPE(TP_CHAR);
      break;
    case 'U':
      flags |= LONGINT;
      // FALLTHROUGH
    case 'u':
    case 'X':
    case 'x':
      ADDUARG();
      break;
    default:  // "%?" prints ?, unless ? is NUL
      if (ch == '\0')
        goto done;
      break;
    }
  }
done:
  //
  // A NULL type table indicates a malloc() failure.
  //
  if (typetable == 0)
  {
    ret = -1;
    goto finish;
  }
  //
  // Allocate the argument table, if necessary.
  //
  if (tablemax >= STATIC_ARG_TBL_SIZE)
  {
    *argtablesiz = sizeof(union arg) * (tablemax + 1);
    *argtable = (union arg *) malloc(*argtablesiz);
    if (argtable == 0)
    {
      ret = -1;
      goto finish;
    }
  }
  //
  // Fill the argument table based on the entries of the type table.
  //
  for (i = 1; i <= min(vargc, tablemax); i++)
  {
    switch (typetable[i])
    {
      case T_UNUSED:
      case T_CHAR:
      case T_U_CHAR:
      case T_SHORT:
      case T_U_SHORT:
      case T_INT:
        (*argtable)[i].intarg  = va_arg(ap, int);
        break;
      case T_WINT:
        (*argtable)[i].wintarg = va_arg(ap, wint_t);
        break;
      case TP_SHORT:
      case TP_INT:
      case TP_LONG:
      case TP_LLONG:
      case TP_CHAR:
      case TP_VOID:
      case TP_PTRINT:
      case TP_SSIZEINT:
      case TP_MAXINT:
        (*argtable)[i].pvoidarg = va_arg(ap, void *);
        break;
      case T_U_INT:
        (*argtable)[i].uintarg = va_arg(ap, unsigned int);
        break;
      case T_LONG:
        (*argtable)[i].longarg = va_arg(ap, long);
        break;
      case T_U_LONG:
        (*argtable)[i].ulongarg = va_arg(ap, unsigned long);
        break;
      case T_LLONG:
        (*argtable)[i].longlongarg = va_arg(ap, long long);
        break;
      case T_U_LLONG:
        (*argtable)[i].ulonglongarg = va_arg(ap, unsigned long long);
        break;

#ifdef FLOATING_POINT
      case T_DOUBLE:
        (*argtable)[i].doublearg = va_arg(ap, double);
        break;
      case T_LONG_DOUBLE:
        (*argtable)[i].longdoublearg = va_arg(ap, long double);
        break;
#endif

      case T_PTRINT:
        (*argtable)[i].ptrdiffarg = va_arg(ap, ptrdiff_t);
        break;
      case T_SIZEINT:
        (*argtable)[i].sizearg = va_arg(ap, size_t);
        break;
      case T_SSIZEINT:
        (*argtable)[i].ssizearg = va_arg(ap, ssize_t);
        break;
    }
  }

overflow:
  errno = ENOMEM;
  ret   = -1;

finish:
  // Free the type table, if necessary.
  if (typetable != stattypetable)
    free(typetable);
  return ret;
}

//
// grow_type_table()
//
// Expand the internal type table to be at least of size minsz.
//
// Inputs:
//  typetable - a pointer to the current type table
//  tablesize - a pointer to a value holding the current table size
//  minsz     - the minimum new size of the table
//
// Returns:
//  This function returns 0 on success and -1 on failure.
//
static inline int
grow_type_table(unsigned char **typetable, size_t *tablesize, size_t minsz)
{
  //
  // Allocate the new table on the heap.
  //
  const size_t newsize = max(*tablesize * 2, minsz);
  if (*tablesize == STATIC_ARG_TBL_SIZE)
    *typetable = (unsigned char *) malloc(newsize);
  else
    *typetable = (unsigned char *) realloc(*typetable, newsize);
  if (*typetable == 0)
    return -1;
  //
  // Fill the rest of the table with empty entries.
  //
  memset(*typetable + *tablesize, T_UNUSED, (newsize - *tablesize));
  *tablesize = newsize;
  return 0;
}

#ifdef FLOATING_POINT
//
// Convert the exponent into a string, of the form fNNN, where f is the format
// directive and NNN a digit string.
//
// Inputs:
//  p0    - the destination buffer
//  exp   - the value to convert
//  fmtch - the associated format directive
//
// Returns:
//  This function returns the length of the converted string.
//
static int
exponent(char *p0, int exp, int fmtch)
{
  char *p, *t;
  char expbuf[MAXEXPDIG];

  p = p0;
  *p++ = fmtch;
  if (exp < 0)
  {
    exp = -exp;
    *p++ = '-';
  }
  else
    *p++ = '+';
  t = expbuf + MAXEXPDIG;
  if (exp > 9)
  {
    do *--t = to_char(exp % 10); while ((exp /= 10) > 9);
    *--t = to_char(exp);
    for (; t < expbuf + MAXEXPDIG; *p++ = *t++)
      /* nothing */;
  }
  else
  {
    //
    // Exponents for decimal floating point conversions
    // (%[eEgG]) must be at least two characters long,
    // whereas exponents for hexadecimal conversions can
    // be only one character long.
    //
    if (fmtch == 'e' || fmtch == 'E')
      *p++ = '0';
    *p++ = to_char(exp);
  }
  return (p - p0);
}
#endif // FLOATING_POINT

