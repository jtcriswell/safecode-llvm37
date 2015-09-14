//===------- stdarg.cpp - Runtime for functions handling va_lists ---------===//
// 
//                          The SAFECode Compiler
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file implements the SAFECode intrinsics that manage va_lists, and it
// also implements runtime wrapper versions of functions that take va_lists.
//
//===----------------------------------------------------------------------===//

#include "CStdLib.h"
#include "FormatStrings.h"

#include <cstdarg>
#include <cstdio>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <stdint.h>
#include <syslog.h>
#include <vector>

using std::map;
using std::vector;
using std::set;

// Declare SAFECode intrinsics as C functions.
extern "C" uint32_t __sc_targetcheck(void *func);
extern "C" void __sc_varegister(va_list ap, uint32_t id);
extern "C" void __sc_vacopyregister(va_list dest, va_list src);
extern "C" void __sc_vacallregister(void *func, uint32_t argc, ...);
extern "C" void __sc_vacallunregister();

typedef map<void *, unsigned> VaListToArgIndexMap;
typedef set<void *> VaListSet;
typedef struct {
  VaListSet referrers;
  vector<void *> pointerList;
} ArgListEntry;
 
// Used for determining if the expected target of a vararg function call is the
// actual target.
static void *expectedTarget = 0;
// This vector contains information about all registered va_lists in the
// program.
static vector<ArgListEntry> & argLists (void) {
  static vector<ArgListEntry> realargLists;
  return realargLists;
}

// A map from a va_list to the index of its registered contents in the argLists
// vector above.
static VaListToArgIndexMap & vaListRegistrations (void) {
  static VaListToArgIndexMap realvaListRegistrations;
  return realvaListRegistrations;
}

// Remove all references of a va_list from the internal data structures.
static inline void clearVaList(va_list ap) {
  if (!vaListRegistrations().count(ap))
    return;
  unsigned idx = vaListRegistrations()[ap];
  vaListRegistrations().erase(ap);
  argLists()[idx].referrers.erase(ap);
}

// Check if the expected callee is the actual callee.
// Returns a number under 0xffffffff if this is the case, and otherwise returns
// 0xffffffff.
uint32_t __sc_targetcheck(void *func) {
  uint32_t id = (expectedTarget == func) ? argLists().size() - 1 : 0xffffffffu;
  // Always reset the expected target to NULL.
  // This is needed for correctness, eg. in the case of recursive calls of the
  // same function from external code.
  expectedTarget = 0;
  return id;
}

// Associate a va_list with an index returned from __sc_targetcheck.
void __sc_varegister(va_list ap, uint32_t id) {
  // Invalid index
  if (id == 0xffffffffu)
    return;
  // Remove all prior references of this list.
  clearVaList(ap);
  // Insert the list into the appropriate place.
  vaListRegistrations()[ap] = id;
  argLists()[id].referrers.insert(ap);
}

// Associate one va_list with the information from another va_list.
void __sc_vacopyregister(va_list dest, va_list src) {
  // If the source list is not registered, don't do anything.
  if (!vaListRegistrations().count(src))
    return;
  // Remove all references of the destination list.
  clearVaList(dest);
  // Register the destination list with the same information as the source list.
  unsigned idx = vaListRegistrations()[dest] = vaListRegistrations()[src];
  argLists()[idx].referrers.insert(dest);
}

// Add a new entry to the lists of pointer arguments.
void __sc_vacallregister(void *func, uint32_t argc, ...) {
  argLists().push_back(ArgListEntry());
  ArgListEntry &end = argLists().back();
  // Find all the pointer arguments that were passed to this function and put
  // them in the list.
  va_list ap;
  void *arg;
  va_start(ap, argc);
  for (arg = va_arg(ap, void *); arg != 0; arg = va_arg(ap, void *))
    end.pointerList.push_back(arg);
  va_end(ap);
  // Set the value of the passed function pointer as the expected target.
  expectedTarget = func;
}

