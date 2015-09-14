//===- Report.h - Debugging reports for bugs found by SAFECode ------------===//
// 
//                       The SAFECode Compiler Project
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file implements functions for creating reports for the SAFECode
// run-time.
//
//===----------------------------------------------------------------------===//

#ifndef _REPORT_H_
#define _REPORT_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "safecode/Config/config.h"

#ifdef SC_DEBUGTOOL
#define ABORT_PROGRAM() abort()
#else
//#define ABORT_PROGRAM() *((volatile int*)NULL)
#define ABORT_PROGRAM() __builtin_trap()
#endif

extern FILE * ReportLog;

#ifdef SC_DEBUGTOOL
// Global Configuration Information
extern unsigned StopOnError;

static unsigned alertNum = 0;
static unsigned initialized = 0;

//
// Function: printAlertHeader()
//
// Description:
//  Increment the alert number and print a header for this report message.
//
static unsigned
printAlertHeader (void) {
  //
  // Initialize the reporting code if necessary.  This cannot be done in the
  // SAFECode initialization routines because those need to run before
  // constructors are executed, and the libc stdio functions may not have been
  // initialized at that point.
  //
  if (!initialized) {
    //
    // Open up an additional file for error reporting.
    //
    ReportLog = fopen ("sclogfile", "a");
    fprintf (ReportLog, "\n");
    fprintf (ReportLog, "New Run of Program\n");
    fprintf (ReportLog, "====================================================\n");
    fflush (ReportLog);
    initialized = 1;
  }
  fprintf (ReportLog, "=======+++++++    SAFECODE RUNTIME ALERT #%04d   +++++++=======\n",
          ++alertNum);
  return alertNum;
}

//
// Function: ReportDanglingPointer()
//
// Description:
//  Create a report entry for a dangling pointer error.
//
// Inputs:
//  addr     - The dangling pointer value that was dereferenced.
//  pc       - The program counter of the instruction.
//  allocpc  - The program counter at which the object was last allocated.
//  allocgen - The generation number of the allocation.
//  freepc   - The program counter at which the object was last freed.
//  freegen  - The generation number of the free.
//
static void
ReportDanglingPointer (void * addr,
                       unsigned pc,
                       unsigned allocpc,
                       unsigned allocgen,
                       unsigned freepc,
                       unsigned freegen) {
  // Print the header and get the ID for this report
  unsigned id = printAlertHeader();

  fprintf (ReportLog, "%04d: Dangling pointer access to memory address 0x%p \n", id, addr);
  fprintf (ReportLog, "%04d:                        at program counter 0x%08x\n", id, (unsigned)pc);
  fprintf (ReportLog, "%04d:\tObject allocated at program counter   : 0x%08x \n", id, (unsigned)allocpc);
  fprintf (ReportLog, "%04d:\tObject allocation generation number   : %d \n", id, allocgen);
  fprintf (ReportLog, "%04d:\tObject freed at program counter       : 0x%08x \n", id, freepc);
  fprintf (ReportLog, "%04d:\tObject free generation number         : %d \n", id, freegen);
  fprintf(ReportLog, "=======+++++++    end of runtime error report    +++++++=======\n");
  if (StopOnError) ABORT_PROGRAM();
  return;
}

//
// Function: ReportLoadStoreCheck()
//
// Description:
//  Report a failure on a load or store check.
//
// Inputs:
//  ptr      - The pointer for the failed load/store operation.
//  pc       - The program counter of the failed run-time check.
//
static void
ReportLoadStoreCheck (void * ptr,
                      void * pc,
                      char * SourceFile,
                      unsigned lineno) {
  // Print the header and get the ID for this report
  unsigned id = printAlertHeader();

  fprintf (ReportLog, "%04d: Load/Store violation to memory address %p\n", id, ptr);
  fprintf (ReportLog, "%04d:                     at program counter %p\n", id, pc);
  fprintf (ReportLog, "%04d:\tAddress                : %p \n", id, ptr);
  fprintf (ReportLog, "%04d:\tSource filename        : %s \n", id, SourceFile);
  fprintf (ReportLog, "%04d:\tSource line number     : %d \n", id, lineno);
  fflush (ReportLog);
  if (StopOnError) ABORT_PROGRAM();
  return;
}

