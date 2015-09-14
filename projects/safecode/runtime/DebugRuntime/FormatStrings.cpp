//===- FormatStrings.cpp - Format string function implementation ----------===//
// 
//                            The SAFECode Compiler
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file implements intrinsics and runtime wrapper functions for format
// string functions.
//
//===----------------------------------------------------------------------===//

#include "../include/CWE.h"
#include "../include/FormatStringSupport.h"
#include "../include/strnlen.h"

#include "FormatStrings.h"
#include "PoolAllocator.h"
#include "DebugReport.h"

#include <err.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <wchar.h>

#include <iostream>

using std::cerr;
using std::endl;

//
// To do
//
// - Add support for detecting overlapping writes.
// - Add support for puts() which is transformed by compilers from simple
//   printf() statements.
//

//
// Error reporting functions
//

void out_of_bounds_error(call_info *c,
                         pointer_info *p,
                         size_t obj_len)
{
  //
  // Attempt to look up information about the memory object for which the
  // operation fails.
  //
  void * ObjStart = 0;
  void * ObjEnd = 0;
  void * ptr = p->ptr;
  PDebugMetaData debugmetadataptr = 0;
  dummyPool.DPTree.find (ptr, ObjStart, ObjEnd, debugmetadataptr);

  OutOfBoundsViolation v;
  v.type        = ViolationInfo::FAULT_OUT_OF_BOUNDS;
  v.faultPC     = __builtin_return_address(0);
  v.faultPtr    = p->ptr;
  v.CWE         = CWEBufferOverflow;
  v.SourceFile  = c->source_info;
  v.lineNo      = c->line_no;
  v.PoolHandle  = p->pool;
  v.objLen      = obj_len;
  v.dbgMetaData = debugmetadataptr;
  ReportMemoryViolation(&v);
}

void write_out_of_bounds_error(call_info *c,
                               pointer_info *p,
                               size_t dst_sz,
                               size_t src_sz)
{
  //
  // Attempt to look up information about the memory object for which the
  // operation fails.
  //
  void * ObjStart = 0;
  void * ObjEnd = 0;
  void * ptr = p->ptr;
  PDebugMetaData debugmetadataptr = 0;
  dummyPool.DPTree.find (ptr, ObjStart, ObjEnd, debugmetadataptr);

  WriteOOBViolation v;
  v.type        = ViolationInfo::FAULT_WRITE_OUT_OF_BOUNDS;
  v.faultPC     = __builtin_return_address(0);
  v.faultPtr    = p->ptr;
  v.CWE         = CWEBufferOverflow;
  v.SourceFile  = c->source_info;
  v.lineNo      = c->line_no;
  v.PoolHandle  = p->pool;
  v.dstSize     = dst_sz;
  v.srcSize     = src_sz;
  v.dbgMetaData = debugmetadataptr;
  ReportMemoryViolation(&v);
}

void c_library_error(call_info *c, const char *function)
{
  CStdLibViolation v;
  v.type        = ViolationInfo::FAULT_CSTDLIB;
  v.faultPC     = __builtin_return_address(0);
  v.faultPtr    = 0;
  v.CWE         = CWEBufferOverflow;
  v.SourceFile  = c->source_info;
  v.lineNo      = c->line_no;
  v.function    = function;
  v.dbgMetaData = NULL;
  ReportMemoryViolation(&v);
}

void load_store_error(call_info *c, pointer_info *p)
{
  DebugViolationInfo v;
  v.type        = ViolationInfo::FAULT_LOAD_STORE;
  v.faultPC     = __builtin_return_address(0);
  v.faultPtr    = p->ptr;
  v.CWE         = CWEBufferOverflow;
  v.dbgMetaData = NULL;
  v.SourceFile  = c->source_info;
  v.lineNo      = c->line_no;
  v.PoolHandle  = p->pool;
  ReportMemoryViolation(&v);
}

//
// Intrinsics
//