// Unregister the last pointer argument list.
void __sc_vacallunregister() {
  ArgListEntry &last = argLists().back();
  VaListSet::iterator idx = last.referrers.begin();
  VaListSet::iterator end = last.referrers.end();
  // Remove each va_list associated with this list from the hash table.
  for (; idx != end; ++idx)
    vaListRegistrations().erase(*idx);
  // Now remove the last entry altogether.
  argLists().pop_back();
}

//
// Allocate a call_info structure that describes a call to a format string
// function. call_info is defined as:
//
// typedef struct {
//   uint32_t vargc;
//   uint32_t tag;
//   uint32_t line_no;
//   const char *source_info
//   void *whitelist[1];
// } call_info;
//
// Inputs
//   result   - a reference to a pointer that holds the location of the
//              allocation
//   ap       - the va_list associated with the function call
//   TAG      - tag information for debugging purposes
//   SRC_INFO - source and line number information for debugging purposes
//
// Returns
//  On return, result points to allocated memory.
//  This function returns true if the pointer list associated with the
//  va_list argument was found, and false if the va_list was not
//  recognized.
//
static inline bool
build_call_info(call_info *&result, va_list ap, TAG, SRC_INFO) {
  // Check if the list is registered.
  VaListToArgIndexMap::iterator idx = vaListRegistrations().find(ap);
  if (idx == vaListRegistrations().end()) {
    // If not registered, return a call_info structure without the whitelist.
    result = (call_info *) malloc(sizeof(call_info));
    if (result != 0) {
      // Don't limit the number of arguments to access.
      result->vargc = 0xffffffffu;
      result->tag = tag;
      result->line_no = lineNo;
      result->source_info = SourceFile;
      result->whitelist[0] = 0;
    }
    return false;
  }
  // Otherwise, allocate and populate a call_info structure with the pointer
  // whitelist as specified in the corresponding index.
  else {
    const unsigned index = idx->second;
    const vector<void *> &pointerList = argLists()[index].pointerList;
    const size_t wl_size = pointerList.size();
    // Allocate enough space so that the structure can hold the whitelist.
    result =
      (call_info *) malloc(sizeof(call_info) + wl_size * sizeof(void *));
    if (result != 0) {
      // Don't limit the number of arguments to access.
      result->vargc = 0xffffffffu;
      result->tag   = tag;
      result->line_no = lineNo;
      result->source_info = SourceFile;
      // Copy over the pointer list for this registration into the whitelist.
      for (unsigned i = 0; i < wl_size; ++i)
        result->whitelist[i] = pointerList[i];
      // End the whitelist with NULL.
      result->whitelist[wl_size] = 0;
    }
    return true;
  }
}

// Initialize a pointer_info structure around a pointer.
static inline void
create_wrapper(pointer_info &dest, void *p, DebugPoolTy *pool, bool complete) {
  dest.ptr = p;
  dest.pool = (void *) pool;
  dest.flags = complete ? ISCOMPLETE : 0;
}
 
typedef const uint8_t bv_t;

//
// pool_vprintf()
//
// See pool_vprintf_debug().
//
int pool_vprintf(DebugPoolTy *fmtPool,
                 char *fmt,
                 va_list ap,
                 bv_t complete) {
  return pool_vprintf_debug(fmtPool, fmt, ap, complete, DEFAULTS);
}

