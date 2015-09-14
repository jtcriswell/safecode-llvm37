//===- CWE.h ----------------------------------------------------*- C++ -*-===//
// 
//                       The SAFECode Compiler Project
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file define ID numbers for various CWEs.
//
//===----------------------------------------------------------------------===//

#ifndef _CWE_H_
#define _CWE_H_

static const unsigned CWEBufferOverflow = 120;
static const unsigned CWEStackOverflow  = 121;
static const unsigned CWEHeapOverflow   = 122;
static const unsigned CWEFormatString   = 134;
static const unsigned CWEDoubleFree     = 415;
static const unsigned CWEDP             = 416;
static const unsigned CWENull           = 476;
static const unsigned CWEFreeNotHeap    = 590;
static const unsigned CWEFreeNotStart   = 761;
static const unsigned CWEAllocMismatch  = 762;

#endif