//
// Function: ReportBoundsCheck()
//
// Description:
//  Generate a report for a bounds check violation.
//
// Inputs:
//  src        - The source pointer for the failed indexing operation.
//  dest       - The result pointer for the failed indexing operation.
//  allocID    - The generation number of the object's allocation.
//  allocPC    - The program counter at which the object was last allocated.
//  pc         - The program counter of the failed run-time check.
//  objstart   - The start of the object in which the source pointer was found.
//  objlen     - The length of the object in which the source pointer was found.
//  SourceFile - The source file in which the check failed.
//  lineno     - The line number at which the check failed.
//  allocSF    - The source file in which the object was allocated.
//  allocLN    - The line number at which the object was allocated.
//
// Note:
//  An objstart and objlen of 0 indicate that the source pointer was not found
//  within a valid object.
//
static void
ReportBoundsCheck (unsigned src,
                   unsigned dest,
                   unsigned allocID,
                   unsigned allocPC,
                   unsigned pc,
                   unsigned objstart,
                   unsigned objlen,
                   unsigned char * SourceFile,
                   unsigned lineno,
                   unsigned char * allocSF,
                   unsigned allocLN) {
  // Print the header and get the ID for this report
  unsigned id = printAlertHeader();

  fprintf (ReportLog, "%04d: Bounds violation to memory address 0x%08x\n", id, dest);
  fprintf (ReportLog, "%04d:                 at program counter 0x%08x\n", id, pc);
  fprintf (ReportLog, "%04d:\tIndex source pointer : 0x%08x \n", id, src);
  fprintf (ReportLog, "%04d:\tIndex result pointer : 0x%08x \n", id, dest);
  fprintf (ReportLog, "%04d:\tSource filename        : %s \n", id, SourceFile);
  fprintf (ReportLog, "%04d:\tSource line number     : %d \n", id, lineno);
  if (objstart || objlen) {
    fprintf (ReportLog, "%04d:\tObject lower bound   : 0x%08x \n", id, objstart);
    fprintf (ReportLog, "%04d:\tObject upper bound   : 0x%08x \n", id, objstart+objlen);
    fprintf (ReportLog, "%04d:\tObject allocated at program counter   : 0x%08x \n", id, allocPC);
    fprintf (ReportLog, "%04d:\tObject allocation generation number   : %d \n", id, allocID);
    fprintf (ReportLog, "%04d:\tObject allocated in source file       : %s \n", id, allocSF);
    fprintf (ReportLog, "%04d:\tObject allocated at line number       : %d \n", id, allocLN);
    fprintf(ReportLog, "=======+++++++    end of runtime error report    +++++++=======\n");
  } else {
    fprintf (ReportLog, "%04d:\tNot found within object\n", id);
  }
  fflush (ReportLog);
  if (StopOnError) ABORT_PROGRAM();
  return;
}

//
// Function: ReportExactCheck()
//
// Description:
//  Identical to ReportBoundsCheck() but does not use the start pointer.
//
// Inputs:
//  src      - The source pointer for the failed indexing operation (unused).
//  dest     - The result pointer for the failed indexing operation.
//  pc       - The program counter of the failed run-time check.
//  objstart - The start of the object in which the source pointer was found.
//  objlen   - The length of the object in which the source pointer was found.
//
// Note:
//  An objstart and objlen of 0 indicate that the source pointer was not found
//  within a valid object.
//
static void
ReportExactCheck (unsigned src,
                  unsigned dest,
                  unsigned pc,
                  unsigned objstart,
                  unsigned objlen,
                  unsigned char * SourceFile,
                  unsigned lineno) {
  // Print the header and get the ID for this report
  unsigned id = printAlertHeader();

  fprintf (ReportLog, "%04d: Bounds violation to memory address 0x%08x (ExactCheck)\n", id, dest);
  fprintf (ReportLog, "%04d:                 at program counter 0x%08x\n", id, pc);
  fprintf (ReportLog, "%04d:\tSource filename        : %s \n", id, SourceFile);
  fprintf (ReportLog, "%04d:\tSource line number     : %d \n", id, lineno);
  fprintf (ReportLog, "%04d:\tIndex result pointer : 0x%08x \n", id, dest);
  if (objstart || objlen) {
    fprintf (ReportLog, "%04d:\tObject lower bound   : 0x%08x \n", id, objstart);
    fprintf (ReportLog, "%04d:\tObject upper bound   : 0x%08x \n", id, objstart+objlen);
    fprintf(ReportLog, "=======+++++++    end of runtime error report    +++++++=======\n");
  } else {
    fprintf (ReportLog, "%04d:\tNot found within object\n", id);
  }
  fflush (ReportLog);
  if (StopOnError) ABORT_PROGRAM();
  return;
}

