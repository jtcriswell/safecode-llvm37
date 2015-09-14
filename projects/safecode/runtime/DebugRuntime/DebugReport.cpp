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

#include "DebugReport.h"

#include <iostream>

namespace llvm {

void
DebugViolationInfo::print(std::ostream & OS) const {
  //
  // Print out the regular error information.
  //
  ViolationInfo::print(OS);

  //
  // Print the source filename and line number.
  //
  OS << "= Fault PC Source                       :\t"
     << (this->SourceFile ? this->SourceFile : "UNKNOWN")
     << ":" << std::dec << this->lineNo << "\n";

  //
  // Print the pool handle.
  //
#if 0
  OS << "= Pool Handle                           :\t" << this->PoolHandle << "\n";
#endif

  //
  // Print the debug metata.
  //
  if (dbgMetaData) {
    dbgMetaData->print(OS);
  }
}

void
OutOfBoundsViolation::print(std::ostream & OS) const {
  //
  // Print out the regular error information.
  //
  DebugViolationInfo::print(OS);

  //
  // Print information on the start and end locations of the object.
  //
  OS << "= Object start                          :\t" 
     << std::showbase << std::hex << this->objStart << "\n"
     << "= Object length                         :\t"
     << this->objLen << "\n";
}

void
AlignmentViolation::print(std::ostream & OS) const {
  //
  // Print out the regular error information.
  //
  OutOfBoundsViolation::print(OS);

  //
  // Print information on the alignment requirements for the object.
  //
  OS << "= Alignment                             :\t" 
     << std::showbase << std::hex << this->alignment << "\n";
}

void
WriteOOBViolation::print(std::ostream & OS) const {
  //
  // Print out the regular error information.
  //
  DebugViolationInfo::print(OS);

  //
  // Print information on the writing (or copying) out of bounds.
  //
  if (-1 != this->srcSize) {
    OS << std::dec
       << "= Source size (in bytes)                :\t" 
       << this->srcSize << "\n";
  }

  OS << std::dec
     << "= Destination size (in bytes)           :\t"
     << this->dstSize << "\n";

  if (-1 != this->copied) {
    OS << std::dec
       << "= Number of bytes copied                :\t"
       << this->copied << "\n";
  }
}

void
CStdLibViolation::print(std::ostream & OS) const {
  //
  // Print out the regular error information.
  //
  DebugViolationInfo::print(OS);

  //
  // Print the name of the the library function in which the error occurred.
  //
  if (function != 0) {
    OS << "= Library function                      :\t"
       << function << "\n";
  }

}

void
DebugMetaData::print(std::ostream & OS) const {
  //
  // Only print the cononical address when debugging SAFECode itself.
  // The MMU remapping magic should not be exposed to the programmer during
  // regular operation.
  //
#if 0
  OS << "= Canonical object address              :\t" << std::hex
     << this->canonAddr << "\n";
#endif

  //
  // Print object allocation information if available.
  //
  OS << "=\n"
     << "= Object allocated at PC                :\t" << std::hex
     << this->allocPC << "\n"
     << "= Allocated in Source File              :\t"
     << (this->SourceFile ? (char *) this->SourceFile : "UNKNOWN")
     << ":" << std::dec << this->lineno << "\n";
  if (this->allocID) {
    OS << "= Object allocation sequence number     :\t" << std::dec
       << this->allocID << "\n";
  }

  //
  // Print deallocation information if it is available.
  //
  if (this->freeID) {
    OS << "=\n"
       << "= Object freed at PC                    :\t" << std::hex
       << this->freePC << "\n";
    OS << "= Freed in Source File                  :\t"
       << (this->FreeSourceFile ? (char *) this->FreeSourceFile : "UNKNOWN")
       << ":" << std::dec << this->Freelineno << "\n";
    OS << "= Object free sequence number           :\t" << std::dec
       << this->freeID << "\n";
  }

  OS << std::flush;
  return;
}

}
