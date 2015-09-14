//===-------- String.cpp - Secure C standard string library calls ---------===//
//
//                          The SAFECode Compiler
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass finds all calls to functions in the C standard string library and
// transforms them to a more secure form.
//
//===----------------------------------------------------------------------===//

//
// To add a new function to the CStdLib checks, the following modifications are
// necessary:
// 
// In SAFECode:
//
//   - Add the pool_* prototype of the function to
//     runtime/include/CStdLibSupport.h.
//
//   - Implement the pool_* version of the function in the relevant file in
//     runtime/DebugRuntime.
//
//   - Add debug instrumentation information to
//     lib/DebugInstrumentation/DebugInstrumentation.cpp.
//
//   - Update the StringTransform pass to transform calls of the library
//     function into its pool_* version in lib/CStdLib/String.cpp.
//
// In poolalloc:
//
//   - Add an entry for the pool_* version of the function containing the
//     number of initial pool arguments to the structure in
//     include/poolalloc/RuntimeChecks.h.
//
//   - Add an entry to lib/DSA/StdLibPass.cpp for the pool_* version of the
//     function to allow DSA to recognize it.
//

#define DEBUG_TYPE "safecode-string"

#include "safecode/CStdLib.h"
#include "safecode/Config/config.h"
#include "safecode/Utility.h"

#include <cstdarg>
#include <string>

using std::string;

using namespace llvm;

