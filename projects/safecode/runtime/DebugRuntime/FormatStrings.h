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

#ifndef _FORMAT_STRINGS_H
#define _FORMAT_STRINGS_H

#include <cstdio>
#include <cstdarg>
#include <cstddef>
#include <iostream>
#include <map>
#include <stdint.h>

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
  // Options for whether the output string goes.
  enum
  {
    // A dynamically allocated string with a maximum length
    OUTPUT_TO_BOUNDED_ALLOCATED_STRING,
    // A dynamically allocated string
    OUTPUT_TO_ALLOCATED_STRING,
    // A string
    OUTPUT_TO_STRING,
    // A file
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

//
// Options for the printf() / scanf() runtime function.
//
#define USE_M_DIRECTIVE    0x01 // Enable parsing of the %m directive
#define POINTERS_UNWRAPPED 0x02 // Pointer arguments aren't wrapped
#define NO_STACK_CHECKS    0x04 // Don't check for va_list going out of bounds
#define NO_WLIST_CHECKS    0x08 // Don't check the whitelist
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
extern void
out_of_bounds_error(call_info *, pointer_info *, size_t);

extern void
write_out_of_bounds_error(call_info *, pointer_info *, size_t, size_t);

extern void
c_library_error(call_info *, const char *);

extern void
load_store_error(call_info *c, pointer_info *p);

//
// Printing/scanning functions
//
extern int
gprintf(
  const options_t, output_parameter &, call_info &, pointer_info &, va_list
);

extern int
gscanf(
  const options_t, input_parameter &, call_info &, pointer_info &, va_list
);

namespace
{
  using std::cerr;
  using std::endl;
  using std::map;

  using namespace llvm;

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
      ExternalObjects->find(p->ptr, p->bounds[0], p->bounds[1]))
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
  is_in_whitelist(call_info *c, const options_t options, pointer_info *p)
  {
    if (options & NO_WLIST_CHECKS)
      return true;
    void *val = (options & POINTERS_UNWRAPPED) ? p->ptr : (void *) p;
    void **whitelist = c->whitelist;
    do
    {
      if (val == *whitelist)
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
  write_check(call_info *c, const options_t options, pointer_info *p, size_t n)
  {
    size_t max;
    //
    // First check if the object is a valid pointer_info structure.
    //
    if (p == 0 || !is_in_whitelist(c, options, p))
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
  varg_check(call_info *c, const options_t options, unsigned pos)
  {
    if (options & NO_STACK_CHECKS)
      return true;
    else if (pos > c->vargc)
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
    else
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
  unwrap_pointer(call_info *c, const options_t options, void *p)
  {
    if (is_in_whitelist(c, options, (pointer_info *) p))
      return ((pointer_info *) p)->ptr;
    else
      return p;
  }

  //
  // wrap_pointer()
  //
  // Wraps a pointer in a pointer_info structure, if pointers are unwrapped.
  //
  // Inputs:
  //   options - the options passed to the format string function
  //   ptr     - the pointer to wrap
  //   mp      - a map from pointers to their corresponding wrappers
  //
  // Returns:
  //   If (options & POINTERS_UNWRAPPED) is false, returns ptr.
  //   If (options & POINTERS_UNWRAPPED) is true, this function looks up or
  //   adds an entry to the map mp which is the wrapped version of ptr.
  //
  static inline void *
  wrap_pointer(const options_t options,
               void *ptr,
               map<void *, pointer_info *> &mp)
  {
    if (!(options & POINTERS_UNWRAPPED))
      return ptr;
    // Try to find the pointer wrapper in the map.
    map<void *, pointer_info *>::iterator it = mp.find(ptr);
    if (it != mp.end()) {
      return it->second;
    }
    // If not found, create a new entry for the pointer...
    pointer_info *&p = mp[ptr] = new pointer_info();
    p->ptr = ptr;
    p->pool = 0;
    p->flags = 0; // Don't add any flags.
    return p;
  }
}

#endif
