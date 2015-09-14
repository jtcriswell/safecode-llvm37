#
# RUN: mkdir -p %t
#
# RUN: python %s --function=sprintf > %t/sprintftest.c
# RUN: test.sh -p -t %t %t/sprintftest.c
#
# RUN: python %s --function=vsprintf > %t/vsprintftest.c
# RUN: test.sh -p -t %t %t/vsprintftest.c
#
# We xfail because this test is not 32-bit clean.
# XFAIL: i386, i686

#
# Generate a C program to test sprintf() or vsprintf().
#

basictesttab = \
[
#   format                arguments          expected output
  [ '%i',                 '1',               '1'                        ],
  [ '%d',                 '1',               '1'                        ],
  [ '%u',                 '1',               '1'                        ],
  [ '%o',                 '1',               '1'                        ],
  [ '%x',                 '1',               '1'                        ],
  [ '%X',                 '1',               '1'                        ],
  [ '%#o',                '1',               '01'                       ],
  [ '%#2o',               '1',               '01'                       ],
  [ '%.3o',               '1',               '001'                      ],
  [ '%*.*hho',            '8, 4, (unsigned char) 64', '    0100'        ],
  [ '%-*.*hi',            '8, 4, (short) 555',        '0555    '        ],
  [ '%#hx',               '(short) 0xffff',  '0xffff'                   ],
  [ '%10hhx',             '(unsigned char) 0xff', '        ff'          ],
  [ '%#x',                '1',               '0x1'                      ],
  [ '%#.2o',              '5',               '05',                      ],
  [ '%-#3.2o',            '9',               '011'                      ],
  [ '%+ 3d',              '-6',              ' -6',                     ],
  [ '%+*.*i%*x',          '4, 2, 1, 0, 15',  ' +01f'                    ],
  [ '%#X',                '1',               '0X1'                      ],
  [ '%+i%#X%0*d',         '6, 17, 2, 3',     '+60X1103'                 ],
  [ '%%i%%.*s',           '0',               '%i%.*s'                   ],
  [ '%.*s',               '5, "01234567"',   '01234'                    ],
  [ '%10.*s',             '5, "012345678"',  '     01234'               ],
  [ '%2i',                '1',               ' 1'                       ],
  [ '|%10c|',             '\'a\'',           '|         a|'             ],
  [ '%8.1e',              '1e-99',           ' 1.0e-99'                 ],
  [ '%8.0g',              '1e-99',           '   1e-99'                 ],
  [ '%8.1g',              '1e-99',           '   1e-99'                 ],
  [ '%1i',                '20',              '20'                       ],
  [ '%2.3u',              '14',              '014'                      ],
  [ '%*.*g',              '8, 1, 1e-99',     '   1e-99'                 ],
  [ '%3$*2$.*1$i',        '3, 4, 5',         ' 005'                     ],
  [ '%3$*1$.*2$s',        '10, 2, "String"', '        St'               ],
  [ '%+i',                '6',               '+6'                       ],
# Note that these %a formats are all 4-bit aligned.
  [ '%.5a',               '12.345',          '0x1.8b0a4p+3'             ],
  [ '%10.1a',             '2.5',             '  0x1.4p+1'               ],
  [ '%.*a',               '5, 12.345',       '0x1.8b0a4p+3'             ],
  [ '%*.*a',              '10, 1, 2.5',      '  0x1.4p+1',              ],
  [ '%3$-*2$.*1$a',       '12, 2, 5.0',      '0x1.400000000000p+2'      ],
  [ '%2$0*1$a',           '10, 10.0',        '0x001.4p+3'               ],
  [ '% i',                '6',               ' 6'                       ],
  [ '%#lx',               '0xfffffffffl',    '0xfffffffff'              ],
  [ '%1$ +i',             '99',              '+99'                      ],
  [ '%2$s %2$s %2$.*1$s', '4, "string"',     'string string stri'       ],
  [ 'pi: %.5f',           '3.14159',         'pi: 3.14159'              ],
  [ '%-3i',               '6',               '6  '                      ],
  [ '%2$-*1$i',           '3, 6',            '6  '                      ],
  [ '%%i%i',              '5',               '%i5'                      ],
  [ '%%%-5.4s',           '"string"',        '%stri '                   ],
  [ '%#o',                '64',              '0100'                     ],
  [ '%-2i',               '100',             '100'                      ],
  [ '%2$#*1$.*3$x',       '10, 10, 8',       '0x0000000a'               ],
  [ '%2$#*1$.*3$x',       '10, 10, 6',       '  0x00000a'               ],
  [ '%2$#*1$.*3$x',       '11, 10, 6',       '   0x00000a'              ],
  [ '%2$#*1$.*3$x',       '10, 11, 6',       '  0x00000b'               ],
  [ '%2$-#*1$.*3$x',      '10, 11, 6',       '0x00000b  '               ],
  [ '%1.7x',              '15',              '000000f'                  ],
  [ '%10.7x',             '15',              '   000000f'               ],
  [ '%-10.7x',            '15',              '000000f   '               ],
  [ '%10s',               '"abc"',           '       abc'               ],
  [ '%-10s',              '"abc"',           'abc       '               ],
  [ '%llo',               '0xffffffffffffffffull', '1777777777777777777777' ],
  [ '%-10.4s',            '"  abc"',         '  ab      '               ],
  [ '%10.4s',             '"  \\0abc"',      '          '               ],
  [ '%.7X',               '0xffffff',        '0FFFFFF'                  ],
  [ '%1s',                '"\\0"',           ' '                        ],
  [ '%1$.*2$e',           '3.14, 5',         '3.14000e+00'              ],
  [ '%0*.*E',             '14, 2, 9.99E+99',    '0000009.99E+99'        ],
  [ '%*.*E',              '14, 2, 9.99E+99',    '      9.99E+99'        ],
  [ '%0*.*F',             '14, 2, 9.9978e+02', '00000000999.78'         ],
  [ '%6c.*F',             '\'\\xfe\'',          '     \\xfe.*F'         ],
  [ '%lli',               '1ll',                '1'                     ],
  [ '%10.6llx',           '0xfffffull',         '    0fffff'            ],
  [ '%10.6lli',           '0xfffffll',          '   1048575'            ],
  [ '%.0e',               '1.0',                '1e+00'                 ],
  [ '%.0f',               '1.0',                '1'                     ],
  [ '%024.12g',           '123450.006789',      '00000000000123450.006789' ],
  [ '%0#13E',             '100.0',              '01.000000E+02'         ],
  [ '%0#13.0E',           '100.0',              '00000001.E+02'         ],
  [ '%013.0E',            '100.0',              '000000001E+02'         ],
  [ '%%%i%%%i%%',         '6, 7',               '%6%7%'                 ],
  [ ' %5s',               '"1234"',             '  1234'                ],
  [ '%.10Lg',             '3.24e9l',            '3240000000'            ],
  [ '%#.0Lf',             '0.0l',               '0.'                    ],
  [ '%2$#.*1$Le',         '0, 0.0l',            '0.e+00'                ],
# These conversions may also print "infinity"
  [ '%Lf',                '(long double) infinity()', 'inf'             ],
  [ '%f',                 'infinity()',           'inf'                 ],
  [ '%6e',                '-infinity()',          '  -inf'              ],
  [ '%-6e',               '-infinity()',          '-inf  '              ],
  [ '%+a',                'infinity()',           '+inf'                ],
# Theses conversion may also print "INFINITY"
  [ '%#F',                'infinity()',           'INF'                 ],
  [ '%+LG',               '(long double) infinity()',   '+INF'          ],
  [ '%+LG',               '(long double) -infinity()', '-INF'           ],
# These conversions may also append a character sequence in parentheses
  [ '%Lg',                '(long double) nan()',  'nan'                 ],
  [ '%f',                 'nan()',                'nan'                 ],
  [ '%6e',                'nan()',                '   nan'              ],
  [ '%.1Lg',              '(long double) nan()',  'nan'                 ],
  [ '%-6e',               'nan()',                'nan   '              ],
  [ '%F',                 'nan()',                'NAN'                 ],
  [ '%LG',                '(long double) nan()',  'NAN'                 ],
# Wide character support
  [ '%ls',                'L"123456"',            '123456'              ],
  [ '%lc',                'L\'u\'',               'u'                   ],
  [ '%1$*2$ls',           'L"123", 4',            ' 123'                ],
  [ '%.3ls',              'L"string"',            'str'                 ],
  [ '%3lc',               'L\'n\'',               '  n'                 ],
  [ '%-4ls',              'L"  "',                '    '                ],
]

