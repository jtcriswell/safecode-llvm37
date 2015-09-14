/* Uninitialized function pointer in nested union */

typedef union
{
  union {
    union {
      union {
        void (*f)(int, char *);
        int item;
      } InnerUnion3;
      int k;
    } InnerUnion2;
    int j;
  } InnerUnion1;
  int i;
} NestedUnion;

int main()
{
  NestedUnion n;
  n.InnerUnion1.InnerUnion2.InnerUnion3.f(10, "String");
  return 0;
}
