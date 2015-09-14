//===- ScanfSupport.cpp -  Secure scanf() replacement          ------------===//
// 
//                       The SAFECode Compiler Project
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file implements a secure runtime replacement for scanf() and similar
// functions.
//
//===----------------------------------------------------------------------===//

//
// This code is derived from MINIX's doscan.c; original license follows:
//
// /cvsup/minix/src/lib/stdio/doscan.c,v 1.1.1.1 2005/04/21 14:56:35 beng Exp $
//
// Copyright (c) 1987,1997,2001 Prentice Hall
// All rights reserved.
//
// Redistribution and use of the MINIX operating system in source and
// binary forms, with or without modification, are permitted provided
// that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright
//      notice, this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above
//      copyright notice, this list of conditions and the following
//      disclaimer in the documentation and/or other materials provided
//      with the distribution.
//
//    * Neither the name of Prentice Hall nor the names of the software
//      authors or contributors may be used to endorse or promote
//      products derived from this software without specific prior
//      written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS, AUTHORS, AND
// CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL PRENTICE HALL OR ANY AUTHORS OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
// BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
// EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#include "safecode/Config/config.h"
#include "FormatStrings.h"

#include <ctype.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <wchar.h>

#ifdef __x86_64__
#define set_pointer(flags)  (flags |= FL_LONG)
#else
#define set_pointer(flags)        // nothing
#endif

// Maximum allowable size for an input number.
#define NUMLEN    512
#define NR_CHARS  256

//
// Flags describing how to process the input
//
#define FL_CHAR       0x0001    // hh length modifier
#define FL_SHORT      0x0002    // h  length modifier
#define FL_LLONG      0x0004    // ll length modifier
#define FL_LONG       0x0008    // l  length modifier
#define FL_LONGDOUBLE 0x0010    // L  length modifier
#define FL_INTMAX     0x0020    // j  length modifier
#define FL_SIZET      0x0040    // z  length modifier
#define FL_PTRDIFF    0x0080    // t  length modifier
#define FL_NOASSIGN   0x0100    // do not assign (* flag)
#define FL_WIDTHSPEC  0x0200    // field width specified

//
// _getc()
//
// Get the next character from the input.
// Returns EOF on reading error or end of file/string.
//
static inline int
_getc(input_parameter *i)
{
  //
  // Get the next character from the string.
  //
  if (i->input_kind == input_parameter::INPUT_FROM_STRING)
  {
    const char *string = i->input.string.string;
    size_t &pos = i->input.string.pos;
    if (string[pos] == '\0')
      return EOF;
    else
      return string[pos++];
  }
  //
  // Get the next character from the stream.
  //
  else // i->InputKind == input_parameter::INPUT_FROM_STREAM
  {
    FILE *file = i->input.stream.stream;
    char &lastch = i->input.stream.lastch;
    int ch;
    //
    // Call fgetc_unlocked() for performance because the given input stream
    // should already be locked by the thread.
    //
    // Use fgetc() on platforms without fgetc_unlocked().
    //
#ifndef HAVE_FGETC_UNLOCKED
#define fgetc_unlocked fgetc
#endif
    if ((ch = fgetc_unlocked(file)) == EOF)
      return EOF;
    else
    {
      // Save the character we got in case it is pushed back via _ungetc().
      lastch = ch;
      return ch;
    }
  }
}

//
// _ungetc()
//
// 'Push back' the last character that was read from the input source.
// This function assumes at least one character has been read via _getc().
// This function should be called at most once between calls to _getc(),
// so that at most one character is pushed back at any given time.
//
static inline void
_ungetc(input_parameter *i)
{
  if (i->input_kind == input_parameter::INPUT_FROM_STRING)
    //
    // 'Push back' the string by just decrementing the position.
    //
    i->input.string.pos--;
  else if (i->input_kind == input_parameter::INPUT_FROM_STREAM)
  {
    const char lastch = i->input.stream.lastch;
    //
    // Use ungetc() to push the last character back into the stream.
    // See the note over internal_scanf() about the portability of this
    // operation.
    //
    ungetc(lastch, i->input.stream.stream);
  }
}