percentntesttab = \
[
#  format          arguments          output           expected length(s)
  ['%n %n',        '&n1, &n2',        ' ',             [0, 1]       ],
  ['%s%n',         '"abc", &n1',      'abc',           [3]          ],
  ['%ls %n',       'L"xy", &n1',      'xy ',           [3]          ],
  ['12345%n',      '&n1',             '12345',         [5]          ],
  ['%-10i%nj',     '10, &n1',         '10        j',   [10]         ],
# Assume infinity prints 'inf'
  ['%+f-%nx',      'infinity(), &n1', '+inf-x',        [5]          ],
  ['%s%n%s',       '"a", &n1, "b"',   'ab',            [1]          ],
  ['%.2s%n%n',     '"str", &n1, &n2', 'st',            [2, 2]       ],
  ['12%n%s%n3',    '&n1, "", &n2',    '123',           [2, 2]       ]
]

prefix = \
'''#include <locale.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

int mysprintf(char *dst, const char *fmt, ...)
{
  int result;
  va_list ap;
  va_start(ap, fmt);
  result = vsprintf(dst, fmt, ap);
  va_end(ap);
  return result;
}

/* These functions return floating point NaN and infinity. */
double infinity(void)
{
  return strtod("infinity", NULL);
}

double nan(void)
{
  return strtod("nan", NULL);
}

/* Check for support of the %%ls directive. */
int russian_test(void)
{
  const char *cat = "\\xd0\\x9a\\xd0\\x9e\\xd0\\xa8\\xd0\\x9a\\xd0\\x90";
  char buf[100];
  wchar_t wcat[100];
  int fail, sz;
  size_t result;
  fail = 0;
  setlocale(LC_ALL, "en_US.UTF-8");
  result = mbstowcs(&wcat[0], &cat[0], 100);
  if (result == (size_t) -1)
  {
    puts("error in multibyte conversion!");
    return 1;
  }
  sz = %s(&buf[0], "%%ls", &wcat[0]);
  fail = strcmp(&buf[0], &cat[0]) != 0 || sz != strlen(&buf[0]);
  setlocale(LC_ALL, "");
  if (fail)
    puts("failed test 0!");
  return fail;
}

int main(void)
{
  int n1, n2;
  int fail;
  int result;
  char buf[100];
  fail = 0;

  n1 = n2 = 0;

  /* Begin tests. */

  fail = russian_test();
'''