//
// Secure runtime wrapper to replace vprintf()
//
// Inputs
//   fmtPool  - pool for format string
//   fmt      - format string
//   ap       - list of arguments to vprintf()
//   complete - completeness bit vector
//   TAG      - tag information for debugging purposes
//   SRC_INFO - source and line number information for debugging purposes
//
// Returns
//   This function returns the number of bytes written to stdout, or a negative
//   number on error.
//
int pool_vprintf_debug(DebugPoolTy *fmtPool,
                       char *fmt,
                       va_list ap,
                       bv_t complete,
                       TAG,
                       SRC_INFO) {
  // Create the call_info structure associated with this call.
  call_info *cinfo;
  bool vaListFound = build_call_info(cinfo, ap, tag, SRC_INFO_ARGS);
  // On error creating the structure, just print without runtime checks.
  if (cinfo == 0)
    return vprintf(fmt, ap);

  // Tell the gprintf() function that a) pointers are unwrapped and b) we
  // don't track the size of the vararg list.
  options_t options = POINTERS_UNWRAPPED | NO_STACK_CHECKS;
  // Also, tell gprintf() not to do whitelist checks if the va_list isn't
  // registered.
  if (!vaListFound)
    options |= NO_WLIST_CHECKS;

  // Set up the output to stdout.
  output_parameter p;
  p.output_kind = output_parameter::OUTPUT_TO_FILE;
  p.output.file = stdout;

  // Wrap the format string in a pointer_info structure.
  pointer_info fmt_info;
  create_wrapper(fmt_info, fmt, fmtPool, ARG1_COMPLETE(complete));

  // Call the printing function with stdout locked.
  flockfile(stdout);
  int result = gprintf(options, p, *cinfo, fmt_info, ap);
  funlockfile(stdout);

  // Free the allocated call_info structure.
  free(cinfo);

  return result;
}

//
// pool_vfprintf()
//
// See pool_vfprintf_debug().
//
int pool_vfprintf(DebugPoolTy *fPool,
                  DebugPoolTy *fmtPool,
                  void *fil,
                  char *fmt,
                  va_list ap,
                  bv_t complete) {
  return pool_vfprintf_debug(fPool, fmtPool, fil, fmt, ap, complete, DEFAULTS);
}

//
// Secure runtime wrapper function to replace vfprintf()
//
// Inputs
//   fPool    - pool for file
//   fmtPool  - pool for format string
//   fil      - pointer to FILE object that specified output location
//   fmt      - format string
//   ap       - list of arguments to vfprintf()
//   complete - completeness bit vector
//   TAG      - tag information for debugging purposes
//   SRC_INFO - source and line number information for debugging purposes
//
// Returns
//   This function returns the number of bytes written to fil, or a negative
//   number on error.
//
int pool_vfprintf_debug(DebugPoolTy *fPool,
                        DebugPoolTy *fmtPool,
                        void *fil,
                        char *fmt,
                        va_list ap,
                        bv_t complete,
                        TAG,
                        SRC_INFO) {
  // Create the call_info structure associated with this call.
  call_info *cinfo;
  bool vaListFound = build_call_info(cinfo, ap, tag, SRC_INFO_ARGS);
  // On error creating the structure, just print without runtime checks.
  if (cinfo == 0)
    return vfprintf((FILE *) fil, fmt, ap);

  // Tell the gprintf() function that a) pointers are unwrapped and b) we
  // don't track the size of the vararg list.
  options_t options = POINTERS_UNWRAPPED | NO_STACK_CHECKS;
  // Also, tell gprintf() not to do whitelist checks if the va_list isn't
  // registered.
  if (!vaListFound)
    options |= NO_WLIST_CHECKS;

  // Set up the output to the passed FILE object.
  output_parameter p;
  p.output_kind = output_parameter::OUTPUT_TO_FILE;
  p.output.file = (FILE *) fil;

  // Wrap the format string in a pointer_info structure.
  pointer_info fmt_info;
  create_wrapper(fmt_info, fmt, fmtPool, ARG2_COMPLETE(complete));

  // Call the printing function with the output file locked.
  flockfile((FILE *) fil);
  int result = gprintf(options, p, *cinfo, fmt_info, ap);
  funlockfile((FILE *) fil);

  // Free the allocated call_info structure.
  free(cinfo);

  return result;
}

