// RUN: test.sh -p -t %t %s

#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

// Example of the correct usage of stat().

static char * names [] = {
  "loosy",
  "goosy",
  0
};

int
main() {
  struct stat info;

  for (char * p = names; *p; ++p) {
    if (stat (*p, &info))
      printf ("okay\n");
  }

  return 0;
}