//
// __sc_fsparameter
//
// Store the given pointer/pool/completeness information into a pointer_info
// structure that gets passed into the transformed format string function.
//
// Inputs:
//     _pool:    pointer to the pool to store
//       ptr:    pointer to store
//     _dest:    pointer to the pointer_info structure to write the information
//               info
//  complete:    completeness byte
//
// This function returns a pointer to the pointer_info structure (= _dest).
//
void *__sc_fsparameter(void *_pool, void *ptr, void *_dest, uint8_t complete)
{
  DebugPoolTy *pool = (DebugPoolTy *) _pool;
  pointer_info *dest = (pointer_info *) _dest;

  dest->ptr  = ptr;
  dest->pool = pool;
  dest->flags = complete;

  return dest;
}

//
// __sc_fscallinfo
//
// Register information about a call to a secured format string function.
// This information is stored into a call_info structure that gets passed into
// the secured format string function.
//
// Inputs:
//  _dest:  A pointer to the call_info structure to write the information into
//  vargc:  The number of varargs arguments to the call to the function
//
//  The NULL-ended variable argument list consists of the vararg parameters to
//  the format string function which are pointer_info structures. The secured
//  format string function will only access these values as pointers.
//
// This function returns a pointer to the call_info structure (= _dest).
//
void *__sc_fscallinfo(void *_dest, uint32_t vargc, ...)
{
  va_list ap;
  call_info *dest = (call_info *) _dest;

  dest->vargc = vargc;

  void *arg;
  unsigned argpos = 0;

  va_start(ap, vargc);
  do
  {
    arg = va_arg(ap, void *);
    dest->whitelist[argpos++] = arg;
  } while (arg != 0);
  va_end(ap);

  //
  // Add empty debugging information.
  //
  dest->tag         = 0;
  dest->source_info = "UNKNOWN";
  dest->line_no     = 0;

  return dest;
}

//
// __sc_fscallinfo_debug
//
// Register information about a call to a secured format string function.
// This information is stored into a call_info structure that gets passed into
// the secured format string function and also holds debugging information.
//
// Inputs:
//  _dest:  A pointer to the call_info structure to write the information into
//  vargc:  The number of varargs arguments to the call to the function
//
//  The NULL-ended variable argument list consists of the vararg parameters to
//  the format string function which are pointer_info structures. The secured
//  format string function will only access these values as pointers.
//
//  After the NULL value is read, there are three more arguments in the
//  variable argument list: an integral tag, a char * pointer to a source
//  filename, and an integral line number. These are also added to the
//  call_info structure.
//
// This function returns a pointer to the call_info structure (= _dest).
//
void *__sc_fscallinfo_debug(void *_dest, uint32_t vargc, ...)
{
  va_list ap;
  call_info *dest = (call_info *) _dest;

  dest->vargc = vargc;

  void *arg;
  unsigned argpos = 0;

  va_start(ap, vargc);
  do
  {
    arg = va_arg(ap, void *);
    dest->whitelist[argpos++] = arg;
  } while (arg != 0);
  //
  // Add debugging information.
  //
  dest->tag         = va_arg(ap, uint32_t);
  dest->source_info = va_arg(ap, const char *);
  dest->line_no     = va_arg(ap, uint32_t);
  va_end(ap);

  return dest;
}

//
// Wrappers for standard library functions
//

//
// Secure runtime wrapper to replace printf()
//
int pool_printf(void *_info, void *_fmt, ...)
{
  va_list ap;
  int result;
  pointer_info *fmt  = (pointer_info *) _fmt;
  call_info    *call = (call_info *)    _info;
  options_t options = 0x0;
  //
  // Set up the output parameter structure to point to stdout as the output
  // file.
  //
  output_parameter p;
  p.output_kind = output_parameter::OUTPUT_TO_FILE;
  p.output.file = stdout;
  //
  // Lock stdout before calling the function which does the printing.
  //
  flockfile(stdout);
  va_start(ap, _fmt);
  result = gprintf(options, p, *call, *fmt, ap);
  va_end(ap);
  funlockfile(stdout);

  return result;
}

