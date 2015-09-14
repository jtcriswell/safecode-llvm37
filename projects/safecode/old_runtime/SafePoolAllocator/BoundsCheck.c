/*
//===- BoundsCheck.c - Call backs for boundary checking       -------------===//
// 
//                       The LLVM Compiler Infrastructure
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file implements the BoundsCheck.h interface.
//
//===----------------------------------------------------------------------===//
*/

#include "BoundsCheck.h"
#include <stdio.h>
#include <stdlib.h>

void bchk_ind_fail(void *target) {
	fprintf(stderr, "Boundary checking for indirect call to %p has failed\n", target);
	exit(1);
}
