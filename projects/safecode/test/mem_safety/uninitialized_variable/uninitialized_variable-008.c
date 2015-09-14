/* Uninitialized pointer passed and used in a parameter. */

int f(int *ptr)
{
  return *ptr << 7;
}

int main()
{
  int *ptr, y;
  y = f(ptr);
  return y;
}