//
// Secure runtime wrapper to replace fprintf()
//
int pool_fprintf(void *_info, void *_dest, void *_fmt, ...)
{
  va_list ap;
  int result;
  call_info    *call = (call_info *)    _info;
  pointer_info *fmt  = (pointer_info *) _fmt;
  pointer_info *file = (pointer_info *) _dest;
  options_t options = 0x0;
  //
  // Set up the output parameter structure to point to the output file.
  //
  output_parameter p;
  p.output_kind  = output_parameter::OUTPUT_TO_FILE;
  p.output.file = (FILE *) file->ptr;
  //
  // Lock the file before calling the function which does the printing.
  //
  flockfile(p.output.file);
  va_start(ap, _fmt);
  result = gprintf(options, p, *call, *fmt, ap);
  va_end(ap);
  funlockfile(p.output.file);

  return result;
}

//
// Secure runtime wrapper to replace sprintf()
//
int pool_sprintf(void *_info, void *_dest, void *_fmt, ...)
{
  va_list ap;
  int result;
  output_parameter p;
  options_t options = 0x0;

  call_info   *call = (call_info *)    _info;
  pointer_info *str = (pointer_info *) _dest;
  pointer_info *fmt = (pointer_info *)  _fmt;
  //
  // Set up the output parameter to output into a string.
  //
  p.output_kind = output_parameter::OUTPUT_TO_STRING;
  p.output.string.string = (char *) str->ptr;
  p.output.string.pos    = 0;
  p.output.string.info   = str;
  //
  // Get the object boundaries of the destination array.
  //
  find_object(call, str);
  if (str->flags & HAVEBOUNDS)
    p.output.string.maxsz = (char *) str->bounds[1] - (char *) str->ptr;
  else // If boundaries are not found, assume unlimited length.
    p.output.string.maxsz = SIZE_MAX;
  p.output.string.n = SIZE_MAX; // The caller didn't place a size limitation.

  va_start(ap, _fmt);
  result = gprintf(options, p, *call, *fmt, ap);
  va_end(ap);
  //
  // Add the terminator byte.
  //
  p.output.string.string[p.output.string.pos] = '\0';

  return result;
}

//
// Secure runtime wrapper to replace snprintf()
//
int pool_snprintf(void *_info, void *_dest, size_t n, void *_fmt, ...)
{
  va_list ap;
  int result;
  output_parameter p;
  options_t options = 0x0;

  call_info   *call = (call_info *)    _info;
  pointer_info *str = (pointer_info *) _dest;
  pointer_info *fmt = (pointer_info *)  _fmt;
  //
  // Set up the output parameter to output into a string.
  //
  p.output_kind = output_parameter::OUTPUT_TO_STRING;
  p.output.string.string = (char *) str->ptr;
  p.output.string.pos    = 0;
  p.output.string.info   = str;
  //
  // Get the object boundaries of the destination array.
  //
  find_object(call, str);
  if (str->flags & HAVEBOUNDS)
    p.output.string.maxsz = (char *) str->bounds[1] - (char *) str->ptr;
  else // If boundaries are not found, assume unlimited length.
    p.output.string.maxsz = SIZE_MAX;
  if (n > 0)
    p.output.string.n = n - 1; // Caller-imposed size limitation.
  else
    p.output.string.n = 0;     // Don't write anything.

  va_start(ap, _fmt);
  result = gprintf(options, p, *call, *fmt, ap);
  va_end(ap);
  //
  // Add the terminator byte, if n is not 0.
  // (If n is 0, nothing is written.)
  //
  if (n > 0)
    p.output.string.string[p.output.string.pos] = '\0';

  return result;
}