namespace llvm {

// Identifier variable for the pass
char StringTransform::ID = 0;

// Statistics counters
#define ADD_STATISTIC_FOR(func) \
  STATISTIC(st_xform_ ## func, "Total " #func "() calls transformed")

ADD_STATISTIC_FOR(vprintf);
ADD_STATISTIC_FOR(vfprintf);
ADD_STATISTIC_FOR(vsprintf);
ADD_STATISTIC_FOR(vsnprintf);
ADD_STATISTIC_FOR(vscanf);
ADD_STATISTIC_FOR(vsscanf);
ADD_STATISTIC_FOR(vfscanf);
ADD_STATISTIC_FOR(vsyslog);
ADD_STATISTIC_FOR(memccpy);
ADD_STATISTIC_FOR(memchr);
ADD_STATISTIC_FOR(memcmp);
ADD_STATISTIC_FOR(memcpy);
ADD_STATISTIC_FOR(memmove);
ADD_STATISTIC_FOR(memset);
ADD_STATISTIC_FOR(strcat);
ADD_STATISTIC_FOR(strchr);
ADD_STATISTIC_FOR(strcmp);
ADD_STATISTIC_FOR(strcoll);
ADD_STATISTIC_FOR(strcpy);
ADD_STATISTIC_FOR(strcspn);
ADD_STATISTIC_FOR(strlen);
ADD_STATISTIC_FOR(strncat);
ADD_STATISTIC_FOR(strncmp);
ADD_STATISTIC_FOR(strncpy);
ADD_STATISTIC_FOR(strpbrk);
ADD_STATISTIC_FOR(strrchr);
ADD_STATISTIC_FOR(strspn);
ADD_STATISTIC_FOR(strstr);
ADD_STATISTIC_FOR(strxfrm);
ADD_STATISTIC_FOR(bcmp);
ADD_STATISTIC_FOR(bcopy);
ADD_STATISTIC_FOR(bzero);
ADD_STATISTIC_FOR(index);
ADD_STATISTIC_FOR(rindex);
ADD_STATISTIC_FOR(strcasecmp);
ADD_STATISTIC_FOR(strncasecmp);
ADD_STATISTIC_FOR(fgets);
ADD_STATISTIC_FOR(fputs);
ADD_STATISTIC_FOR(fwrite);
ADD_STATISTIC_FOR(fread);
ADD_STATISTIC_FOR(gets);
ADD_STATISTIC_FOR(puts);
ADD_STATISTIC_FOR(tmpnam);
ADD_STATISTIC_FOR(read);
ADD_STATISTIC_FOR(recv);
ADD_STATISTIC_FOR(recvfrom);
ADD_STATISTIC_FOR(write);
ADD_STATISTIC_FOR(send);
ADD_STATISTIC_FOR(sendto);
ADD_STATISTIC_FOR(readdir_r);
ADD_STATISTIC_FOR(readlink);
ADD_STATISTIC_FOR(realpath);
ADD_STATISTIC_FOR(getcwd);

#ifdef HAVE_MEMPCPY
ADD_STATISTIC_FOR(mempcpy);
#endif
#ifdef HAVE_STRCASESTR
ADD_STATISTIC_FOR(strcasestr);
#endif
#ifdef HAVE_STPCPY
ADD_STATISTIC_FOR(stpcpy);
#endif
#ifdef HAVE_STRNLEN
ADD_STATISTIC_FOR(strnlen);
#endif

STATISTIC(NumStringChecks, "Number of calls to poolcheckstr() added");

//
// Functions that aren't handled (yet...):
//  - stpncpy and __stpncpy_chk
//  - setbuf
//  - setvbuf
//  - strtok() family
//

static RegisterPass<StringTransform>
ST("string_transform", "Secure C standard string library calls");

//
// Function: addStringCheck()
//
// Description:
//  This function will add a run-time check for the specified argument for
//  calls to the specified function.
//
// Inputs:
//  M - The module to modify
//  name - The name of the function which uses a string input.
//  argNo - The argument of the function which is a string.
//
static bool
addStringCheck (Module & M, const std::string & name, unsigned argNo) {
  //
  // Get the function that uses a string.  If it is not used within the
  // program, do nothing.
  //
  Function * F = M.getFunction (name);
  if (!F) return false;

  //
  // Get the type expected for string arguments.
  //
  Type * Int8PtrTy = Type::getInt8PtrTy(M.getContext());

  //
  // Don't instrument calls to the function if it is defined in this program.
  //
  if (!(F->isDeclaration()))
    return false;

  //
  // Scan through the module for uses of the function to instrument.
  //
  std::vector<CallSite *> callsToInstrument;
  for (Value::use_iterator UI = F->use_begin(), UE = F->use_end();
       UI != UE;
       ++UI) {
    // Ensure the use is an instruction and that the instruction calls the
    // source function (as opposed to passing it as a parameter or other
    // possible uses).
    CallSite CS (*UI);
    if (!CS || CS.getCalledValue() != F)
      continue;
    callsToInstrument.push_back(&CS);
  }

  //
  // Keep track of changes to the module.
  //
  bool changed = false;

  //
  // Go through all of the calls and instrument them.
  //
  Function * strCheck = cast<Function>(M.getFunction ("poolcheckstrui"));
  for (unsigned index = 0; index < callsToInstrument.size(); ++index) {
    CallSite CS = *callsToInstrument[index];

    //
    // Ignore this call site when the specified argument doesn't appear to
    // exist or is of the wrong type.
    //
    if (CS.arg_size() < argNo + 1 ||
        CS.getArgument(argNo)->getType() != Int8PtrTy)
      continue;

    //
    // Setup the arguments for the run-time check.  The first is the NULL
    // pool handle.  The second is the argument from the actual function call.
    //
    std::vector<Value *> Params;
    PointerType *VoidPtrTy = IntegerType::getInt8PtrTy(M.getContext());
    Params.push_back (ConstantPointerNull::get(VoidPtrTy));
    Params.push_back (CS.getArgument (argNo));

    //
    // Create a call instruction to the string run-time check.
    //
    CallInst *CallToStringCheck
      = CallInst::Create (strCheck, Params, "", CS.getInstruction());

    //
    // If the original call site has debug metadata, associate the metadata
    // with the newly created call.
    //
    if (MDNode *DebugNode = CS.getInstruction()->getMetadata("dbg"))
      CallToStringCheck->setMetadata("dbg", DebugNode);

    //
    // mark that we modified something in the module and also update the 
    // string check statistic.
    //
    changed = true;
    ++NumStringChecks;
  }

  return changed;
}

//
// Entry point for the LLVM pass that transforms C standard string library calls
//
bool
StringTransform::runOnModule (Module & M) {
  // Flags whether we modified the module.
  bool chgd = false;

  tdata = &M.getDataLayout();

  // Create needed pointer types (char * == i8 * == VoidPtrTy).
  Type *VoidPtrTy = IntegerType::getInt8PtrTy(M.getContext());
  // Determine the type of size_t for functions that return this result.
  Type *SizeTTy = tdata->getIntPtrType(M.getContext(), 0);
  Type *SSizeTTy = SizeTTy;
  // Create other return types (int, void).
  Type *Int32Ty = IntegerType::getInt32Ty(M.getContext());
  Type *VoidTy  = Type::getVoidTy(M.getContext());

  //
  // Add a function that will perform a basic check upon string inputs.
  //
  std::vector<Type *> ParamTypes;
  ParamTypes.push_back (VoidPtrTy);
  ParamTypes.push_back (VoidPtrTy);
  FunctionType *FT = FunctionType::get(VoidPtrTy, ParamTypes, false);
  M.getOrInsertFunction ("poolcheckstrui", FT);

  //
  // Add basic checks on strings which are read by their C library functions.
  //
  chgd |= addStringCheck (M, "access", 0);
  chgd |= addStringCheck (M, "chdir", 0);
  chgd |= addStringCheck (M, "chmod", 0);
  chgd |= addStringCheck (M, "chown", 0);
  chgd |= addStringCheck (M, "creat", 0);
  chgd |= addStringCheck (M, "dlopen", 0);
  chgd |= addStringCheck (M, "fattach", 1);
  chgd |= addStringCheck (M, "fchmodat", 1);
  chgd |= addStringCheck (M, "fdopen", 1);
  chgd |= addStringCheck (M, "fopen", 0);
  chgd |= addStringCheck (M, "\01_fopen", 0);
  chgd |= addStringCheck (M, "freopen", 0);
  chgd |= addStringCheck (M, "fstatat", 1);
  chgd |= addStringCheck (M, "ftok", 0);
  chgd |= addStringCheck (M, "ftw", 0);
  chgd |= addStringCheck (M, "getaddrinfo", 0);
  chgd |= addStringCheck (M, "getenv", 0);
  chgd |= addStringCheck (M, "gethostbyname", 0);
  chgd |= addStringCheck (M, "lchmod", 0);
  chgd |= addStringCheck (M, "lchown", 0);
  chgd |= addStringCheck (M, "link", 0);
  chgd |= addStringCheck (M, "link", 1);
  chgd |= addStringCheck (M, "linkat", 1);
  chgd |= addStringCheck (M, "linkat", 3);
  chgd |= addStringCheck (M, "lstat", 0);
  chgd |= addStringCheck (M, "mkdir", 0);
  chgd |= addStringCheck (M, "mkdirat", 1);
  chgd |= addStringCheck (M, "mkfifo", 0);
  chgd |= addStringCheck (M, "mkfifoat", 1);
  chgd |= addStringCheck (M, "mknod", 0);
  chgd |= addStringCheck (M, "mknodat", 1);
  chgd |= addStringCheck (M, "mount", 0);
  chgd |= addStringCheck (M, "mount", 1);
  chgd |= addStringCheck (M, "mount", 2);
  chgd |= addStringCheck (M, "open", 0);
  chgd |= addStringCheck (M, "openat", 1);
  chgd |= addStringCheck (M, "openlog", 0);
  chgd |= addStringCheck (M, "popen", 0);
  chgd |= addStringCheck (M, "putenv", 0);
  chgd |= addStringCheck (M, "remove", 0);
  chgd |= addStringCheck (M, "rename", 0);
  chgd |= addStringCheck (M, "rename", 1);
  chgd |= addStringCheck (M, "renameat", 1);
  chgd |= addStringCheck (M, "renameat", 3);
  chgd |= addStringCheck (M, "rmdir", 0);
  chgd |= addStringCheck (M, "setenv", 0);
  chgd |= addStringCheck (M, "shm_open", 0);
  chgd |= addStringCheck (M, "shm_unlink", 0);
  chgd |= addStringCheck (M, "stat", 0);
  chgd |= addStringCheck (M, "statvfs", 0);
  chgd |= addStringCheck (M, "symlink", 0);
  chgd |= addStringCheck (M, "symlink", 1);
  chgd |= addStringCheck (M, "system", 0);
  chgd |= addStringCheck (M, "tempnam", 0);
  chgd |= addStringCheck (M, "tempnam", 1);
  chgd |= addStringCheck (M, "truncate", 0);
  chgd |= addStringCheck (M, "unlink", 0);
  chgd |= addStringCheck (M, "unsetenv", 0);
  chgd |= addStringCheck (M, "utime", 0);
  chgd |= addStringCheck (M, "utimensat", 1);
  chgd |= addStringCheck (M, "utimes", 0);

  //
  // Handle 64-bit versions of these functions that may exist on hybrid 32/64
  // bit systems.
  //
  chgd |= addStringCheck (M, "access64", 0);
  chgd |= addStringCheck (M, "chdir64", 0);
  chgd |= addStringCheck (M, "chmod64", 0);
  chgd |= addStringCheck (M, "chown64", 0);
  chgd |= addStringCheck (M, "creat64", 0);
  chgd |= addStringCheck (M, "fopen64", 0);
  chgd |= addStringCheck (M, "lchmod64", 0);
  chgd |= addStringCheck (M, "lchown64", 0);
  chgd |= addStringCheck (M, "link64", 0);
  chgd |= addStringCheck (M, "link64", 1);
  chgd |= addStringCheck (M, "lstat64", 0);
  chgd |= addStringCheck (M, "mkdir64", 0);
  chgd |= addStringCheck (M, "mkfifo64", 0);
  chgd |= addStringCheck (M, "mknod64", 0);
  chgd |= addStringCheck (M, "open64", 0);
  chgd |= addStringCheck (M, "openat64", 1);
  chgd |= addStringCheck (M, "remove64", 0);
  chgd |= addStringCheck (M, "rename64", 0);
  chgd |= addStringCheck (M, "rename64", 1);
  chgd |= addStringCheck (M, "rmdir64", 0);
  chgd |= addStringCheck (M, "stat64", 0);
  chgd |= addStringCheck (M, "symlink64", 0);
  chgd |= addStringCheck (M, "symlink64", 1);
  chgd |= addStringCheck (M, "unlink64", 0);

  //
  // exec() family (note only partial support; we only check the first
  // argument).
  //
  chgd |= addStringCheck (M, "execl", 0);
  chgd |= addStringCheck (M, "execlp", 0);
  chgd |= addStringCheck (M, "execle", 0);
  chgd |= addStringCheck (M, "execv", 0);
  chgd |= addStringCheck (M, "execvp", 0);

  // Functions from <stdio.h>, <syslog.h>
  chgd |= transform(M, "vprintf",  2, 1, Int32Ty, st_xform_vprintf);
  chgd |= transform(M, "vfprintf", 3, 2, Int32Ty, st_xform_vfprintf);
  chgd |= transform(M, "vsprintf", 3, 2, Int32Ty, st_xform_vsprintf);
  chgd |= transform(M, "vscanf",   2, 1, Int32Ty, st_xform_vscanf);
  chgd |= transform(M, "vsscanf",  3, 2, Int32Ty, st_xform_vsscanf);
  chgd |= transform(M, "vfscanf",  3, 2, Int32Ty, st_xform_vfscanf);
  SourceFunction VSNPf   = { "vsnprintf", Int32Ty, 4 };
  SourceFunction VSyslog = { "vsyslog", VoidTy, 3 };
  DestFunction PVSNPf   = { "pool_vsnprintf", 4, 2 };
  DestFunction PVSyslog = { "pool_vsyslog", 3, 1 };
  // Note that we need to switch the order of arguments to the instrumented
  // calls of vsnprintf() and vsyslog(), because the CStdLib pass convention is
  // to place all the interesting pointer arguments at the start of the
  // parameter list, but these functions have initial arguments that are
  // non-pointers.
  chgd |= vtransform(M, VSNPf, PVSNPf, st_xform_vsnprintf, 1u, 3u, 2u, 4u);
  chgd |= vtransform(M, VSyslog, PVSyslog, st_xform_vsyslog, 2u, 1u, 3u);
  // __isoc99_vscanf family: these are found in glibc
  SourceFunction IsoC99Vscanf  = { "__isoc99_vscanf", Int32Ty, 2 };
  SourceFunction IsoC99Vsscanf = { "__isoc99_vsscanf", Int32Ty, 3 };
  SourceFunction IsoC99Vfscanf = { "__isoc99_vfscanf", Int32Ty, 3 };
  DestFunction PVscanf  = { "pool_vscanf", 2, 1 };
  DestFunction PVsscanf = { "pool_vsscanf", 3, 2 };
  DestFunction PVfscanf = { "pool_vfscanf", 3, 2 };
  chgd |= vtransform(M, IsoC99Vscanf, PVscanf, st_xform_vscanf, 1u, 2u);
  chgd |= vtransform(M, IsoC99Vsscanf, PVsscanf, st_xform_vsscanf, 1u, 2u);
  chgd |= vtransform(M, IsoC99Vfscanf, PVfscanf, st_xform_vfscanf, 1u, 2u);
  // __vsprintf_chk, __vsnprintf_chk
  SourceFunction VSPfC  = { "__vsprintf_chk", Int32Ty, 5 };
  SourceFunction VSNPfC = { "__vsnprintf_chk", Int32Ty, 6 };
  DestFunction PVSPf = { "pool_vsprintf", 3, 2 };
  chgd |= vtransform(M, VSPfC, PVSPf, st_xform_vsprintf, 1u, 4u, 5u);
  chgd |= vtransform(M, VSNPfC, PVSNPf, st_xform_vsnprintf, 1u, 5u, 2u, 6u);
  // Functions from <string.h>
  chgd |= transform(M, "memccpy", 4, 2, VoidPtrTy, st_xform_memccpy);
  chgd |= transform(M, "memchr",  3, 1, VoidPtrTy, st_xform_memchr);
  chgd |= transform(M, "memcmp",  3, 2, Int32Ty,   st_xform_memcmp);
  chgd |= transform(M, "memcpy",  3, 2, Int32Ty,   st_xform_memcpy);
  chgd |= transform(M, "memmove", 3, 2, VoidPtrTy, st_xform_memmove);
  chgd |= transform(M, "memset",  2, 1, VoidPtrTy, st_xform_memset);
  chgd |= transform(M, "strcat",  2, 2, VoidPtrTy, st_xform_strcat);
  chgd |= transform(M, "strchr",  2, 1, VoidPtrTy, st_xform_strchr);
  chgd |= transform(M, "strcmp",  2, 2, Int32Ty,   st_xform_strcmp);
  chgd |= transform(M, "strcoll", 2, 2, Int32Ty,   st_xform_strcoll);
  chgd |= transform(M, "strcpy",  2, 2, VoidPtrTy, st_xform_strcpy);
  chgd |= transform(M, "strcspn", 2, 2, SizeTTy,   st_xform_strcspn);
  // chgd |= handle_strerror_r(M);
  chgd |= transform(M, "strlen",  1, 1, SizeTTy,   st_xform_strlen);
  chgd |= transform(M, "strncat", 3, 2, VoidPtrTy, st_xform_strncat);
  chgd |= transform(M, "strncmp", 3, 2, Int32Ty,   st_xform_strncmp);
  chgd |= transform(M, "strncpy", 3, 2, VoidPtrTy, st_xform_strncpy);
  chgd |= transform(M, "strpbrk", 2, 2, VoidPtrTy, st_xform_strpbrk);
  chgd |= transform(M, "strrchr", 2, 1, VoidPtrTy, st_xform_strrchr);
  chgd |= transform(M, "strspn",  2, 2, SizeTTy,   st_xform_strspn);
  chgd |= transform(M, "strstr",  2, 2, VoidPtrTy, st_xform_strstr);
  chgd |= transform(M, "strxfrm", 3, 2, SizeTTy,   st_xform_strxfrm);
  // Extensions to <string.h>
#ifdef HAVE_MEMPCPY
  chgd |= transform(M, "mempcpy", 3, 2, VoidPtrTy, st_xform_mempcpy);
#endif
#ifdef HAVE_STRCASESTR
  chgd |= transform(M, "strcasestr", 2, 2, VoidPtrTy, st_xform_strcasestr);
#endif
#ifdef HAVE_STPCPY
  chgd |= transform(M, "stpcpy",  2, 2, VoidPtrTy, st_xform_stpcpy);
#endif
#ifdef HAVE_STRNLEN
  chgd |= transform(M, "strnlen", 2, 1, SizeTTy,   st_xform_strnlen);
#endif
  // Functions from <strings.h>
  chgd |= transform(M, "bcmp",    3, 2, Int32Ty,   st_xform_bcmp);
  chgd |= transform(M, "bcopy",   3, 2, VoidTy,    st_xform_bcopy);
  chgd |= transform(M, "bzero",   2, 1, VoidTy,    st_xform_bzero);
  chgd |= transform(M, "index",   2, 1, VoidPtrTy, st_xform_index);
  chgd |= transform(M, "rindex",  2, 1, VoidPtrTy, st_xform_rindex);
  chgd |= transform(M, "strcasecmp", 2, 2, Int32Ty, st_xform_strcasecmp);
  chgd |= transform(M, "strncasecmp", 3, 2, Int32Ty, st_xform_strncasecmp);
  // Darwin-specific secure extensions to <string.h>
  SourceFunction MemcpyChk  = { "__memcpy_chk", VoidPtrTy, 4 };
  SourceFunction MemmoveChk = { "__memmove_chk", VoidPtrTy, 4 };
  SourceFunction MemsetChk  = { "__memset_chk", VoidPtrTy, 4 };
  SourceFunction StrcpyChk  = { "__strcpy_chk", VoidPtrTy, 3 };
  SourceFunction StrcatChk  = { "__strcat_chk", VoidPtrTy, 3 };
  SourceFunction StrncatChk = { "__strncat_chk", VoidPtrTy, 4 };
  SourceFunction StrncpyChk = { "__strncpy_chk", VoidPtrTy, 4 };
  DestFunction PoolMemcpy  = { "pool_memcpy", 3, 2 };
  DestFunction PoolMemmove = { "pool_memmove", 3, 2 };
  DestFunction PoolMemset  = { "pool_memset", 3, 1 };
  DestFunction PoolStrcpy  = { "pool_strcpy", 2, 2 };
  DestFunction PoolStrcat  = { "pool_strcat", 2, 2 };
  DestFunction PoolStrncat = { "pool_strncat", 3, 2 };
  DestFunction PoolStrncpy = { "pool_strncpy", 3, 2 };
  chgd |= vtransform(M, MemcpyChk, PoolMemcpy, st_xform_memcpy, 1u, 2u, 3u);
  chgd |= vtransform(M, MemmoveChk, PoolMemmove, st_xform_memmove, 1u, 2u, 3u);
  chgd |= vtransform(M, MemsetChk, PoolMemset, st_xform_memset, 1u, 2u, 3u);
  chgd |= vtransform(M, StrcpyChk, PoolStrcpy, st_xform_strcpy, 1u, 2u);
  chgd |= vtransform(M, StrcatChk, PoolStrcat, st_xform_strcat, 1u, 2u);
  chgd |= vtransform(M, StrncatChk, PoolStrncat, st_xform_strncat, 1u, 2u, 3u);
  chgd |= vtransform(M, StrncpyChk, PoolStrncpy, st_xform_strncpy, 1, 2u, 3u);
#ifdef HAVE_STPCPY
  SourceFunction StpcpyChk = { "__stpcpy_chk", VoidPtrTy, 3 };
  DestFunction PoolStpcpy = { "pool_stpcpy", 2, 2 };
  chgd |= vtransform(M, StpcpyChk, PoolStpcpy, st_xform_stpcpy, 1u, 2u);
#endif

  // Functions from <stdio.h> 
  chgd |= transform(M, "fgets", 3, 1, VoidPtrTy, st_xform_fgets);
  chgd |= transform(M, "fputs", 2, 1, Int32Ty, st_xform_fputs);
  chgd |= transform(M, "fwrite", 4, 1, SizeTTy, st_xform_fwrite);
  chgd |= transform(M, "fread", 4, 1, SizeTTy, st_xform_fread);
  chgd |= transform(M, "gets", 1, 1, VoidPtrTy, st_xform_gets);
  chgd |= transform(M, "puts", 1, 1, Int32Ty, st_xform_puts);
  chgd |= transform(M, "tmpnam", 1, 1, VoidPtrTy, st_xform_tmpnam);

  // System calls
  SourceFunction Read  = { "read", SSizeTTy, 3 };
  SourceFunction Recv  = { "recv", SSizeTTy, 4 };
  SourceFunction RecvFrom  = { "recvfrom", SSizeTTy, 6 };
  SourceFunction Write = { "write", SSizeTTy, 3 };
  SourceFunction Send  = { "send", SSizeTTy, 4 };
  SourceFunction SendTo = { "sendto", SSizeTTy, 6 };
  DestFunction PoolRead  = { "pool_read", 3, 1 };
  DestFunction PoolRecv  = { "pool_recv", 4, 1 };
  DestFunction PoolRecvFrom = { "pool_recvfrom", 6, 1 };
  DestFunction PoolWrite = { "pool_write", 3, 1 };
  DestFunction PoolSend  = { "pool_send", 4, 1 };
  DestFunction PoolSendTo = { "pool_sendto", 6, 1 };
  chgd |= transform(M, "readlink", 3, 2, SSizeTTy,   st_xform_readlink);
  chgd |= transform(M, "realpath", 2, 2, VoidPtrTy,   st_xform_realpath);
  chgd |= vtransform(M, Read, PoolRead, st_xform_read, 2u, 1u, 3u);
  chgd |= vtransform(M, Recv, PoolRecv, st_xform_recv, 2u, 1u, 3u, 4u);
  chgd |= vtransform(M, Write, PoolWrite, st_xform_write, 2u, 1u, 3u);
  chgd |= vtransform(M, Send, PoolSend, st_xform_send, 2u, 1u, 3u, 4u);
  chgd |= vtransform(
    M, RecvFrom, PoolRecvFrom, st_xform_recvfrom, 2u, 1u, 3u, 4u, 5u, 6u
  );
  chgd |= vtransform(
    M, SendTo, PoolSendTo, st_xform_sendto, 2u, 1u, 3u, 4u, 5u, 6u
  );

  // realpath() on Darwin
  SourceFunction DarwinRealpath = 
    { "\01_realpath$DARWIN_EXTSN", VoidPtrTy, 2 };
  DestFunction PoolRealpath = { "pool_realpath", 2, 2 };
  chgd |= vtransform(
    M, DarwinRealpath, PoolRealpath, st_xform_realpath, 1u, 2u
  );

  // Functions from <dirent.h>
  SourceFunction RdDirR = { "readdir_r", Int32Ty, 3 };
  DestFunction PoolRdDirR = { "pool_readdir_r", 3, 2 };
  chgd |= vtransform(M, RdDirR, PoolRdDirR, st_xform_readdir_r, 2u, 3u, 1u);

  // Functions from <unistd.h>
  chgd |= transform(M, "getcwd", 2, 1, VoidPtrTy, st_xform_getcwd);

  return chgd;
}

//
// Simple wrapper to gtransform() for when
//   1) the transformed function is named "pool_" + original name.
//   2) the order and number of arguments is preserved from the original to the
//      transformed function.
//
// Parameters:
//   M         - the module to scan
//   argc      - the expected number of arguments to the original function
//   pool_argc - the number of initial pool parameters to add to the transformed
//               function
//   ReturnTy  - the expected return type of the original function
//   statistic - a reference to a relevant Statistic for the number of
//               transformation
//
// Returns:
//   This function returns true if the module was modified and false otherwise.
//
bool
StringTransform::transform(Module &M,
                           const StringRef FunctionName,
                           const unsigned argc,
                           const unsigned pool_argc,
                           Type *ReturnTy,
                           Statistic &statistic) {
  SourceFunction src = { FunctionName.data(), ReturnTy, argc };
  string dst_name  = "pool_" + FunctionName.str();
  DestFunction dst = { dst_name.c_str(), argc, pool_argc };
  vector<unsigned> args;
  for (unsigned i = 1; i <= argc; i++)
    args.push_back(i);
  return gtransform(M, src, dst, statistic, args);
}

//
// vtransform() - wrapper to gtransform() that takes variable arguments
// instead of a vector as the final parameter
//
bool
StringTransform::vtransform(Module &M,
                            const SourceFunction &from,
                            const DestFunction &to,
                            Statistic &stat,
                            ...) {
  vector<unsigned> args;
  va_list ap;
  va_start(ap, stat);
  // Read the positions to append as vararg parameters.
  for (unsigned i = 1; i <= to.source_argc; i++) {
    unsigned position = va_arg(ap, unsigned);
    args.push_back(position);
  }
  va_end(ap);
  return gtransform(M, from, to, stat, args);
}

//
// Secures C standard string library calls by transforming them into
// their corresponding runtime wrapper functions.
//
// The 'from' parameter describes a function to transform. It is struct of
// the form
//   struct { const char *name; Type *return_type; unsigned argc };
// where
//   - 'name' is the name of the function to transform
//   - 'return_type' is its expected return type
//   - 'argc' is its expected number of arguments.
//
// The 'to' parameter describes the function to transform into. It is a struct
// of the form
//   struct { const char *name, unsigned source_argc, unsigned pool_argc };
// where
//   - 'name' is the name of the resulting function
//   - 'source_argc' is the number of parameters the function takes from the
//     original function
//   - 'pool_argc' is the number of initial pool parameters to add.
//
// The 'append_order' vector describes how to move the parameters of the
// original function into the transformed function call.
//
// @param M              Module from runOnModule() to scan for functions to
//                       transform.
// @param from           SourceFunction structure reference described above
// @param to             DestFunction structure reference described above.
// @param stat           A reference to the relevant transform statistic.
// @param append_order   A vector describing the order to add the arguments from
//                       the source function into the destination function.
// @return               Returns true if any calls were transformed, and
//                       false if no changes were made.
//
bool
StringTransform::gtransform(Module &M,
                            const SourceFunction &from,
                            const DestFunction &to,
                            Statistic &stat,
                            const vector<unsigned> &append_order) {
  // Get the source function if it exists in the module.
  Function *src = M.getFunction(from.name);
  if (!src)
    return false;
  // Make sure the source function behaves as described, otherwise skip it.
  FunctionType *F_type = src->getFunctionType();
  if (F_type->getReturnType() != from.return_type || F_type->isVarArg() ||
      F_type->getNumParams() != from.argc)
    return false;
  // Make sure the append_order vector has the expected number of elements.
  assert(append_order.size() == to.source_argc &&
    "Unexpected number of parameter positions in vector!");
  // Check that each pool parameter has a corresponding original parameter.
  assert(to.pool_argc <= to.source_argc && "More pools than arguments?");
  // Check if the pool completeness information can be fit into a single 8 bit
  // quantity.
  assert(to.pool_argc <= 8 && "Only up to 8 pool parameters supported!");
  std::vector<Instruction *> toModify;
  std::vector<Instruction *>::iterator modifyIter, modifyEnd;
  // Scan through the module for uses of the function to transform.
  for (Value::use_iterator UI = src->use_begin(), UE = src->use_end();
       UI != UE;
       ++UI) {
    CallSite CS(*UI);
    // Ensure the use is an instruction and that the instruction calls the
    // source function (as opposed to passing it as a parameter or other
    // possible uses).
    if (!CS || CS.getCalledValue() != src)
      continue;
    toModify.push_back(CS.getInstruction());
  }
  // Return early if we've found nothing to modify.
  if (toModify.empty())
    return false;
  // The pool handle type is a void pointer (i8 *).
  PointerType *VoidPtrTy = IntegerType::getInt8PtrTy(M.getContext());
  Type *Int8Ty = IntegerType::getInt8Ty(M.getContext());
  // Build the type of the parameters to the transformed function. This function
  // has to.pool_argc initial arguments of type i8 *.
  std::vector<Type *> ParamTy(to.pool_argc, VoidPtrTy);
  // After the initial pool arguments, parameters from the original function go
  // into the type.
  for (unsigned i = 0; i < to.source_argc; i++) {
    unsigned position = append_order[i];
    assert(0 < position && position <= from.argc && "Parameter out of bounds!");
    Type *ParamType = F_type->getParamType(position - 1);
    if (i < to.pool_argc)
      assert(
        isa<PointerType>(ParamType) && "Pointer type expected for parameter!"
      );
    ParamTy.push_back(ParamType);
  }
  // The completeness bitvector goes at the end.
  ParamTy.push_back(Int8Ty);
  // Build the type of the transformed function.
  FunctionType *FT = FunctionType::get(F_type->getReturnType(), ParamTy, false);
#ifndef NDEBUG
  Function *PoolFInModule = M.getFunction(to.name);
  // Make sure that the function declarations don't conflict.
  assert((PoolFInModule == 0 ||
    PoolFInModule->getFunctionType() == FT ||
    PoolFInModule->hasLocalLinkage()) &&
    "Replacement function already declared with wrong type!");
#endif
  // Build the actual transformed function.
  Constant *PoolF = M.getOrInsertFunction(to.name, FT);
  // This is a placeholder value for the pool handles (to be "filled in" later
  // by poolalloc).
  Value *PH = ConstantPointerNull::get(VoidPtrTy);
  // Transform every valid use of the function that was found.
  for (modifyIter = toModify.begin(), modifyEnd = toModify.end();
       modifyIter != modifyEnd;
       ++modifyIter) {
    Instruction *I = *modifyIter;
    // Construct vector of parameters to transformed function call. Also insert
    // NULL pool handles.
    std::vector<Value *> Params(to.pool_argc, PH);
    // Insert the original parameters.
    for (unsigned i = 0; i < to.source_argc; i++) {
      Value *f = I->getOperand(append_order[i] - 1);
      Params.push_back(f);
    }
    // Add the DSA completeness bitvector. Set it to 0 (= incomplete).
    Params.push_back(ConstantInt::get(Int8Ty, 0));
    // Create the call instruction for the transformed function and insert it
    // before the current instruction.
    CallInst *C = CallInst::Create(PoolF, Params, "", I);
    if (InvokeInst* Invoke = dyn_cast<InvokeInst>(I)) {
      // Our versions don't throw under any circumstances.
      // Just branch to normal:
      BranchInst::Create(Invoke->getNormalDest(), I);
      // Remove any PHIs relying on the removed edge to the unwind BB
      removeInvokeUnwindPHIs(Invoke);
    }
    // Transfer debugging metadata if it exists from the old call into the new
    // one.
    if (MDNode *DebugNode = I->getMetadata("dbg"))
      C->setMetadata("dbg", DebugNode);
    // Replace all uses of the function with its transformed equivalent.
    I->replaceAllUsesWith(C);
    I->eraseFromParent();
    // Record the transformation.
    ++stat;
  }
  // Reaching here means some call has been modified;
  return true;
}

}
