/* Passing pointer to uninitialized function pointer. */

typedef int (*fptr)();

int f(fptr *ptr)
{
  return (*ptr)();
}

int main()
{
  fptr p;
  f(&p);
  return 0;
}
