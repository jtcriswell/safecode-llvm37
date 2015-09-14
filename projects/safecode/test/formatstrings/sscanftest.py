#
# RUN: mkdir -p %t
#
# RUN: python %s --function=sscanf > %t/sscanftest.c
# RUN: test.sh -p -t %t %t/sscanftest.c
#
# RUN: python %s --function=vsscanf > %t/vsscanftest.c
# RUN: test.sh -p -t %t %t/vsscanftest.c
#

#
# Generate a C program to test sscanf() or vsscanf().
#

Str = 0 # string
Int = 1 # integer
Flt = 2 # float
Dbl = 3 # double
Ldb = 4 # long double
Arr = 5 # array of bytes of given size
Wst = 6 # wide character string
Nsp = 7 # %n specifier (as int)

basictests = \
[
#   input        format string    return  write
  [ 'abc ',      '%s',            1,      [ (Str, 'abc')     ]             ],
  [ '  abc',     '%1[ ]',         1,      [ (Str, ' ')       ]             ],
  [ '  abc',     '%[] a]',        1,      [ (Str, '  a')     ]             ],
  [ '\\n123',    '%2d%1d',        2,      [ (Int, '12'),  (Int, '3') ]     ],
  [ '%ab\\n  ',  '%%%s',          1,      [ (Str, 'ab')      ]             ],
  [ '[][][]',    '%[][]',         1,      [ (Str, '[][][]')  ]             ],
  [ 'abcde',     '%3[^ ]%s',      2,      [ (Str, 'abc'), (Str, 'de') ]    ],
  [ '123.4',     '%i.%i',         2,      [ (Int, '123'), (Int, '4')  ]    ],
  [ '123.4',     '%Lf',           1,      [ (Ldb, '123.4l')  ]             ],
  [ '6.6e-3',    '%1Le',          1,      [ (Ldb, '6.0l')    ]             ],
  [ '6.6e-3',    '%7Le',          1,      [ (Ldb, '6.6e-3l') ]             ],
  [ '6.6e-3',    '%lg',           1,      [ (Dbl, '6.6e-3')  ]             ],
  [ 'yy x',      '%3c',           1,      [ (Arr, 'yy ')     ]             ],
  [ '-0x1.6p+2', '%Lf',           1,      [ (Ldb, '-5.5l')   ]             ],
  [ '100ent',    '%e',            0,      [ ],                             ],
  [ '100e',      '%e',            'EOF',  [ ],                             ],
  [ '10010',     '%*2i%o',        1,      [ (Int, '8')       ]             ],
  [ ' \\na',     '%[^a]',         1,      [ (Str, ' \\n')    ]             ],
  [ '-6',        '%f',            1,      [ (Flt, '-6.f')    ]             ],
  [ ' ans',      '%1s',           1,      [ (Str, 'a')       ]             ],
  [ 'abcdefgh',  '%[a-f]%[g-h]',  2,      [ (Str, 'abcdef'), (Str, 'gh') ] ],
  [ '123.4',     '%3ls',          1,      [ (Wst, '123')     ]             ],
  [ 'str',       '%ls',           1,      [ (Wst, 'str')     ]             ],
  [ 'this',      '%[nmop]',       0,      [ ],                             ],
  [ '%',         '%%%n',          0,      [ (Nsp, '1')       ],            ],
  [ ' string',   '%10s%n',        1,      [ (Str, 'string'), (Nsp, '7') ]  ],
  [ '  ',        '%c%n%c',        2,[ (Arr, ' '), (Nsp, '1'), (Arr, ' ') ] ],
  [ 'i',         '%n%ni',         0,      [ (Nsp, '0'), (Nsp, '0') ]       ],
  [ 'i',         '%ni%n',         0,      [ (Nsp, '0'), (Nsp, '1') ]       ],
  [ 'abc 1',     '%ls%n%d',       2, [ (Wst,'abc'), (Nsp,'3'), (Int,'1') ] ],
  [ '1234',      '%2c%n',         1,      [ (Arr, '12'), (Nsp, '2') ]      ],
  [ 'xyzab',     '%[x-z]%nab',    1,      [ (Str, 'xyz'), (Nsp, '3') ]     ],
  [ '456789',    '%[4-7]%n%i',    2,[ (Str,'4567'), (Nsp,'4'), (Int,'89') ]],
  [ '456789',    '%4i%n%i',       2,[ (Int,'4567'), (Nsp,'4'), (Int,'89') ]],
]

quote = lambda s : '"' + s + '"'