//
// pool_vsprintf()
//
// See pool_vsprintf_debug().
//
int pool_vsprintf(DebugPoolTy *sPool,
                  DebugPoolTy *fmtPool,
                  char *str,
                  char *fmt,
                  va_list ap,
                  bv_t complete) {
  return pool_vsprintf_debug(sPool, fmtPool, str, fmt, ap, complete, DEFAULTS);
}

//
// Secure runtime wrapper function to replace vsprintf()
//
// Inputs
//   strPool  - pool for output character buffer
//   fmtPool  - pool for format string
//   str      - pointer to output character buffer
//   fmt      - format string
//   ap       - list of arguments to vsprintf()
//   complete - completeness bit vector
//   TAG      - tag information for debugging purposes
//   SRC_INFO - source and line number information for debugging purposes
//
// Returns
//   This function returns the number of bytes written to str, or a negative
//   number on error.
//
int pool_vsprintf_debug(DebugPoolTy *strPool,
                        DebugPoolTy *fmtPool,
                        char *str,
                        char *fmt,
                        va_list ap,
                        bv_t complete,
                        TAG,
                        SRC_INFO) {
  // Create the call_info structure associated with this call.
  call_info *cinfo;
  bool vaListFound = build_call_info(cinfo, ap, tag, SRC_INFO_ARGS);
  // On error creating the structure, just print with no runtime checks.
  if (cinfo == 0)
    return vsprintf(str, fmt, ap);

  // Tell the gprintf() function that a) pointers are unwrapped and b) we
  // don't track the size of the vararg list.
  options_t options = POINTERS_UNWRAPPED | NO_STACK_CHECKS;
  // Also, tell gprintf() not to do whitelist checks if the va_list isn't
  // registered.
  if (!vaListFound)
    options |= NO_WLIST_CHECKS;

  // Set up the output to the passed buffer.
  output_parameter p;
  p.output_kind = output_parameter::OUTPUT_TO_STRING;

  // Wrap the passed buffer in a pointer_info structure.
  pointer_info str_info;
  create_wrapper(str_info, str, strPool, ARG1_COMPLETE(complete));
  p.output.string.info = &str_info;
  p.output.string.string = str;
  p.output.string.pos = 0;
  // Get the bounds of the buffer.
  find_object(cinfo, &str_info);
  // If the bounds have been found, use this value to set the limit for the
  // "safe" number of bytes to write.
  if (str_info.flags & HAVEBOUNDS) {
    p.output.string.maxsz = byte_range((void *) str, str_info.bounds[1]) - 1;
  }
  else // Otherwise assume maximum buffer length.
    p.output.string.maxsz = SIZE_MAX;
  // User didn't impose a limit on the number of bytes to write.
  p.output.string.n = SIZE_MAX;

  // Wrap the format string in a pointer_info structure.
  pointer_info fmt_info;
  create_wrapper(fmt_info, fmt, fmtPool, ARG2_COMPLETE(complete));

  // Call the printing function.
  int result = gprintf(options, p, *cinfo, fmt_info, ap);

  // Free the allocated call_info structure.
  free(cinfo);

  // Add the terminator byte (internal_printf() doesn't do this automatically).
  p.output.string.string[p.output.string.pos] = '\0';

  return result;
}

//
// pool_vsnprintf()
//
// See pool_vsnprintf_debug().
//
int pool_vsnprintf(DebugPoolTy *sPool,
                   DebugPoolTy *fPool,
                   char *s,
                   char *f,
                   size_t n,
                   va_list ap,
                   bv_t c) {
  return pool_vsnprintf_debug(sPool, fPool, s, f, n, ap, c, DEFAULTS);
}