//
// Secure runtime wrapper to replace __printf_chk()
//
// This function is currently identical to pool_printf().
//
int pool___printf_chk(void *_info, int flags, void *_fmt, ...)
{
  va_list ap;
  int result;
  pointer_info *fmt  = (pointer_info *) _fmt;
  call_info    *call = (call_info *)    _info;
  options_t options = 0x0;
  //
  // Set up the output parameter structure to point to stdout as the output
  // file.
  //
  output_parameter p;
  p.output_kind  = output_parameter::OUTPUT_TO_FILE;
  p.output.file = stdout;
  //
  // Lock stdout before calling the function which does the printing.
  //
  flockfile(stdout);
  va_start(ap, _fmt);
  result = gprintf(options, p, *call, *fmt, ap);
  va_end(ap);
  funlockfile(stdout);

  return result;
}

//
// Secure runtime wrapper to replace __fprintf_chk()
//
// This function is currently identical to pool_fprintf().
//
int pool___fprintf_chk(void *_info, void *_dest, int flags, void *_fmt, ...)
{
  va_list ap;
  int result;
  call_info    *call = (call_info *)    _info;
  pointer_info *fmt  = (pointer_info *) _fmt;
  pointer_info *file = (pointer_info *) _dest;
  options_t options = 0x0;
  //
  // Set up the output parameter structure to point to the output file.
  //
  output_parameter p;
  p.output_kind  = output_parameter::OUTPUT_TO_FILE;
  p.output.file = (FILE *) file->ptr;
  //
  // Lock the file before calling the function which does the printing.
  //
  flockfile(p.output.file);
  va_start(ap, _fmt);
  result = gprintf(options, p, *call, *fmt, ap);
  va_end(ap);
  funlockfile(p.output.file);

  return result;
}

//
// Secure runtime wrapper to replace __sprintf_chk()
//
// The only difference between this function and pool_sprintf() is that this
// function aborts the program when the parameter 'n' (= the size of the
// output buffer) is 0.
//
int pool___sprintf_chk(void *_i, void *_d, int f, size_t n, void *_fmt, ...)
{
  va_list ap;
  int result;
  output_parameter p;
  options_t options = 0x0;

  //
  // Abort if n is 0.
  //
  if (n == 0) {
    DebugViolationInfo v;
    v.type = DebugViolationInfo::FAULT_LOAD_STORE,
      v.faultPC = __builtin_return_address(0),
      v.faultPtr = _d;
      v.CWE = CWEBufferOverflow;
      v.PoolHandle = 0;
      v.dbgMetaData = 0;
      v.SourceFile = "Unknown";
      v.lineNo = 0;
    ReportMemoryViolation(&v);
    abort();
  }

  call_info   *call = (call_info *)    _i;
  pointer_info *str = (pointer_info *) _d;
  pointer_info *fmt = (pointer_info *) _fmt;
  //
  // Set up the output parameter to output into a string.
  //
  p.output_kind = output_parameter::OUTPUT_TO_STRING;
  p.output.string.string = (char *) str->ptr;
  p.output.string.pos    = 0;
  p.output.string.info   = str;
  //
  // Get the object boundaries of the destination array.
  //
  find_object(call, str);
  if (str->flags & HAVEBOUNDS)
    p.output.string.maxsz = (char *) str->bounds[1] - (char *) str->ptr;
  else // If boundaries are not found, assume unlimited length.
    p.output.string.maxsz = SIZE_MAX;
  p.output.string.n = SIZE_MAX; // The caller didn't place a size limitation.

  va_start(ap, _fmt);
  result = gprintf(options, p, *call, *fmt, ap);
  va_end(ap);
  //
  // Add the terminator byte.
  //
  p.output.string.string[p.output.string.pos] = '\0';

  return result;
}