def build_test(n, input, fmt, rval, wrvals, function):
  strcount = 0
  intcount = 0
  fltcount = 0
  dblcount = 0
  ldbcount = 0
  wstcount = 0
  nspcount = 0
  front = 'result = %s(%s, %s' % (function, quote(input), quote(fmt))
  args = []
  comparisons = []
  for wrval in wrvals:
    valtype = wrval[0]
    compare = wrval[1]
    if valtype == Str:
      strcount += 1
      args.append('s%i' % strcount)
      comparisons.append('strcmp(s%i, %s) == 0' % (strcount, quote(compare)))
    elif valtype == Int:
      intcount += 1
      args.append('&i%i' % intcount)
      comparisons.append('i%i == %s' % (intcount, compare))
    elif valtype == Flt:
      fltcount += 1
      args.append('&f%i' % fltcount)
      comparisons.append('flteq(f%i, %s)' % (fltcount, compare))
    elif valtype == Dbl:
      dblcount += 1
      args.append('&d%i' % dblcount)
      comparisons.append('dbleq(d%i, %s)' % (dblcount, compare))
    elif valtype == Ldb:
      ldbcount += 1
      args.append('&ldb%i' % ldbcount)
      comparisons.append('ldbeq(ldb%i, %s)' % (ldbcount, compare))
    elif valtype == Arr:
      size = len(compare)
      strcount += 1
      args.append('s%i' % strcount)
      comparisons.append(
        'memcmp((void *) s%i, (void *) %s, %i) == 0'
          % (strcount, quote(compare), size)
      )
    elif valtype == Wst:
      wstcount += 1
      args.append('w%i' % wstcount)
      comparisons.append('wcscmp(w%i, L%s) == 0' % (wstcount, quote(compare)))
    elif valtype == Nsp:
      nspcount += 1
      args.append('&n%i' % nspcount)
      comparisons.append('n%i == %s' % (nspcount, compare))
  args.append('(void *) buf') # Don't segfault bad implementations that write
                              # one more argument than they should.
  if args != []:
    argv = ', ' + ', '.join(args)
  else:
    argv = ''
  command = front + argv + ');'
  tests   = [command]
  for ctest in comparisons:
    tests.append(
      'if (!(%s)) { fail = 1; puts("failure on test %s"); }' % (ctest, n)
    )
  tests.append(
    'if (result != %s) { fail = 1; puts("failure on test %s"); }' % (rval, n)
  )
  return '  ' + '\n  '.join(tests)

top = \
'''#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

int mysscanf(char *buf, const char *fmt, ...)
{
  int result;
  va_list ap;
  va_start(ap, fmt);
  result = vsscanf(buf, fmt, ap);
  va_end(ap);
  return result;
}

/* Very crude floating point equality tests. */
int ldbeq(long double a, long double b)
{
  return (a - b) < 0.00001l && (a - b) > -0.00001l;
}

int dbleq(double a, double b)
{
  return (a - b) < 0.00001 && (a - b) > -0.00001;
}

int flteq(float a, float b)
{
  return (a - b) < 0.00001f && (a - b) > -0.00001f;
}

int main(int argc, char *argv[])
{
  char buf[100];
  char s1[100], s2[100], s3[100], s4[100], s5[100];
  wchar_t w1[100], w2[100], w3[100], w4[100], w5[100];
  int  i1, i2, i3, i4, i5;
  int  n1, n2, n3, n4, n5;
  long double ldb1, ldb2, ldb3, ldb4, ldb5;
  double d1, d2, d3, d4, d5;
  float  f1, f2, f3, f4, f5;
  int fail, result;

  fail = 0;

  /* Begin tests. */

'''

bottom = \
'''  /* End tests. */

  return fail;
}
'''

import cStringIO
import optparse
import sys

if __name__ == '__main__':
  parser = optparse.OptionParser()
  parser.set_defaults(function='sscanf')
  parser.add_option('--function', dest='function',
                    help='function to test: sscanf or vsscanf')
  (options, args) = parser.parse_args()
  if options.function == 'sscanf':
    function = 'sscanf'
  elif options.function == 'vsscanf':
    function = 'mysscanf'
  else:
    sys.stderr.write('specify either sscanf or vsscanf!\n')
    sys.exit(1)
  count = 0
  output = cStringIO.StringIO()
  output.write(top)
  for basic_test in basictests:
    count += 1
    input  = basic_test[0]
    fmt    = basic_test[1]
    rval   = basic_test[2]
    wrvals = basic_test[3]
    result = build_test(count, input, fmt, rval, wrvals, function)
    output.write(result)
    output.write('\n\n')
  output.write(bottom)
  sys.stdout.write(output.getvalue())
  output.close()
