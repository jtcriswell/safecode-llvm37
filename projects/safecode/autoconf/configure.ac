dnl **************************************************************************
dnl * Initialize
dnl **************************************************************************
AC_INIT([SAFECode],[1.00],[jtcriswel@gmail.com])

dnl Identify where LLVM source tree is
LLVM_SRC_ROOT="`(cd $srcdir/../..; pwd)`"
LLVM_OBJ_ROOT="`(cd ../..; pwd)`"

dnl Tell autoconf that this is an LLVM project being configured
dnl This provides the --with-llvmsrc and --with-llvmobj options
LLVM_CONFIG_PROJECT($LLVM_SRC_ROOT,$LLVM_OBJ_ROOT)

dnl Tell autoconf that the auxilliary files are actually located in
dnl the LLVM autoconf directory, not here.
AC_CONFIG_AUX_DIR($LLVM_SRC/autoconf)

dnl Verify that the source directory is valid
AC_CONFIG_SRCDIR(["Makefile.common.in"])

dnl Configure a common Makefile
AC_CONFIG_FILES(Makefile.common)

dnl Configure project makefiles
dnl List every Makefile that exists within your source tree
AC_CONFIG_MAKEFILE(Makefile)
AC_CONFIG_MAKEFILE(lib/Makefile)
AC_CONFIG_MAKEFILE(lib/ArrayBoundChecks/Makefile)
AC_CONFIG_MAKEFILE(lib/ConvertUnsafeAllocas/Makefile)
AC_CONFIG_MAKEFILE(lib/InsertPoolChecks/Makefile)
AC_CONFIG_MAKEFILE(lib/PointerChecks/Makefile)
AC_CONFIG_MAKEFILE(lib/StackSafety/Makefile)
AC_CONFIG_MAKEFILE(runtime/Makefile)
AC_CONFIG_MAKEFILE(tools/Makefile)
AC_CONFIG_MAKEFILE(tools/clang/Makefile)
AC_CONFIG_MAKEFILE(test/Makefile)
AC_CONFIG_MAKEFILE(test/BOdiagsuite-20050808/Makefile)

dnl **************************************************************************
dnl * Determine which system we are building on
dnl **************************************************************************

dnl **************************************************************************
dnl * Check for programs.
dnl **************************************************************************

AC_PATH_PROG(OMEGA, [omega], [echo omega])
if test "$OMEGA" != "echo omega" ; then
  AC_DEFINE([HAVE_OMEGA],[1],[Define if the Omega program is available])
  AC_DEFINE_UNQUOTED([LLVM_PATH_OMEGA],"$OMEGA",
   [Define to path of Omega program if found or 'echo omega' otherwise])
fi

dnl **************************************************************************
dnl * Check for libraries.
dnl **************************************************************************

dnl Check if fopen64 is available.
AC_SEARCH_LIBS(fopen64,c,AC_DEFINE([HAVE_FOPEN64],[1],
               [Define if fopen64() is available on this platform.]),
               AC_MSG_WARN([fopen64() not found - Will use fopen() instead]))

dnl Check for GNU GMP
AC_CHECK_LIB(gmp, __gmpz_init)

dnl Check for CVC3
AC_CHECK_LIB(cvc3, vc_get_error_status,[],[],[-lgmp -lstdc++])

dnl **************************************************************************
dnl * Checks for header files.
dnl **************************************************************************
AC_CHECK_HEADERS([iconv.h])

dnl **************************************************************************
dnl * Checks for typedefs, structures, and compiler characteristics.
dnl **************************************************************************

dnl **************************************************************************
dnl * Checks for library functions.
dnl **************************************************************************

AC_CHECK_FUNCS(fgetc_unlocked fwrite_unlocked)
AC_CHECK_FUNCS(mempcpy strcasestr stpcpy strnlen)

dnl **************************************************************************
dnl * Enable various compile-time options
dnl **************************************************************************

dnl --enable-kernel: compile SAFECode for use with kernels compiled to SVA
AC_ARG_ENABLE(kernel,
  AS_HELP_STRING(--enable-kernel,
                 [Use SAFECode for use with Linux Kernel (default is NO)]),
                  AC_DEFINE(LLVA_KERNEL,[[1]]),
                  )