//
// Secure runtime wrapper to replace __snprintf_chk()
//
// This function is the same as pool_snprintf(), except that it aborts the
// program when strlen (= the size of the output buffer) < n.
//
int
pool___snprintf_chk(void *_info,
                    void *_dest,
                    size_t n,
                    int flag,
                    size_t strlen,
                    void *_fmt,
                    ...)
{
  va_list ap;
  int result;
  output_parameter p;
  options_t options = 0x0;

  //
  // Abort if strlen < n.
  //
  if (strlen < n) {
    DebugViolationInfo v;
    v.type = DebugViolationInfo::FAULT_LOAD_STORE,
      v.faultPC = __builtin_return_address(0),
      v.faultPtr = _dest;
      v.CWE = CWEBufferOverflow;
      v.PoolHandle = 0;
      v.dbgMetaData = 0;
      v.SourceFile = "Unknown";
      v.lineNo = 0;
    ReportMemoryViolation(&v);
    abort();
  }

  call_info   *call = (call_info *)    _info;
  pointer_info *str = (pointer_info *) _dest;
  pointer_info *fmt = (pointer_info *)  _fmt;
  //
  // Set up the output parameter to output into a string.
  //
  p.output_kind = output_parameter::OUTPUT_TO_STRING;
  p.output.string.string = (char *) str->ptr;
  p.output.string.pos    = 0;
  p.output.string.info   = str;
  //
  // Get the object boundaries of the destination array.
  //
  find_object(call, str);
  if (str->flags & HAVEBOUNDS)
    p.output.string.maxsz = (char *) str->bounds[1] - (char *) str->ptr;
  else // If boundaries are not found, assume unlimited length.
    p.output.string.maxsz = SIZE_MAX;
  if (n > 0)
    p.output.string.n = n - 1; // Caller-imposed size limitation.
  else
    p.output.string.n = 0;     // Don't write anything.

  va_start(ap, _fmt);
  result = gprintf(options, p, *call, *fmt, ap);
  va_end(ap);
  //
  // Add the terminator byte, if n is not 0.
  // (If n is 0, nothing is written.)
  //
  if (n > 0)
    p.output.string.string[p.output.string.pos] = '\0';

  return result;
}

//
// For functions err(), errx(), warn(), warnx(), and syslog(), which do
// additional work beyond format string processing, we first print the
// string into an allocate buffer, then pass the result into the actual
// function to let it do its additional work.
//
// This is the size of the string to initially allocate for printing into.
//
static const size_t INITIAL_ALLOC_SIZE = 64;
//
// If the wrapper function needs to pass a formatted result to another
// function, but there's been an error during the formatting, it uses this
// message instead.
//
static const char *message_error = "SAFECode: error building message";

//
// Secure runtime wrapper to replace err()
//
void pool_err(void *_info, int eval, void *_fmt, ...)
{
  va_list ap;
  int result;
  output_parameter p;
  options_t options = 0x0;

  call_info   *info = (call_info *)   _info;
  pointer_info *fmt = (pointer_info *) _fmt;
  //
  // On a NULL format string, no formatted message is output.
  //
  if (fmt->ptr == 0)
    err(eval, 0); // Doesn't return.
  //
  // Set up the output parameter to allocate space to output into a string.
  //
  p.output_kind = output_parameter::OUTPUT_TO_ALLOCATED_STRING;
  p.output.alloced_string.string = (char *) malloc(INITIAL_ALLOC_SIZE);
  p.output.alloced_string.bufsz  = INITIAL_ALLOC_SIZE;
  p.output.alloced_string.pos    = 0;
  //
  // Print into the allocated string.
  //
  va_start(ap, _fmt);
  result = gprintf(options, p, *info, *fmt, ap);
  va_end(ap);
  //
  // Print the resulting string if there was no error making it.
  //
  if (result < 0)
    err(eval, "%s", message_error);
  else
    // This call exits the program; we can't free the allocated string.
    err(eval, "%.*s", result, p.output.alloced_string.string);
}