//
// input_failure()
//
// Check if the parameter has had an input failure.
// This is defined as EOF or a read error, according to the standard.
// For strings an input error is the same as the end of the string.
//
// Returns: True if the parameter is said to have an input failure, false
// otherwise.
//
static inline bool
input_failure(input_parameter *i)
{
  if (i->input_kind == input_parameter::INPUT_FROM_STRING)
    return i->input.string.string[i->input.string.pos] == 0;
  else // i->InputKind == input_parameter::INPUT_FROM_STREAM
  {
    FILE *f = i->input.stream.stream;
    return ferror(f) || feof(f);
  }
}

//
// o_collect()
//
// Collect a number of characters which constitite an ordinal number.
// When the type is 'i', the base can be 8, 10, or 16, depending on the
// first 1 or 2 characters. This means that the base must be adjusted
// according to the format of the number. At the end of the function, base
// is then set to 0, so strtol() will get the right argument.
//
// Inputs:
//
//  c       - the first character to read as an input item
//  stream  - the input_parameter object which contains the rest of the
//            characters to be read as input items
//  inp_buf - the buffer into which the number should be written
//  type    - the type of the specifier associated with this conversion, one of
//            'i', 'p', 'x', 'X', 'd', 'o', or 'b'
//  width   - the maximum field width
//  basep   - A pointer to an integer. This value is written into with the
//            numerical base of the value in the buffer, suitable for a call to
//            strtol() or other function, as determined by this function.
//
// This function returns NULL if the input buffer was not filled with a valid
// integer that could be converted; and otherwise if the input buffer contains
// the digits of a valid integer, the function returns the last nonnul position
// of the buffer that was written.
//
// On success, the buffer is suited for a call to strtol() or other integer
// conversion function, with the base given in *basep.
// 
static char *
o_collect(int c,
          input_parameter *stream,
          char *inp_buf,
          char type,
          unsigned int width,
          int *basep)
{
  char *bufp = inp_buf;
  int base = 0;

  switch (type)
  {
    case 'i': // i means octal, decimal or hexadecimal
    case 'p':
    case 'x':
    case 'X': base = 16;  break;
    case 'd':
    case 'u': base = 10;  break;
    case 'o': base = 8; break;
    case 'b': base = 2; break;
  }
  //
  // Process any initial +/- sign.
  //
  if (c == '-' || c == '+')
  {
    *bufp++ = c;
    if (--width)
      c = _getc(stream);
    else
      return 0;  // An initial [+-] is not a valid number.
  }
  //
  // Determine whether an initial '0' means to process the number in
  // hexadecimal or octal, if we are given a choice between the two.
  //
  if (width && c == '0' && base == 16)
  {
    *bufp++ = c;
    if (--width)
      c = _getc(stream);
    if (c != 'x' && c != 'X')
    {
      if (type == 'i') base = 8;
    }
    else if (width)
    {
      *bufp++ = c;
      if (--width)
        c = _getc(stream);
    }
    else
      return 0; // Don't accept only an initial [+-]?0[xX] as a valid number.
  }
  else if (type == 'i')
    base = 10;
  //
  // Read as many digits as we can.
  //
  while (width)
  {
    if (    ((base == 10) && isdigit(c)             )
         || ((base == 16) && isxdigit(c)            )
         || ((base ==  8) && isdigit(c) && (c < '8'))
         || ((base ==  2) && isdigit(c) && (c < '2')))
    {
      *bufp++ = c;
      if (--width)
        c = _getc(stream);
    }
    else
      break;
  }
  //
  // Push back any extra read characters that aren't part of an integer.
  //
  if (width && c != EOF)
    _ungetc(stream);
  if (type == 'i')
    base = 0;
  *basep = base;
  *bufp = '\0';
  return bufp == inp_buf ? 0 : bufp - 1;
}

#ifdef FLOATING_POINT

#include "ScanfTables.h"