test = \
'''
  result = %s(&buf[0], %s, %s);
  if (strcmp(buf, %s) != 0 || result != strlen(buf))
  {
    puts("failed test %i!");
    /* printf("%%s\\n", buf); */
    fail = 1;
  }
'''

percent_n_test = \
'''
  result = %s(&buf[0], %s, %s);
  if (strcmp(buf, %s) != 0 || result != strlen(buf) || n1 != %s || n2 != %s)
  {
    puts("failed test %i!");
    fail = 1;
  }
'''

suffix = \
'''
  /* End tests. */

  return fail;
}
'''

import cStringIO
import optparse
import sys

quote = lambda s : '"' + s + '"'

def make_regular_test(count, param, f):
  fmt = quote(param[0])
  arg = param[1]
  out = quote(param[2])
  return test % (f, fmt, arg, out, count)

def make_percent_n_test(count, param, f):
  fmt = quote(param[0])
  arg = param[1]
  out = quote(param[2])
  ns = len(param[3])
  if ns == 0:
    n1, n2 = ('n1', 'n2')
  elif ns == 1:
    n1, n2 = (param[3][0], 'n2')
  elif ns == 2:
    n1, n2 = (param[3][0], param[3][1])
  return percent_n_test % (f, fmt, arg, out, n1, n2, count)

if __name__ == '__main__':
  parser = optparse.OptionParser()
  parser.set_defaults(function='sprintf')
  parser.add_option('--function', dest='function',
                    help='function to test: sprintf or vsprintf')
  (options, args) = parser.parse_args()
  output = cStringIO.StringIO()
  if options.function == 'sprintf':
    function = 'sprintf'
  elif options.function == 'vsprintf':
    function = 'mysprintf'
  else:
    sys.stderr.write('specify either sprintf or vsprintf!\n')
    sys.exit(1)
  output.write(prefix % function)
  count = 1
  for unit in basictesttab:
    built_test = make_regular_test(count, unit, function)
    output.write(built_test)
    count += 1
  for unit in percentntesttab:
    built_test = make_percent_n_test(count, unit, function)
    output.write(built_test)
    count += 1
  output.write(suffix)
  sys.stdout.write(output.getvalue())
  output.close()