//
// Secure runtime wrapper to replace errx()
//
void pool_errx(void *_info, int eval, void *_fmt, ...)
{
  va_list ap;
  int result;
  output_parameter p;
  options_t options = 0x0;

  call_info   *info = (call_info *)   _info;
  pointer_info *fmt = (pointer_info *) _fmt;
  //
  // On a NULL format string, no formatted message is output.
  //
  if (fmt->ptr == 0)
    errx(eval, 0); // Doesn't return.
  //
  // Set up the output parameter to allocate space to output into a string.
  //
  p.output_kind = output_parameter::OUTPUT_TO_ALLOCATED_STRING;
  p.output.alloced_string.string = (char *) malloc(INITIAL_ALLOC_SIZE);
  p.output.alloced_string.bufsz  = INITIAL_ALLOC_SIZE;
  p.output.alloced_string.pos    = 0;
  //
  // Print into the allocated string.
  //
  va_start(ap, _fmt);
  result = gprintf(options, p, *info, *fmt, ap);
  va_end(ap);
  //
  // Print the resulting string if there was no error making it.
  //
  if (result < 0)
    errx(eval, "%s", message_error);
  else
    // This call exits the program; we can't free the allocated string.
    errx(eval, "%.*s", result, p.output.alloced_string.string);
}

//
// Secure runtime wrapper to replace warn()
//
void pool_warn(void *_info, void *_fmt, ...)
{
  va_list ap;
  int result;
  output_parameter p;
  options_t options = 0x0;

  call_info   *info = (call_info *)   _info;
  pointer_info *fmt = (pointer_info *) _fmt;
  //
  // On a NULL format string, no formatted message is output.
  //
  if (fmt->ptr == 0)
  {
    warn(0);
    return;
  }
  //
  // Set up the output parameter to allocate space to output into a string.
  //
  p.output_kind = output_parameter::OUTPUT_TO_ALLOCATED_STRING;
  p.output.alloced_string.string = (char *) malloc(INITIAL_ALLOC_SIZE);
  p.output.alloced_string.bufsz  = INITIAL_ALLOC_SIZE;
  p.output.alloced_string.pos    = 0;
  //
  // Print into the allocated string.
  //
  va_start(ap, _fmt);
  result = gprintf(options, p, *info, *fmt, ap);
  va_end(ap);
  //
  // Print and free the resulting string if there was no error making it.
  //
  if (result < 0)
    warn("%s", message_error);
  else
  {
    warn("%.*s", result, p.output.alloced_string.string);
    free(p.output.alloced_string.string);
  }
  return;
}

//
// Secure runtime wrapper to replace warnx()
//
void pool_warnx(void *_info, void *_fmt, ...)
{
  va_list ap;
  int result;
  output_parameter p;
  options_t options = 0x0;

  call_info   *info = (call_info *)   _info;
  pointer_info *fmt = (pointer_info *) _fmt;
  //
  // On a NULL format string, no formatted message is output.
  //
  if (fmt->ptr == 0)
  {
    warnx(0);
    return;
  }
  //
  // Set up the output parameter to allocate space to output into a string.
  //
  p.output_kind = output_parameter::OUTPUT_TO_ALLOCATED_STRING;
  p.output.alloced_string.string = (char *) malloc(INITIAL_ALLOC_SIZE);
  p.output.alloced_string.bufsz  = INITIAL_ALLOC_SIZE;
  p.output.alloced_string.pos    = 0;
  //
  // Print into the allocated string.
  //
  va_start(ap, _fmt);
  result = gprintf(options, p, *info, *fmt, ap);
  va_end(ap);
  //
  // Print and free the resulting string if there was no error making it.
  //
  if (result < 0)
    warnx("%s", message_error);
  else
  {
    warnx("%.*s", result, p.output.alloced_string.string);
    free(p.output.alloced_string.string);
  }
  return;
}

