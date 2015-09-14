//===- FormatStrings.h - Header for the format string function runtime ----===//
// 
//                            The SAFECode Compiler
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file contains definitions of structures and functions used by the
// format string functions in the runtime.
//
//===----------------------------------------------------------------------===//

#ifndef _FORMAT_STRING_RUNTIME_H
#define _FORMAT_STRING_RUNTIME_H

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

#include <iostream>

#include "PoolAllocator.h"

//
// Enable support for floating point numbers.
//
#define FLOATING_POINT

//
// The pointer_info structure and associated flags
// This holds a pointer argument to a format string function.
// This structure is initialized by a call to sc.fsparameter.
//
#define ISCOMPLETE  0x01 // Whether the pointer is complete according to DSA
#define ISRETRIEVED 0x02 // Whether there has been an attempt made to retrive
                         // the target object's boundaries
#define HAVEBOUNDS  0x04 // Whether the boundaries were retrieved successfully
#define NULL_PTR    0x08 // Whether the pointer in the structure is NULL

typedef struct
{
  void *ptr;             // The pointer which is wrapped by this structure
  void *pool;            // The pool to which the pointer belongs
  void *bounds[2];       // Space for retrieving object boundaries
  uint8_t flags;         // See above
} pointer_info;

//
// The call_info structure, which is initialized by sc.fscallinfo before a call
// to a format string function.
//
typedef struct
{
  uint32_t vargc;        // The number of varargs to this function call
  uint32_t tag;          // tag, line_no, source_file hold debug information
  uint32_t line_no;
  const char *source_info;
  void *whitelist[1];    // This is a list of pointer arguments that the
                         // format string function should treat as varargs
                         // arguments which are pointers. These arguments are
                         // all pointer_info structures. The list is terminated
                         // by a NULL element.
} call_info;

//
// This structure describes where to print the output for the internal printf()
// wrapper.
//
typedef struct
{
  enum
  {
    OUTPUT_TO_ALLOCATED_STRING,
    OUTPUT_TO_STRING,
    OUTPUT_TO_FILE
  } output_kind;
  union
  {
    FILE *file;
    struct
    {
      pointer_info *info;
      char   *string;
      size_t pos;
      size_t maxsz;  // Maximum size of the array that can be written into the
                     // object safely. (SAFECode-imposed)
      size_t n;      // The maximum number of bytes to write. (user-imposed)
    } string;
    struct
    {
      char *string;
      size_t bufsz;
      size_t pos;
    } alloced_string;
  } output;
} output_parameter;

#define USE_M_DIRECTIVE 0x0001 // Enable parsing of the %m directive for
                               // syslog()
typedef unsigned options_t;

//
// This structure describes where to get input characters for the internal
// scanf() wrapper.
//
typedef struct
{
  enum
  {
    INPUT_FROM_STREAM,
    INPUT_FROM_STRING
  } input_kind;
  union
  {
    struct
    {
      FILE *stream;
      char lastch;
    } stream;
    struct
    {
      const char *string;
      size_t pos;
    } string;
  } input;
} input_parameter;

//
// Error reporting functions
//
extern void out_of_bounds_error(call_info *c,
                                pointer_info *p,
                                size_t obj_len);

extern void write_out_of_bounds_error(call_info *c,
                                      pointer_info *p,
                                      size_t dst_sz,
                                      size_t src_sz);

extern void c_library_error(call_info *c, const char *function);

extern void load_store_error(call_info *c, pointer_info *p);

//
// Printing/scanning functions
//
extern int gprintf(const options_t &Options,
                   output_parameter &P,
                   call_info &C,
                   pointer_info &FormatString,
                   va_list Args);

extern int gscanf(input_parameter &P,
                  call_info &C,
                  pointer_info &FormatString,
                  va_list Args);

extern int internal_printf(const options_t &options,
                           output_parameter &P,
                           call_info &C,
                           const char *fmt,
                           va_list args);

extern int internal_scanf(input_parameter &p,
                          call_info &c,
                          const char *fmt,
                          va_list args);

namespace
{
  using std::cerr;
  using std::endl;

  using namespace NAMESPACE_SC;

  //
  // find_object()
  //
  // Get the object boundaries of the pointer associated with the pointer_info
  // structure.
  //
  // Inputs:
  //  c - a pointer to the relevant call_info structure
  //  p - a pointer to a valid pointer_info structure which contains the pointer
  //      whose object boundaries should be discovered
  // 
  //
  static inline void
  find_object(call_info *c, pointer_info *p)
  {
    if (p->flags & ISRETRIEVED)
      return;

    DebugPoolTy *pool = (DebugPoolTy *) p->pool;
    if (p->ptr == 0)
      p->flags |= NULL_PTR;
    else if ((pool && pool->Objects.find(p->ptr, p->bounds[0], p->bounds[1])) ||
        ExternalObjects.find(p->ptr, p->bounds[0], p->bounds[1]))
    {
      p->flags |= HAVEBOUNDS;
    }
    else if (p->flags & ISCOMPLETE)
    {
      cerr << "Object not found in pool!" << endl;
      load_store_error(c, p);
    }
    p->flags |= ISRETRIEVED;
  }
  
