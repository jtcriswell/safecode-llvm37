// RUN: test.sh -f -t %t %s

/* exactcheck() move pointer in and out of bounds test */

#include <stdio.h>
#include <stdlib.h>

int
main (int argc, char ** argv) {
  char c[3];
  char tst_array[] = "test array";
  char * p0;
  char * p1;
  char * p2;
  char * p3;
  char * p4;
  char * p5;
  char * p6;

  fprintf(stderr, "setting p0 = &(tst_array[0])\n");
  p0 = &(tst_array[0]);
  fprintf(stderr, "setting p1 = p0 + 128\n");
  p1 = p0 + 128;
  fprintf(stderr, "setting p2 = p1 + 128\n");
  p2 = p1 + 128;
  fprintf(stderr, "setting p3 = p2 - 256\n");
  p3 = p2 - 256;
  fprintf(stderr, "setting p4 = p1 - 128\n");
  p4 = p1 - 128;
  fprintf(stderr, "setting p5 = p2 - 512\n");
  p5 = p2 - 512;
  fprintf(stderr, "setting p6 = p5 + 256\n");
  p6 = p5 + 256;
  
  printf("p0 = 0x%llx\n", (unsigned long long)p0);
  printf("p1 = 0x%llx -- should equal p0 + 0x%x\n", 
         (unsigned long long)p1, (unsigned)128);
  printf("p2 = 0x%llx -- should equal p0 + 0x%x\n", 
          (unsigned long long)p2, (unsigned)256);
  printf("p3 = 0x%llx -- should equal p0\n", 
         (unsigned long long)p3);
  printf("p4 = 0x%llx -- should equal p0\n", 
         (unsigned long long)p4);
  printf("p5 = 0x%llx -- should equal p0 - 0x%x\n", 
         (unsigned long long)p5, (unsigned)256);
  printf("p6 = 0x%llx -- should equal p0\n", 
         (unsigned long long)p6);
  printf("*p0 = \"%s\"\n", p0);

  /* the next two print statements should cause a memory failure,
   * but this will only happen if we are using libLTO
   */
  printf("*p1 = \"%s\"\n", p1); /* memory safety error */
  printf("*p2 = \"%s\"\n", p2); /* memory safety error */

  printf("*p3 = \"%s\"\n", p3);
  printf("*p4 = \"%s\"\n", p4);

  /* When libLTO is used, this should cause a memory safety error as well */
  printf("*p5 = \"%s\"\n", p5); /* memory safety error */

  printf("*p6 = \"%s\"\n", p6);
  fflush (stdout);

  /* The following three lines should trigger memory safety errors
   * with or without libLTO.
   */
  c[0] = *p1;
  c[1] = *p2;
  c[2] = *p5;

  return 0;
}