//
// Secure runtime wrapper function to replace vsnprintf()
//
// Inputs
//   strPool  - pool for output character buffer
//   fmtPool  - pool for format string
//   str      - pointer to output character buffer
//   fmt      - format string
//   n        - maximum number of bytes to write into buffer
//   ap       - list of arguments to vnsprintf()
//   complete - completeness bit vector
//   TAG      - tag information for debugging purposes
//   SRC_INFO - source and line number information for debugging purposes
//
// Returns
//   This function returns the number of bytes written to str, or a negative
//   number on error.
//
int pool_vsnprintf_debug(DebugPoolTy *strPool,
                         DebugPoolTy *fmtPool,
                         char *str,
                         char *fmt,
                         size_t n,
                         va_list ap,
                         bv_t complete,
                         TAG,
                         SRC_INFO) {
  // Create the call_info structure associated with this call.
  call_info *cinfo;
  bool vaListFound = build_call_info(cinfo, ap, tag, SRC_INFO_ARGS);
  // On error creating the structure, just print with no runtime checks.
  if (cinfo == 0)
    return vsnprintf(str, n, fmt, ap);

  // Tell the gprintf() function that a) pointers are unwrapped and b) we
  // don't track the size of the vararg list.
  options_t options = POINTERS_UNWRAPPED | NO_STACK_CHECKS;
  // Also, tell gprintf() not to do whitelist checks if the va_list isn't
  // registered.
  if (!vaListFound)
    options |= NO_WLIST_CHECKS;

  // Wrap the passed buffer in a pointer_info structure.
  pointer_info str_info;
  create_wrapper(str_info, str, strPool, ARG1_COMPLETE(complete));

  // Set up the output to the passed buffer.
  output_parameter p;
  p.output_kind = output_parameter::OUTPUT_TO_STRING;
  p.output.string.info = &str_info;
  p.output.string.string = str;
  p.output.string.pos = 0;
  // Get the bounds of the buffer.
  find_object(cinfo, &str_info);
  // Use any found bounds to set the "safe" length for writing.
  if (str_info.flags & HAVEBOUNDS)
    p.output.string.maxsz = byte_range((void *) str, str_info.bounds[1]) - 1;
  else // Otherwise assume maximum buffer length.
    p.output.string.maxsz = SIZE_MAX;
  // Add the user-imposed size limitation.
  if (n > 0)
    p.output.string.n = n - 1;
  else
    p.output.string.n = 0; // Don't write anything when n = 0.

  // Wrap the format string in a pointer_info structure.
  pointer_info fmt_info;
  create_wrapper(fmt_info, fmt, fmtPool, ARG2_COMPLETE(complete));

  // Call the printing function.
  int result = gprintf(options, p, *cinfo, fmt_info, ap);

  // Free the allocated call_info structure.
  free(cinfo);

  // Add the terminator byte (internal_printf() doesn't do this automatically).
  // Only add it if n > 0. When n = 0, nothing is written.
  if (n > 0)
    p.output.string.string[p.output.string.pos] = '\0';

  return result;
}

//
// pool_vscanf()
//
// See pool_vscanf_debug().
//
int pool_vscanf(DebugPoolTy *fmtPool,
                char *fmt,
                va_list ap,
                bv_t complete) {
  return pool_vscanf_debug(fmtPool, fmt, ap, complete, DEFAULTS);
}

//
// Secure runtime wrapper function to replace vscanf()
//
int pool_vscanf_debug(DebugPoolTy *fmtPool,
                      char *fmt,
                      va_list ap,
                      bv_t complete,
                      TAG,
                      SRC_INFO) {
  // Initialize the call_info structure.
  call_info *cinfo;
  bool vaListFound = build_call_info(cinfo, ap, tag, SRC_INFO_ARGS);
  if (cinfo == 0) // Error creating the structure.
    return vscanf(fmt, ap);

  // Create the options. Tell gscanf a) pointers will be unwrapped and
  // b) not to check for reading off the end of the va_list.
  options_t options = POINTERS_UNWRAPPED | NO_STACK_CHECKS;
  // If the va_list was not found to be registered, also tell gscanf() not to
  // check the whitelist.
  if (!vaListFound)
    options |= NO_WLIST_CHECKS;

  // Create the structure describing the input source.
  input_parameter input;
  input.input_kind = input_parameter::INPUT_FROM_STREAM;
  input.input.stream.stream = stdin;

  // Wrap the format string into a pointer_info structure.
  pointer_info fmt_info;
  create_wrapper(fmt_info, fmt, fmtPool, ARG1_COMPLETE(complete));

  // Lock stdin and call gscanf().
  flockfile(stdin);
  int result = gscanf(options, input, *cinfo, fmt_info, ap);
  funlockfile(stdin);

  // Free the allocated call_info structure.
  free(cinfo);

  return result;
}

