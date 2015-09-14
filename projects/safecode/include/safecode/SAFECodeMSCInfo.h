//===- SAFECodeMSCInfo.h - SAFECode memory safety check info -----*- C++ -*---//
// 
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file declares the interface of the SAFECode memory safety check info
// provider pass.
//
// FIXME: This header file should be merged into LLVM-style headers.
//
//===----------------------------------------------------------------------===//

#ifndef _SAFECODE_MSC_INFO_H_
#define _SAFECODE_MSC_INFO_H_

namespace llvm {

class PassRegistry;
class ImmutablePass;

void initializeSAFECodeMSCInfoPass(PassRegistry&);
ImmutablePass *createSAFECodeMSCInfoPass();

}

#endif