//
// f_collect()
//
// Read the longest valid floating point number prefix from the input buffer.
// Upon encountering an error, the function returns and leaves the character
// to have caused the error in the input stream.
//
// Inputs:
//  c       - the first character to read
//  stream  - the parameter from which to get the rest of the characters
//  inp_buf - the buffer into which to write the number
//  width   - the maximum number of characters to read
//
// Returns:
//  On error, the function returns NULL. On success, it returns a pointer to the
//  last non-nul position in the input buffer.
//
char *
f_collect(int c, input_parameter *stream, char *inp_buf, unsigned int width)
{
  int   state = 1;   // The start state from the scanner
  char *buf = inp_buf;
  int   ch;
  int   accept;

  ch = c;
  //
  // This loop matches the input with the transition table.
  //
  while (width && state > 0)
  {
    //
    // Handle an 8 bit character or EOF character by breaking immediately.
    //
    if (ch == EOF || ch > 127)
      break;
    state = yy_nxt[state][ch];
    //
    // Advance to the next state and save the current character, if valid.
    //
    if (state > 0)
      *buf++ = ch;
    //
    // Get the next character.
    //
    if (--width)
      ch = _getc(stream);
  }
  //
  // Push back the next character if it was a valid character not part of the
  // input sequence.
  //
  if (width > 0 && ch != EOF)
    _ungetc(stream);
  //
  // Get information about the next action of the scanner.
  //
  accept = yy_accept[state < 0 ? -state : state];
  //
  // A value of 0 for accept indicates failure/that the scanner should revert
  // to the previous accepting state. Since this is not possible without
  // pushing back more than one character, we should just fail.
  //
  // A value of DEFAULT_RULE indicates to the scanner to echo the output
  // since it is unmatched. This also indicates failure.
  //
#define DEFAULT_RULE 5
  if (accept == 0 || accept == DEFAULT_RULE)
    return 0;
  else if (buf == inp_buf)
    return 0;
  else
  {
    *buf = '\0';
    return buf - 1;
  }
}
#endif // FLOATING_POINT

//
// eat_whitespace()
//
// Read all initial whitespace from the input stream.
//
// Inputs:
//  stream     - the input stream
//  count      - a reference to a counter of the number of characters read
//
// Returns:
//  This function returns the first non whitespace character encountered in the
//  input stream (which could be EOF).
//
static inline int
eat_whitespace(input_parameter *stream, int &count)
{
  int ch;
  do
  {
    ch = _getc(stream);
    count++;
  } while (isspace(ch));
  return ch;
}

//
// This is a bitvector which specifies a set of characters to match.
//
typedef struct
{
  enum { FUNCTION, TABLE } kind;
  union
  {
    uint64_t t[4];
    int (*f)(int);
  } s;
} scanset_t;

const scanset_t all_chars =
{
  scanset_t::TABLE,
  { { 0xfffffffffffffffful,
      0xfffffffffffffffful,
      0xfffffffffffffffful,
      0xfffffffffffffffful } }
};

// Clear the scanset.
#define clear_scanset(set) \
  for (int i = 0; i < 4; i++) (set)->s.t[i] = 0ul
// Add a character to the scanset.
#define add_to_scanset(set, c) \
  (set)->s.t[((unsigned char) c) >> 6] |= ((uint64_t)1) << (c & 0x3f)
// Take the complement of the scanset.
#define invert_scanset(set) \
  for (int i = 0; i < 4; i++) (set)->s.t[i] = ~(set)->s.t[i]
// Query if a character is in the scanset.
#define is_in_scanset(set, c)                                                  \
  (                                                                            \
    (set)->kind == scanset_t::TABLE ?                                          \
    (set)->s.t[((unsigned char) c) >> 6] & (((uint64_t) 1) << (c & 0x3f)) :    \
    (set)->s.f(c)                                                              \
  )

