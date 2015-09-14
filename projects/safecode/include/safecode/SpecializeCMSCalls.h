//===- SpecializeCMSCalls.h - The CMS call specialization pass ---*- C++ -*---//
// 
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file declares the interface of the common memory safety specialization
// pass.
//
// FIXME: This header file should be merged into LLVM-style headers.
//
//===----------------------------------------------------------------------===//

#ifndef _SAFECODE_SPECIALIZE_CMS_CALLS_H_
#define _SAFECODE_SPECIALIZE_CMS_CALLS_H_

namespace llvm {

class PassRegistry;
class ModulePass;

void initializeSpecializeCMSCallsPass(PassRegistry&);
ModulePass *createSpecializeCMSCallsPass();

}

#endif