//
// Function: ReportOOBPointer()
//
// Description:
//  Generate a report for the use of an out of bounds (OOB) pointer.
//
// Inputs:
//  pc         - The program counter of the failed run-time check.
//  ptr        - The out of bounds pointer.
//  oobp       - The rewritten pointer that caused the fault.
//  ObjStart   - The start of the object from which the pointer came.
//  Objend     - The end of the object from which the pointer came.
//  SourceFile - The source file in which the pointer went out of bounds.
//  lineno     - The line number at which the pointer went out of bounds.
//
static void
ReportOOBPointer (unsigned pc,
                  void * ptr,
                  void * oobp,
                  void * ObjStart,
                  void * ObjEnd,
                  char * SourceFile,
                  unsigned lineno) {
  // Print the header and get the ID for this report
  unsigned id = printAlertHeader();

  fprintf (ReportLog, "%04d: Load/Store violation to out of bounds memory address 0x%08x\n", id, (unsigned)ptr);
  fprintf (ReportLog, "%04d:                 at program counter 0x08%p\n", id, pc);
  fprintf (ReportLog, "%04d:\tOut of Bounds rewrite pointer : 0x%p \n", id, oobp);
  fprintf (ReportLog, "%04d:\tStart of Object               : 0x%p \n", id, ObjStart);
  fprintf (ReportLog, "%04d:\tEnd of Object                 : 0x%p \n", id, ObjEnd);
  fprintf (ReportLog, "%04d:\tSource filename               : %s \n", id, SourceFile);
  fprintf (ReportLog, "%04d:\tSource line number            : %d \n", id, lineno);
  fflush (ReportLog);
  if (StopOnError) ABORT_PROGRAM();
  return;
}

//
// Function: ReportInvalidFree()
//
// Description:
//  Generate a report for an invalid free.
//
// Inputs:
//  pc         - The program counter at which the invalid free occurred.
//  ptr        - The invalid pointer passed to poolfree().
//  SourceFile - The source file in which the call to free() occurred.
//  lineno     - The line number at which the call to free() occurred.
//
static void
ReportInvalidFree (unsigned pc,
                   void * ptr,
                   char * SourceFile,
                   unsigned lineno) {
  // Print the header and get the ID for this report
  unsigned id = printAlertHeader();

  fprintf (ReportLog, "%04d: Invalid free of address 0x%08x\n", id,
                                                                (unsigned)ptr);
  fprintf (ReportLog, "%04d:      at program counter 0x%08x\n", id, pc);
  fprintf (ReportLog, "%04d:\tSource filename        : %s \n", id, SourceFile);
  fprintf (ReportLog, "%04d:\tSource line number     : %d \n", id, lineno);
  fflush (ReportLog);
  if (StopOnError) ABORT_PROGRAM();
  return;
}

#else

// Production code: all reporters are just simple wrappers for ABORT_PROGRAM()

static inline unsigned printAlertHeader (void) {
  ABORT_PROGRAM();
  return 0;
}

static inline void ReportDanglingPointer (void * addr,
                       unsigned pc,
                       unsigned allocpc,
                       unsigned allocgen,
                       unsigned freepc,
                       unsigned freegen) {
  ABORT_PROGRAM();
}

static inline void
ReportLoadStoreCheck (void * ptr,
                      void * pc,
                      char * SourceFile,
                      unsigned lineno) {
  ABORT_PROGRAM();
}

static inline void
ReportBoundsCheck (unsigned src,
                   unsigned dest,
                   unsigned allocID,
                   unsigned allocPC,
                   unsigned pc,
                   unsigned objstart,
                   unsigned objlen,
                   unsigned char * SourceFile,
                   unsigned lineno,
                   unsigned char * allocSF,
                   unsigned allocLN) {
  ABORT_PROGRAM();
}


static inline void ReportExactCheck (unsigned src,
                  unsigned dest,
                  unsigned pc,
                  unsigned objstart,
                  unsigned objlen,
                  unsigned char * SourceFile,
                  unsigned lineno) {
  ABORT_PROGRAM();
}

static inline void
ReportOOBPointer (unsigned pc, void * ptr, char * SourceFile, unsigned lineno) {
  ABORT_PROGRAM();
}

#endif
#endif