//
// read_scanset()
//
// Read the %[...]-style directive embedded in the given format string, and
// construct the resuling scanset into *scanset.
//
// Inputs:
//  format  - the format string, assumed to be pointing to the next character
//            after the sequence "%["
//  scanset - the scanset to fill with the characters in the sequence
//
// Returns:
//  The function returns a pointer to the first subsequent character in the
//  format string that is considered not part of the scanset. This character
//  will be ']' in a valid directive, or '\0' if the directive is somehow
//  malformed.
//
static inline const char *
read_scanset(const char *format, scanset_t *scanset)
{
  int reverse;
  const char *start = format;
  //
  // Determine if we take the complement of the scanset.
  //
  if (*++format == '^')
  {
    reverse = 1;
    format++;
  }
  else
    reverse = 0;
  //
  // Clear the scanset first.
  //
  clear_scanset(scanset);
  //
  // ']' appearing as the first character in the set does not close the
  // directive, but rather adds ']' to the scanset.
  //
  if (*format == ']')
  {
    add_to_scanset(scanset, ']');
    format++;
  }
  //
  // Parse the rest of the directive.
  //
  while (*format != '\0' && *format != ']')
  {
    //
    // Add a character range to the scanset...
    //
    if (*format == '-')
    {
      format++;
      //
      // ...unless we're in a boundary condition, in which case just add '-'.
      //
      if (*format == ']' || &format[-2] == start ||
          (&format[-2] == &start[1] && start[1] == '^'))
      {
        add_to_scanset(scanset, '-');
      }
      else if (*format >= format[-2])
      {
        int c;
        for (c = format[-2] + 1; c <= *format; c++)
          add_to_scanset(scanset, c);
        format++;
      }
    }
    else
    {
      add_to_scanset(scanset, *format);
      format++;
    }
  }
  //
  // Take the complement, if necessary.
  //
  if (reverse)
    invert_scanset(scanset);
  return format;
}

//
// isnspace()
//
// Used by the %s directive for reading input.
//
int isnspace(int c)
{
  return !isspace(c);
}

