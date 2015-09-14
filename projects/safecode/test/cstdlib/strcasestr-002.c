// RUN: test.sh -e -t %t %s

// strcasestr() with an unterminated substring.

extern char *strcasestr(const char *s1, const char *s2);

int main()
{
  char substring[] = { 'a', 'b', 'c' };
  char string[] = "ABCDEFG";

  strcasestr(string, substring);
  return 0;
}