  //
  // is_in_whitelist()
  //
  // Check if a (non-NULL) pointer_info structure exists in the whitelist of the
  // given call_info structure.
  //
  static inline bool
  is_in_whitelist(call_info *c, pointer_info *p)
  {
    void **whitelist = c->whitelist;
    do
    {
      if ((void *) p == *whitelist)
        break;
      whitelist++;
    } while (*whitelist);
    return (*whitelist != 0);
  }

  //
  // object_len()
  //
  // Get the number of bytes in the object that the pointer associated with the
  // pointer_info structure points to, from address the pointer points to, until
  // the end of the object.
  //
  // Note: Call find_object() before calling this.
  //
  static inline size_t
  object_len(pointer_info *p)
  {
    return 1 + (size_t) ((char *) p->bounds[1] - (char *) p->ptr);
  }

  //
  // write_check()
  //
  // Check if a write into the object associated with the given pointer_info
  // structure of n bytes would be safe.
  //
  // Inputs:
  //  c - the relevant call_info structure
  //  p - the pointer_info structure
  //  n - the size of the write
  //
  // This function outputs any relevant SAFECode messages. It returns true if
  // the write is to be considered safe, and false otherwise.
  //
  static inline bool
  write_check(call_info *c, pointer_info *p, size_t n)
  {
    size_t max;
    //
    // First check if the object is a valid pointer_info structure.
    //
    if (p == 0 || !is_in_whitelist(c, p))
    {
      cerr << "The destination of the write isn't a valid pointer!" << endl;
      c_library_error(c, "va_arg");
      return false;
    }
    //
    // Look up the object boundaries.
    //
    find_object(c, p);
    //
    // Check for NULL pointer writes.
    //
    if (p->flags & NULL_PTR)
    {
      cerr << "Writing into a NULL pointer!" << endl;
      c_library_error(c, "va_arg");
      return false;
    }
    else if (p->flags & HAVEBOUNDS)
    {
      max = object_len(p);
      if (n > max)
      {
        cerr << "Writing out of bounds!" << endl;
        write_out_of_bounds_error(c, p, max, n);
        return false;
      }
      else
        return true;
    }
    //
    // Assume an object without discovered boundaries has enough space.
    //
    return true;
  }

  //
  // varg_check()
  //
  // Check if too many arguments are accessed, if so, report an error.
  //
  // Inputs:
  //  c   - the call_info structure describing the function call
  //  pos - the number of the variable argument that the function is trying to
  //        access
  //
  // This function returns true if an argument is trying to be accessed beyond
  // the arguments that exist to the function call, and false otherwise.
  //
  static inline bool
  varg_check(call_info *c, unsigned pos)
  {
    if (pos > c->vargc)
    {
      if (c->vargc == 1)
      {
        cerr << "Attempting to access argument " << pos << \
          " but there is only 1 argument!" << endl;
      } 
      else
      {
        cerr << "Attempting to access argument " << pos << \
          " but there are only " << c->vargc << " arguments!" << endl;
      }
      c_library_error(c, "va_arg");
      return true;
    }
    return false;
  }

  //
  // unwrap_pointer()
  //
  // Get the actual pointer argument from the given parameter. If the parameter
  // is whitelisted and so a wrapper, this retrieves the pointer from the
  // wrapper. Otherwise it just returns the parameter because it isn't 
  // recognized as a wrapper.
  //
  // Inputs:
  //  c - a pointer to the relevant call_info structure
  //  p - a pointer to the pointer_info structure to query
  //
  // Returns:
  //  This function returns p->ptr if p is a valid pointer_info structure found
  //  in the whitelist, and p otherwise.
  //
  static inline void *
  unwrap_pointer(call_info *c, void *p)
  {
    if (is_in_whitelist(c, (pointer_info *) p))
      return ((pointer_info *) p)->ptr;
    else
      return p;
  }

  //
  // This function is identical to strnlen(), which is not found on Mac OS X.
  //
  static inline size_t
  _strnlen(const char *s, size_t n)
  {
    size_t i;
    for (i = 0; i < n; i++)
      if (s[i] == '\0')
        break;
    return i;
  }

}

#endif
