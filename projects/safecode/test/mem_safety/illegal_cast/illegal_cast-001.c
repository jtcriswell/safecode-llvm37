/* Illegal cast from char to pointer. */

int f(int u)
{
  return (u << 1);
}

#define SZ 100

int main()
{
  union {
    char c;
    int *i;
  } U[SZ];
  int i, sum;

  sum = 0;
  for (i = 0; i < SZ; i++)
    U[i].c = (char) i;
  for (i = 0; i < SZ; i++)
    sum += f(*U[i].i);
  return 0;
}