//
// match_string()
//
// Read a string of characters that belong in the given scanset, up to size
// width. If 'dowrite' is true, write the string (with a nul terminator) into
// the buffer associated with the pointer_info argument. Report if there are
// any memory safety errors due to writing into the buffer.
//
// Inputs:
//
//  ci      - a pointer to the call_info structure for this function call
//  p       - a pointer to the pointer_info structure that contains the buffer
//            to write into
//  flags   - the set of flags associated with the current conversion
//  c       - the first character to read
//  stream  - the input stream which contains the rest of the characters
//  width   - the maximum number of characters to read
//  dowrite - a boolean, if true, the string is written into a buffer 
//  termin  - a boolean, if true, the buffer is terminated
//  nrchars - a reference to a relevant counter of the number of characters
//            read
//  set     - the set of permissible characters in the string
//
// Returns:
//  This function returns the number of characters that it has read.
//  Reading 0 characters indicates a conversion error.
//
//  If dowrite is true and (flags&FL_LONG) is true, then the buffer is filled
//  with wide characters converted from the multibyte input stream. Otherwise,
//  the buffer is filled with the same characters that composed the input, if
//  dowrite is true.
//
static inline size_t
match_string(call_info    *ci,
             pointer_info *p,
             int          flags,
             int          c,
             input_parameter *stream,
             size_t       width,
             const bool   dowrite,
             const bool   termin,
             int          &nrchars,
             const scanset_t *set)
{
  size_t maxwrite   = SIZE_MAX;
  size_t writecount = 0;
  size_t readcount  = 1; // We've "read" c already.
  char   *buf       = 0;
  char    mbbuf[MB_LEN_MAX];
  size_t  mbbufpos   = 0;
  wchar_t wc;
  size_t wclen;
  const bool wcs    = flags & FL_LONG;
  mbstate_t ps;
  if (wcs)
    memset(&ps, 0, sizeof(mbstate_t));
  //
  // Read the input string.
  //
  while (width > 0 && c != EOF && is_in_scanset(set, c))
  {
    //
    // If after reading a character, the buffer is ready for writing, prepare
    // it for writing.
    //
    if (readcount++ == 1 && dowrite)
    {
      //
      // Get the output buffer.
      //
      buf = (char *) p;
      if (p == NULL)
      {
        cerr << "Writing into a NULL object!" << endl;
        c_library_error(ci, "scanf");
      }
      else if (is_in_whitelist(ci, p) == false)
      {
        cerr << "Writing into a non-pointer!" << endl;
        c_library_error(ci, "scanf");
      }
      else
      {
        find_object(ci, p);
        if (p->flags & HAVEBOUNDS)
          maxwrite = (size_t) 1 + (char *)p->bounds[1] - (char *) p->ptr;
        buf = (char *) p->ptr;
        if (buf == 0)
        {
          cerr << "Writing into a NULL object!" << endl;
          c_library_error(ci, "scanf");
        }
      }
    }
    if (dowrite)
    {
      if (!wcs)
      {
        //
        // Write directly into the output buffer.
        //
        //
        // Check if this write will go out of bounds. Only report this the first
        // time that it occurs.
        //
        if ((++writecount - maxwrite) == 1)
        {
          cerr << "Writing out of bounds!" << endl;
          write_out_of_bounds_error(ci, p, maxwrite, writecount);
        }
        *buf++ = c;
      }
      else
      {
        //
        // Read the next multibyte character incrementally. Write it into the
        // output buffer once we've encountered a valid character.
        //
        mbbuf[mbbufpos++] = (char) c;
        //
        // Attempt a multibyte to wide character conversion.
        //
        wclen = mbrtowc(&wc, &mbbuf[0], mbbufpos, &ps);
        if (wclen == (size_t) -1) // Conversion error
          break;
        else if (wclen == (size_t) -2)
          ; // The buffer might be holding the prefix of a valid
            // multibyte character sequence, so try reading more.
        else
        {
          writecount += sizeof(wchar_t);

          //
          // Check if the write will go out of bounds. Only report this the
          // first time that it occurs.
          //
          if (writecount - maxwrite > 0 &&
	      (writecount - maxwrite) <= sizeof(wchar_t))
          {
            cerr << "Writing out of bounds!" << endl;
            write_out_of_bounds_error(ci, p, maxwrite, writecount);
          }
          ((wchar_t *)buf)[(writecount/sizeof(wchar_t)) - 1] = wc;
          //
          // Reset the input buffer.
          //
          mbbufpos = 0;
        }
      }
    }
    if (--width > 0)
    {
      c = _getc(stream);
      nrchars++;
    }
  }
  //
  // Terminate the string with the appropriate nul terminator, if necessary.
  //
  if (termin && dowrite && writecount > 0)
  {
    if (!wcs)
    {
      if ((++writecount - maxwrite) == 1)
      {
        cerr << "Writing out of bounds!" << endl;
        write_out_of_bounds_error(ci, p, maxwrite, writecount);
      }
      *buf = '\0';
    }
    else
    {
      writecount += sizeof(wchar_t);
      if (writecount - maxwrite > 0 &&
        (writecount - maxwrite) <= sizeof(wchar_t))
      {
        cerr << "Writing out of bounds!" << endl;
        write_out_of_bounds_error(ci, p, maxwrite, writecount);
      }
      ((wchar_t *)buf)[(writecount/sizeof(wchar_t)) - 1] = L'\0';
    }
  }
  //
  // Push back any character that was read and not in the scanset.
  //
  if (width > 0 && c != EOF)
  {
    _ungetc(stream);
    readcount--;
    nrchars--;
  }
  //
  // Don't count EOF.
  //
  else if (c == EOF)
  {
    readcount--;
    nrchars--;
  }
  return readcount;
}

//
// SAFEWRITE()
//
// A macro that attempts to securely write a value into the next parameter.
//
// Inputs:
//  ci       - pointer to the call_info structure
//  ap       - pointer to the va_list
//  arg      - the variable argument number to be accessed
//  item     - the item to write
//  type     - the type to write the item as
//
#define SAFEWRITE(ci, ap, arg, item, type)                                     \
  do                                                                           \
  {                                                                            \
    pointer_info *p;                                                           \
    void *dest;                                                                \
    varg_check(ci, arg++);                                                     \
    p = va_arg(ap, pointer_info *);                                            \
    write_check(ci, p, sizeof(type));                                          \
    dest = unwrap_pointer(ci, p);                                              \
   /* if (dest != 0) */ *(type *) dest = (type) item;                          \
  } while (0)

