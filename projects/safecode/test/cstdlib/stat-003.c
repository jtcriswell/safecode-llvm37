// RUN: test.sh -e -t %t %s
// XFAIL: darwin,linux

//
// We expect this test to fail on Mac OS X (Darwin) because the system call
// names appear differently at the LLVM IR level, and we haven't added full
// support for all of them yet.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

const char * pathname = "/etc/passwd";

// Test whether we can detect a freed filename buffer

int
main() {
  char * name;
  struct stat info;
  struct stat * stp;

  //
  // Allocate space for the name and treat it like a stat struct.  This should
  // make it appear to be type-inconsistent.
  //
  name = malloc (sizeof (struct stat) + strlen (pathname));
  stp = (struct stat *) name;
  stp->st_uid = 5;

  //
  // Copy a pathname into the memory buffer.
  //
  strcpy (name, "/etc/passwd");

  //
  // Now free the buffer
  //
  free (name);

  //
  // Attempt to stat the pathname.  Note that since the filename is type-unsafe
  // and since it has been freed, SAFECode should detect it.
  //
  if (stat (name, &info))
    printf ("okay\n");

  return 0;
}