//
// Secure runtime wrapper to replace syslog()
//
void pool_syslog(void *_info, int priority, void *_fmt, ...)
{
  va_list ap;
  int result;
  output_parameter p;
  options_t options = 0x0;

  call_info   *info = (call_info *)   _info;
  pointer_info *fmt = (pointer_info *) _fmt;
  //
  // Set up the output parameter to allocate space to output into a string.
  //
  p.output_kind = output_parameter::OUTPUT_TO_ALLOCATED_STRING;
  p.output.alloced_string.string = (char *) malloc(INITIAL_ALLOC_SIZE);
  p.output.alloced_string.bufsz  = INITIAL_ALLOC_SIZE;
  p.output.alloced_string.pos    = 0;
  //
  // Print into the allocated string.
  //
  va_start(ap, _fmt);
  result = gprintf(options, p, *info, *fmt, ap);
  va_end(ap);
  //
  // Print and free the resulting string if there was no error making it.
  //
  if (result < 0)
    syslog(priority, "%s", message_error);
  else
  {
    syslog(priority, "%.*s", result, p.output.alloced_string.string);
    free(p.output.alloced_string.string);
  }
  return;
}

//
// Secure runtime wrapper function to replace scanf()
//
int pool_scanf(void *_info, void *_fmt, ...)
{
  va_list ap;
  int result;
  input_parameter p;

  call_info    *info = (call_info *)   _info;
  pointer_info  *fmt = (pointer_info *) _fmt;
  //
  // Set up the input parameter to read from stdin.
  //
  p.input_kind = input_parameter::INPUT_FROM_STREAM;
  p.input.stream.stream = stdin;
  //
  // Lock stdin before calling the function to do the scanning.
  //
  flockfile(stdin);
  va_start(ap, _fmt);
  result = gscanf(0, p, *info, *fmt, ap);
  va_end(ap);
  funlockfile(stdin);

  return result;
}

//
// Secure runtime wrapper function to replace fscanf()
//
int pool_fscanf(void *_info, void *_src, void *_fmt, ...)
{
  va_list ap;
  int result;
  input_parameter p;

  call_info  *info  = (call_info *)   _info;
  pointer_info *str = (pointer_info *) _src;
  pointer_info *fmt = (pointer_info *) _fmt;
  FILE *stream      = (FILE *) str->ptr;
  //
  // Set the input parameter to read from the input stream.
  //
  p.input_kind = input_parameter::INPUT_FROM_STREAM;
  p.input.stream.stream = stream;
  //
  // Lock the stream before calling the function to do the scanning
  //
  flockfile(stream);
  va_start(ap, _fmt);
  result = gscanf(0, p, *info, *fmt, ap);
  va_end(ap);
  funlockfile(stream);

  return result;
}

//
// Secure runtime wrapper to replace sscanf()
//
int pool_sscanf(void *_info, void *_str, void *_fmt, ...)
{
  va_list ap;
  int result;
  input_parameter p;

  call_info   *info = (call_info *)   _info;
  pointer_info *str = (pointer_info *) _str;
  pointer_info *fmt = (pointer_info *) _fmt;
  //
  // Set the input parameter to read from a string
  //
  p.input_kind = input_parameter::INPUT_FROM_STRING;
  p.input.string.string = (const char *) str->ptr;
  p.input.string.pos    = 0;
  //
  // Check if the input string is terminated within object boundaries,
  // if we have the object boundaries.
  //
  find_object(info, str);
  if (str->flags & HAVEBOUNDS)
  {
    const char *string = (const char *) str->ptr;
    size_t maxlen = (char *) str->bounds[1] - string;
    size_t len    = _strnlen(string, 1 + maxlen);
    if (len > maxlen)
    {
      cerr << "Input string not terminated within object bounds!" << endl;
      out_of_bounds_error(info, str, len);
    }
  }

  va_start(ap, _fmt);
  result = gscanf(0, p, *info, *fmt, ap);
  va_end(ap);

  return result;
}

extern int
internal_printf(
  const options_t, output_parameter &, call_info &, const char *, va_list
);

extern int
internal_scanf(
  const options_t, input_parameter &, call_info &, const char *, va_list
);