//
// internal_scanf()
//
// This is the main logic for the secured scanf() family of functions.
//
//
// IMPLEMENTATION NOTES
//  - This function uses ungetc() to push back characters into a stream.
//    This is an error because at most one character can be pushed back
//    portably, but an ungetc() call is allowed to follow a call to a scan
//    function without any intervening I/O. Hence if this function calls
//    ungetc() and then the caller does so again on the same stream, the
//    result might fail.
//
//    However, glibc appears to have support for consecutive 2 character
//    pushback, so this doesn't fail on glibc.
//
//    Mac OS X's libc also has support for 2 character pushback.
//
//  - A nonstandard %b specifier is supported, for reading binary integers.
//  - No support for positional arguments (%n$-style directives) (yet ?).
//  - The maximum supported width of a numerical constant in the input is 512
//    bytes.
//

int
internal_scanf(input_parameter &i, call_info &c, const char *fmt, va_list ap)
{
  int   done = 0;             // number of items converted
  int   nrchars = 0;          // number of characters read
  int   base;                 // conversion base
  uintmax_t val;              // an integer value
  char  *str;                 // temporary pointer
  char  *tmp_string;          // ditto
  unsigned  width = 0;        // width of field
  int   flags;                // some flags
  int   kind;
  int  ic = EOF;              // the input character
  char inp_buf[NUMLEN + 1];   // buffer to hold numerical inputs

#ifdef FLOATING_POINT
  long double ld_val;
#endif

  const char *format = fmt;
  input_parameter *stream = &i;

  str = 0; // Suppress g++ complaints about unitialized variables.
  pointer_info *p = 0;

  // Return immediately for an empty format string.
  if (*format == '\0')
    return 0;

  mbstate_t ps;
  size_t    len;
  const char *mb_pos;
  memset(&ps, 0, sizeof(ps));

  call_info *ci = &c;
  unsigned int arg   = 1;

  bool   wr;
  size_t sz;
  scanset_t nonws;
  nonws.kind = scanset_t::FUNCTION;
  nonws.s.f  = isnspace;
  scanset_t scanset;
  scanset.kind = scanset_t::TABLE;

//
// Version of SAFEWRITE() relevant to this function.
//
#define _SAFEWRITE(item, type) SAFEWRITE(ci, ap, arg, item, type)

  //
  // The main loop that processes the format string.
  // At the end of the loop, the count of assignments is updated and the format
  // string is incremented one position.
  //
  while (1)
  {
    //
    // Whitespace in the format string indicates to match all whitespace in the
    // input stream.
    //
    if (isspace(*format))
    {
      while (isspace(*format))
        format++; // Skip whitespace in the format string.
      ic = eat_whitespace(stream, nrchars);
      if (ic != EOF)
        _ungetc(stream);
    }
    if (*format == '\0')
      break; // End of format
    //
    // Match a multibyte character from the input.
    //
    if (*format != '%')
    {
      len = mbrtowc(NULL, format, MB_CUR_MAX, &ps);
      mb_pos = format;
      format += len;
      while (mb_pos != format)
      {
        ic = _getc(stream);
        if (ic != *mb_pos)
          break;
        else
        {
          nrchars++;
          mb_pos++;
        }
      }
      if (mb_pos != format && ic != *mb_pos)
      {
        //
        // A directive that is an ordinary multibyte character is executed
        // by reading the next characters of the stream. If any of those
        // characters differ from the ones composing the directive, the
        // directive fails and the differing and subsequent characters
        // remain unread.
        //
        // - C99 Standard
        //
        if (ic != EOF)
        {
          _ungetc(stream);
          goto match_failure;
        }
        else
          goto failure;
      }
      continue;
    }
    format++; // We've read '%'; start processing a directive.
    flags = 0;
    //
    // The '%' specifier
    //
    if (*format == '%')
    {
      ic = eat_whitespace(stream, nrchars);
      if (ic == '%')
      {
        format++;
        continue;
      }
      else
      {
        _ungetc(stream);
        goto failure;
      }
    }
    //
    // '*' flag: Suppress assignment.
    //
    if (*format == '*')
    {
      format++;
      flags |= FL_NOASSIGN;
    }
    //
    // Get the field width, if there is any.
    //
    if (isdigit (*format))
    {
      flags |= FL_WIDTHSPEC;
      for (width = 0; isdigit (*format);)
        width = width * 10 + *format++ - '0';
    }
    //
    // Process length modifiers.
    //
    switch (*format)
    {
      case 'h':
        if (*++format == 'h')
        {
          format++;
          flags |= FL_CHAR;
        }
        else
          flags |= FL_SHORT;
        break;
      case 'l':
        if (*++format == 'l')
        {
          format++;
          flags |= FL_LLONG;
        }
        else
          flags |= FL_LONG;
        break;
      case 'j':
        format++;
        flags |= FL_INTMAX;
        break;
      case 'z':
        format++;
        flags |= FL_SIZET;
        break;
      case 't':
        format++;
        flags |= FL_PTRDIFF;
        break;
      case 'L':
        format++;
        flags |= FL_LONGDOUBLE;
        break;
    }
    //
    // Read the actual specifier.
    //
    kind = *format;
    //
    // Eat any initial whitespace for specifiers that allow it.
    //
    if ((kind != 'c') && (kind != '[') && (kind != 'n'))
    {
      ic = eat_whitespace(stream, nrchars);
      if (ic == EOF)
        goto failure;
    }
    //
    // Get the initial character of the input, for non-%n specifiers.
    //
    else if (kind != 'n')
    {
      ic = _getc(stream);
      if (ic == EOF)
        goto failure;
      nrchars++;
    }
    //
    // Process the format specifier.
    //
    switch (kind)
    {
      default:
        // not recognized, like %q
        goto failure;
      //
      // %n specifier
      //
      case 'n':
        if (!(flags & FL_NOASSIGN))
        {
          if (flags & FL_CHAR)
            _SAFEWRITE(nrchars, char);
          else if (flags & FL_SHORT)
            _SAFEWRITE(nrchars, short);
          else if (flags & FL_LONG)
            _SAFEWRITE(nrchars, long);
          else if (flags & FL_LLONG)
            _SAFEWRITE(nrchars, long long);
          else if (flags & FL_INTMAX)
            _SAFEWRITE(nrchars, intmax_t);
          else if (flags & FL_SIZET)
            _SAFEWRITE(nrchars, size_t);
          else if (flags & FL_PTRDIFF)
            _SAFEWRITE(nrchars, ptrdiff_t);
          else
            _SAFEWRITE(nrchars, int);
        }
        break;
      //
      // %p specifier
      //
      case 'p':
        // Set any additional flags regarding pointer representation size.
        set_pointer(flags);
        // fallthrough
      //
      // Integral specifiers: %b, %d, %i, %o, %u, %x, %X
      //
      case 'b':   // binary
      case 'd':   // decimal
      case 'i':   // general integer
      case 'o':   // octal
      case 'u':   // unsigned
      case 'x':   // hexadecimal
      case 'X':   // ditto
        // Don't read more than NUMLEN bytes.
        if (!(flags & FL_WIDTHSPEC) || width > NUMLEN)
          width = NUMLEN;
        if (width == 0)
          goto match_failure;

        str = o_collect(ic, stream, inp_buf, kind, width, &base);

        if (str == 0)
          goto failure;

        //
        // Although the length of the number is str-inp_buf+1
        // we don't add the 1 since we counted it already.
        // 
        nrchars += str - inp_buf;

        if (!(flags & FL_NOASSIGN))
        {
          if (kind == 'd' || kind == 'i')
            val = strtoimax(inp_buf, &tmp_string, base);
          else
            val = strtoumax(inp_buf, &tmp_string, base);
          if (flags & FL_CHAR)
            _SAFEWRITE(val, unsigned char);
          else if (flags & FL_SHORT)
            _SAFEWRITE(val, unsigned short);
          else if (flags & FL_LONG)
            _SAFEWRITE(val, unsigned long);
          else if (flags & FL_LLONG)
            _SAFEWRITE(val, unsigned long long);
          else if (flags & FL_INTMAX)
            _SAFEWRITE(val, uintmax_t);
          else if (flags & FL_SIZET)
            _SAFEWRITE(val, size_t);
          else if (flags & FL_PTRDIFF)
            _SAFEWRITE(val, ptrdiff_t);
          else
            _SAFEWRITE(val, unsigned);
        }
        break;
      //
      // %c specifier
      //
      case 'c':
        //
        // No specified width means to read a single character.
        //
        if (!(flags & FL_WIDTHSPEC))
          width = 1;
        if (width == 0)
          goto match_failure;
        wr = !(flags & FL_NOASSIGN);
        if (wr)
        {
          varg_check(ci, arg++);
          p = va_arg(ap, pointer_info *);
        }
        sz = \
          match_string(
            ci, p, flags, ic, stream, width, wr, false, nrchars, &all_chars
          );
        if (sz == 0)
          goto failure;
        break;
      //
      // %s specifier
      //
      case 's':
        if (!(flags & FL_WIDTHSPEC))
          width = UINT_MAX;
        if (width == 0)
          goto match_failure;
        wr = !(flags & FL_NOASSIGN);
        if (wr)
        {
          varg_check(ci, arg++);
          p = va_arg(ap, pointer_info *);
        }
        sz = \
          match_string(
            ci, p, flags, ic, stream, width, wr, true, nrchars, &nonws
          );
        if (sz == 0)
          goto failure;
        break;
      //
      // %[...] specifier
      //
      case '[':
        if (!(flags & FL_WIDTHSPEC))
          width = UINT_MAX;
        if (width == 0)
          goto match_failure;
        format = read_scanset(format, &scanset);
        //
        // If the format string points to a '\0', then there was an error
        // parsing the scanset.
        //
        if (*format == '\0')
          goto match_failure;
        wr = !(flags & FL_NOASSIGN);
        if (wr)
        {
          varg_check(ci, arg++);
          p = va_arg(ap, pointer_info *);
        }
        sz = \
          match_string(
            ci, p, flags, ic, stream, width, wr, true, nrchars, &scanset
          );
        if (sz == 0)
          goto failure;
        break;

#ifdef FLOATING_POINT
      //
      // Floating point specifiers: %a, %A, %e, %E, %f, %F, %g, %G
      //
      case 'a':
      case 'A':
      case 'e':
      case 'E':
      case 'f':
      case 'F':
      case 'g':
      case 'G':
        if (!(flags & FL_WIDTHSPEC) || width > NUMLEN)
          width = NUMLEN;

        if (!width)
          goto failure;
        str = f_collect(ic, stream, inp_buf, width);

        if (str == 0)
          goto failure;

        //
        // Although the length of the number is str-inp_buf+1
        // we don't add the 1 since we counted it already
        //
        nrchars += str - inp_buf;

        if (!(flags & FL_NOASSIGN))
        {
          ld_val = strtold(inp_buf, &tmp_string);
          if (flags & FL_LONGDOUBLE)
            _SAFEWRITE(ld_val, long double);
          else if (flags & FL_LONG)
            _SAFEWRITE(ld_val, double);
          else
            _SAFEWRITE(ld_val, float);
        }
        break;
#endif

    }
    if (!(flags & FL_NOASSIGN) && kind != 'n')
      done++;
    format++;
  }

  goto finish;

failure:
  //
  // In the event of a possible input failure (=eof or read error), the
  // directive should jump to here.
  //
  if (done == 0 && input_failure(stream))
    return EOF;

  //
  // The fscanf function returns the value of the macro EOF if an input failure
  // occurs before the first conversion (if any) has completed. Otherwise, the
  // function returns the number of input items assigned, which can be fewer
  // than provided for, or even zero, in the event of an early matching failure.
  //
  // - C99 Standard
  //
match_failure:
finish:
  return done;

}
