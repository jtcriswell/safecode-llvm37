// RUN: test.sh -e -t %t %s
// XFAIL: darwin

#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

// Example of incorrect usage of stat().

int
main() {
  char name[1024];
  struct stat info;

  memset (name, 'c', 1024);
  if (stat (name, &info))
    printf ("okay\n");

  return 0;
}