//
// pool_vsscanf()
//
// See pool_vsscanf_debug().
//
int pool_vsscanf(DebugPoolTy *strPool,
                 DebugPoolTy *fmtPool,
                 char *str,
                 char *fmt,
                 va_list ap,
                 bv_t complete) {
  return pool_vsscanf_debug(strPool, fmtPool, str, fmt, ap, complete, DEFAULTS);
}

//
// Secure runtime wrapper function to replace vsscanf()
//
int pool_vsscanf_debug(DebugPoolTy *strPool,
                       DebugPoolTy *fmtPool,
                       char *str,
                       char *fmt,
                       va_list ap,
                       bv_t complete,
                       TAG,
                       SRC_INFO) {
  // Check if the input string is terminated within object boundaries.
  const bool strComplete = ARG1_COMPLETE(complete);
  validStringCheck(str, strPool, strComplete, "vsscanf", SRC_INFO_ARGS);

  // Initialize the call_info structure.
  call_info *cinfo;
  bool vaListFound = build_call_info(cinfo, ap, tag, SRC_INFO_ARGS);
  if (cinfo == 0) // Error creating the structure.
    return vsscanf(str, fmt, ap);

  // Create the options. Tell gscanf() a) pointers will be unwrapped and
  // b) not to check for reading off the end of the va_list.
  options_t options = POINTERS_UNWRAPPED | NO_STACK_CHECKS;
  // If the va_list was not found to be registered, also tell gscanf() not to
  // check the whitelist.
  if (!vaListFound)
    options |= NO_WLIST_CHECKS;

  // Create the structure describing the input source.
  input_parameter input;
  input.input_kind = input_parameter::INPUT_FROM_STRING;
  input.input.string.string = str;
  input.input.string.pos = 0;

  // Wrap the format string into a pointer_info structure.
  pointer_info fmt_info;
  create_wrapper(fmt_info, fmt, fmtPool, ARG2_COMPLETE(complete));

  int result = gscanf(options, input, *cinfo, fmt_info, ap);

  // Free the allocated call_info structure.
  free(cinfo);

  return result;
}

//
// pool_vfscanf()
//
// See pool_vfscanf_debug().
//
int pool_vfscanf(DebugPoolTy *fPool,
                 DebugPoolTy *fmtPool,
                 void *f,
                 char *fmt,
                 va_list ap,
                 bv_t complete) {
  return pool_vfscanf_debug(fPool, fmtPool, f, fmt, ap, complete, DEFAULTS);
}

