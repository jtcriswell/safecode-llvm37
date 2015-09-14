/* Unitialized pointer u. Should this fail? */

int main()
{
  int *u, v;

  v = *u ^ *u;
  if (v)
    return 1;
  else
    return 0;
}
