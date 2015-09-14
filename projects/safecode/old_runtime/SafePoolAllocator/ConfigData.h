/*===- ConfigData.h - Structures for Configuring the Run-time ---*- C++ -*-===*/
/* 
 *                          The SAFECode Compiler 
 *
 * This file was developed by the LLVM research group and is distributed under
 * the University of Illinois Open Source License. See LICENSE.TXT for details.
 * 
 *===----------------------------------------------------------------------===//
 *
 * This file defines the structure used to configure the SAFECode run-time.
 *
 *===----------------------------------------------------------------------===*/

#ifndef CONFIGDATA_H
#define CONFIGDATA_H

/*
 * Structure: ConfigData
 *
 * Description:
 *  This structure tells us what the configuration of the runtime is
 */
struct ConfigData {
  /* Flags whether objects should be remapped */
  unsigned RemapObjects;

  /* Flags whether strict indexing rules should be enforced */
  unsigned StrictIndexing;

  /* Flags whether we should track external memory allocations */
  unsigned TrackExternalMallocs;
};

extern struct ConfigData ConfigData;
#endif