//
// Secure runtime wrapper function to replace vfscanf()
//
int pool_vfscanf_debug(DebugPoolTy *filPool,
                       DebugPoolTy *fmtPool,
                       void *fil,
                       char *fmt,
                       va_list ap,
                       bv_t complete,
                       TAG,
                       SRC_INFO) {
  // Initialize the call_info structure.
  call_info *cinfo;
  bool vaListFound = build_call_info(cinfo, ap, tag, SRC_INFO_ARGS);
  if (cinfo == 0) // Error creating the structure.
    return vfscanf((FILE *) fil, fmt, ap);

  // Create the options. Tell gscanf a) pointers will be unwrapped and
  // b) not to check for reading off the end of the va_list.
  options_t options = POINTERS_UNWRAPPED | NO_STACK_CHECKS;
  // If the va_list was not found to be registered, also tell gscanf() not to
  // check the whitelist.
  if (!vaListFound)
    options |= NO_WLIST_CHECKS;

  // Create the structure describing the input source.
  input_parameter input;
  input.input_kind = input_parameter::INPUT_FROM_STREAM;
  input.input.stream.stream = (FILE *) fil;

  // Wrap the format string into a pointer_info structure.
  pointer_info fmt_info;
  create_wrapper(fmt_info, fmt, fmtPool, ARG1_COMPLETE(complete));

  // Lock input file and call gscanf().
  flockfile((FILE *) fil);
  int result = gscanf(options, input, *cinfo, fmt_info, ap);
  funlockfile((FILE *) fil);

  // Free the allocated call_info structure.
  free(cinfo);

  return result;
}

//
// pool_vsyslog()
//
// See pool_vsyslog_debug().
//
void pool_vsyslog(DebugPoolTy *fmtPool,
                  char *fmt,
                  int pri,
                  va_list ap,
                  bv_t complete) {
  pool_vsyslog_debug(fmtPool, fmt, pri, ap, complete, DEFAULTS);
}

//
// Secure runtime wrapper function to replace vsyslog()
//
// Inputs
//   fmtPool  - pool for format string
//   fmt      - format string
//   priority - syslog priority
//   ap       - list of arguments to vsyslog()
//   complete - completeness bit vector
//   TAG      - tag information for debugging purposes
//   SRC_INFO - source and line number information for debugging purposes
//
// Returns
//   This function returns the number of bytes written to fil, or a negative
//   number on error.
//
void pool_vsyslog_debug(DebugPoolTy *fmtPool,
                        char *fmt,
                        int priority,
                        va_list ap,
                        bv_t complete,
                        TAG,
                        SRC_INFO) {
  // Create the call_info structure associated with this call.
  call_info *cinfo;
  bool vaListFound = build_call_info(cinfo, ap, tag, SRC_INFO_ARGS);
  // On error creating the structure, just print without runtime checks.
  if (cinfo == 0) {
    vsyslog(priority, fmt, ap);
    return;
  }

  // Tell the gprintf() function that a) pointers are unwrapped, b) we don't
  // track the size of the vararg list, and c) use the %m directive.
  options_t options = POINTERS_UNWRAPPED | NO_STACK_CHECKS | USE_M_DIRECTIVE;
  // Also, tell gprintf() not to do whitelist checks if the va_list isn't
  // registered.
  if (!vaListFound)
    options |= NO_WLIST_CHECKS;

  // Set up the output information to output to an allocated string.
  const size_t INITIAL_ALLOC_SIZE = 64;
  output_parameter p;
  p.output_kind = output_parameter::OUTPUT_TO_ALLOCATED_STRING;
  p.output.alloced_string.bufsz = INITIAL_ALLOC_SIZE;
  p.output.alloced_string.pos   = 0;
  p.output.alloced_string.string = (char *) malloc(INITIAL_ALLOC_SIZE);
  // On malloc() error, attempt to print without runtime checks.
  if (p.output.alloced_string.string == 0) {
    free(cinfo);
    vsyslog(priority, fmt, ap);
    return;
  }

  // Wrap the format string in a pointer_info structure.
  pointer_info fmt_info;
  create_wrapper(fmt_info, fmt, fmtPool, ARG1_COMPLETE(complete));

  // Call the printing function.
  int sz = gprintf(options, p, *cinfo, fmt_info, ap);

  // Free the allocated call_info structure.
  free(cinfo);

  // Print the resulting string using syslog(), if there was no error in making
  // it.
  if (sz < 0)
    syslog(priority, "SAFECode: error building message!");
  else
    syslog(priority, "%.*s", sz, p.output.alloced_string.string);
  free(p.output.alloced_string.string);
  return;
}