dnl --enable-poolalloc: compile SAFECode with Automatic Pool Allocation support
AC_ARG_ENABLE(poolalloc,
  AS_HELP_STRING(--enable-poolalloc,
                 [Enable libLTO Automatic Pool Allocation (default is NO)]),
                  AC_DEFINE(HAVE_POOLALLOC,[[1]]),
                  )

dnl --enable-capsaicin: compile SAFECode with Capsaicin support
AC_ARG_ENABLE(capsaicin,
  AS_HELP_STRING(--enable-capsaicin,
                 [Enable Capsaicin (default is NO)]),
                  AC_DEFINE(HAVE_CAPSAICIN,[[1]]),
                  )

dnl **************************************************************************
dnl * Set the location of various third-party software packages
dnl **************************************************************************

dnl Specify the location of the poolalloc project
AC_ARG_WITH(poolalloc-srcdir,
  AS_HELP_STRING([--with-poolalloc-srcdir],
    [Specify location of Pool Allocation source code]),
    AC_SUBST(poolallocsrcdir,"$withval"),
    AC_SUBST(poolallocsrcdir,"$LLVM_SRC_ROOT/projects/poolalloc"
    ))

AC_ARG_WITH(poolalloc-objdir,
  AS_HELP_STRING([--with-poolalloc-objdir],
    [Specify location of Pool Allocation object code]),
    AC_SUBST(poolallocobjdir,"$withval"),
    AC_SUBST(poolallocobjdir,"$LLVM_OBJ_ROOT/projects/poolalloc"
    ))

dnl Specify the location of the capsaicin project
AC_ARG_WITH(capsaicin-srcdir,
  AS_HELP_STRING([--with-capsaicin-srcdir],
    [Specify location of Capsaicin source code]),
    AC_SUBST(capsaicinsrcdir,"$withval"),
    AC_SUBST(capsaicinsrcdir,"$LLVM_SRC_ROOT/projects/capsaicin"
    ))

AC_ARG_WITH(capsaicin-objdir,
  AS_HELP_STRING([--with-capsaicin-objdir],
    [Specify location of Capsaicin object code]),
    AC_SUBST(capsaicinobjdir,"$withval"),
    AC_SUBST(capsaicinobjdir,"$LLVM_OBJ_ROOT/projects/capsaicin"
    ))

dnl Specify the location of the LLVM Test Suite
AC_ARG_WITH(test-srcdir,
  AS_HELP_STRING([--with-test-srcdir],
    [Specify location of LLVM Test Suite source code]),
    AC_SUBST(testsrcdir,"$withval"),
    AC_SUBST(testsrcdir,"$LLVM_SRC_ROOT/projects/test-suite"
    ))

AC_ARG_WITH(test-objdir,
  AS_HELP_STRING([--with-test-objdir],
    [Specify location of LLVM Test Suite object code]),
    AC_SUBST(testobjdir,"$withval"),
    AC_SUBST(testobjdir,"$LLVM_OBJ_ROOT/projects/test-suite"
    ))

dnl Ensure that all pathnames are absolute pathnames
poolallocsrcdir=`cd $poolallocsrcdir; pwd`
poolallocobjdir=`cd $poolallocobjdir; pwd`
testsrcdir=`cd $testsrcdir; pwd`
testobjdir=`cd $testobjdir; pwd`

dnl **************************************************************************
dnl * Set any flags used in the Make
dnl **************************************************************************

dnl Specify whether we should use libLTO when running the test suite

AC_SUBST(enable_lto_tests)
AC_ARG_ENABLE(lto-tests,
  AS_HELP_STRING([--enable-lto-tests],
    [Enable use of lto in SAFECode test-suite runs (default is no)]),
  [enable_lto_tests=$enableval], 
  [enable_lto_tests=no]
  )


dnl **************************************************************************
dnl * Create the output files
dnl **************************************************************************

AC_CONFIG_HEADERS(include/safecode/Config/config.h)

dnl **************************************************************************
dnl * Configure any susidiary projects that are included in SAFECode.
dnl * At present, only Clang falls into this catagory.
dnl **************************************************************************

AC_CONFIG_HEADERS(tools/clang/include/clang/Config/config.h)


AC_OUTPUT

