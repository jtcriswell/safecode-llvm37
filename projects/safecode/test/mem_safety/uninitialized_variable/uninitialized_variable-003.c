/* Use an uninitialized pointer as an array */

int main()
{
  int a, *b;
  a = b[1];
  return a;
}