//
// gprintf()
//
// Secure general printf() family replacement
//
// Attempts to verify the following:
//  - The output string is not written to out of bounds, if there is an output
//    string specified.
//  - The format string is not read out of bounds.
//  - A %s format directive will not result in an out of bounds read of a
//    string.
//  - A %n format directive will not result in an out of bounds write into
//    a parameter.
//  - Only the variable arguments that were passed are accessed.
//
// Arguments:
//  Options:       Parameter describing whether to use various features
//                   (eg. enable/disable parsing of the %m directive)
//  Output:        Information about where to print the output
//  CInfo:         Information about the call parameters (call_info structure)
//  FormatString:  pointer_info structure holding the format string
//  Args:          The list of arguments to the format string
//
// Return values:
//  If successful, the function returns the number of characters that would have
//  been printed if the output had unbounded space.
//
//  If a (non memory-safety) error occurred, the function returns a negative
//  value.
//
int
gprintf(const options_t Options,
        output_parameter &Output,
        call_info &CInfo,
        pointer_info &FormatString,
        va_list Args)
{
  int result;
  const char *Fmt;
  //
  // Get the object boundaries for the format string.
  //
  find_object(&CInfo, &FormatString);
  Fmt = (const char *) FormatString.ptr;
  //
  // Make sure the format string isn't NULL.
  //
  if (Fmt == 0)
  {
    cerr << "NULL format string!" << endl;
    c_library_error(&CInfo, "printf");
    return 0;
  }
  //
  // Check to make sure the format string is within object boundaries, if we
  // have them.
  //
  if (FormatString.flags & HAVEBOUNDS)
  {
    size_t maxbytes = 1 + (char *) FormatString.bounds[1] - Fmt;
    size_t len = _strnlen(Fmt, maxbytes);
    if (len == maxbytes)
    {
      cerr << "Format string not terminated within object bounds!" << endl;
      out_of_bounds_error(&CInfo, &FormatString, len);
    }
  }

  result = internal_printf(Options, Output, CInfo, Fmt, Args);
  return result;
}

//
// gscanf()
//
// Secure general scanf() family replacement
//
// Attempts to verify the following:
//  - The format string is not read out of bounds.
//  - Only the variable arguments that were passed are accessed.
//  - A format directive which writes into a variable argument is writing into
//    a destination object that is large enough to hold the write.
//
// Arguments:
//   Input        - An input_parameter structure describing the input
//   CInfo        - A call_info structure describing the call arguments
//   FormatString - The pointer_info structure for the format string
//   Args         - The list of arguments
//
// Returns:
//   The function returns the number of format directives that were
//   successfully matched with the input, or EOF if there was an input failure
//   before any directives could be matched.
//
int
gscanf(const options_t Options,
       input_parameter &Input,
       call_info &CInfo,
       pointer_info &FormatString,
       va_list Args)
{
  int result;
  const char *Fmt;
  //
  // Get the object boundaries for the formating string.
  //
  find_object(&CInfo, &FormatString);
  Fmt = (const char *) FormatString.ptr;
  //
  // Make sure the format string isn't NULL.
  //
  if (Fmt == 0)
  {
    cerr << "NULL format string!" << endl;
    c_library_error(&CInfo, "scanf");
    return 0;
  }
  //
  // Check to make sure the format string is a nul-terminated within the
  // boundaries of its object, if we have the boundaries.
  //
  if (FormatString.flags & HAVEBOUNDS)
  {
    size_t maxbytes = 1 + (char *) FormatString.bounds[1] - Fmt;
    size_t len      = _strnlen(Fmt, maxbytes);
    if (len == maxbytes)
    {
      cerr << "Format string not terminated within object bounds!" << endl;
      out_of_bounds_error(&CInfo, &FormatString, len);
    }
  }

  result = internal_scanf(Options, Input, CInfo, Fmt, Args);

  return result;
}
