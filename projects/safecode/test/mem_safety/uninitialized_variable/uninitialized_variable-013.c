/* Uninitialized function pointer. */

int main()
{
  int a, b;
  void (*func)(int, int);
  a = b = 0;
  func(a, b);
  return 0;
}
