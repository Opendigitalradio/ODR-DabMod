# Copyright (C) 2007, 2008, 2009, 2010, 2011, 2012 Her Majesty the
# Queen in Right of Canada (Communications Research Center Canada)

# Copyright (C) 2016 Matthias P. Braendli, http://mpb.li

# This file is part of ODR-DabMod.
#
# ODR-DabMod is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
#
# ODR-DabMod is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with ODR-DabMod.  If not, see <http://www.gnu.org/licenses/>.

AC_PREREQ(2.59)
AC_INIT([ODR-DabMod], [0.6.0], [matthias.braendli@mpb.li])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE([-Wall foreign subdir-objects])
AC_CONFIG_SRCDIR([src/DabMod.cpp])
AM_CONFIG_HEADER([config.h])
AM_SILENT_RULES([yes])

ACLOCAL_AMFLAGS = -I m4

case $target_cpu in
    x86_64)
AC_DEFINE([__64BIT__], [], [__16BIT__, __32BIT__])
    ;;
    *)
AC_DEFINE([__32BIT__], [], [__16BIT__, __64BIT__])
    esac
#case $target_os in

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL
AC_PROG_CPP
AC_PROG_MKDIR_P

AX_CXX_COMPILE_STDCXX_11(noext,mandatory)

EXTRA=""
AC_ARG_ENABLE([debug],
        [AS_HELP_STRING([--enable-debug], [Enable debugger symbols])],
        [], [enable_debug=no])
AC_ARG_ENABLE([prof],
        [AS_HELP_STRING([--enable-prof], [Enable profiling])],
        [], [enable_prof=no])
AC_ARG_WITH([debug-malloc],
        [AS_HELP_STRING([--with-debug-malloc[=no|yes|duma|efence|...]],
            [Add malloc debugger support])],
        [], [with_debug_malloc=no])
AC_ARG_ENABLE([trace],
        [AS_HELP_STRING([--enable-trace], [Enable trace output])],
        [], [enable_trace=no])

# ZeroMQ message queue input
AC_ARG_ENABLE([zeromq],
        [AS_HELP_STRING([--disable-zeromq], [Disable ZeroMQ input, output and remote control])],
        [], [enable_zeromq=yes])

# UHD support control
AC_ARG_ENABLE([output_uhd],
        [AS_HELP_STRING([--disable-output-uhd], [Disable UHD output])],
        [], [enable_output_uhd=yes])

PKG_CHECK_MODULES([FFTW], [fftw3f], enable_fftw=yes, enable_fftw=no)

echo "Checking zeromq"

AS_IF([test "x$enable_zeromq" = "xyes"],
        [AC_DEFINE(HAVE_ZEROMQ, [1], [Define if ZeroMQ is enabled]) ,
         AC_CHECK_LIB(zmq, zmq_init, ZMQ_LIBS="-lzmq" ,[AC_MSG_ERROR([ZeroMQ libzmq is required])])])
AS_IF([test "x$enable_debug" = "xno"],
        [EXTRA="$EXTRA -DNDEBUG"],
        [EXTRA="$EXTRA"])
AS_IF([test "x$enable_prof" != "xno"],
        [EXTRA="$EXTRA -pg"])

# Define conditionals for Makefile.am
AM_CONDITIONAL([DEBUG], [test "x$enable_trace" = "xyes"])
AM_CONDITIONAL([IS_GIT_REPO], [test -d '.git'])

# Defines for config.h
AX_PTHREAD([], AC_MSG_ERROR([requires pthread]))

AC_SUBST([CFLAGS], ["$EXTRA $FFTW_CFLAGS $PTHREAD_CFLAGS"])
AC_SUBST([CXXFLAGS], ["$CXXFLAGS $EXTRA $FFTW_CFLAGS $PTHREAD_CFLAGS"])
AC_SUBST([LIBS], ["$FFTW_LIBS $PTHREAD_LIBS $ZMQ_LIBS"])

# Checks for UHD.
AS_IF([test "x$enable_output_uhd" = "xyes"],
        [AC_DEFINE(HAVE_OUTPUT_UHD, [1], [Define if UHD output is enabled]) ,
         AC_CHECK_LIB([uhd], [main], [], [AC_MSG_ERROR([library uhd is missing])])])

AX_BOOST_BASE([1.55.0], [], AC_MSG_ERROR([BOOST 1.55 or later is required]))
AC_CHECK_LIB([boost_system], [main], [], [AC_MSG_ERROR([library boost_system is missing])])
AC_CHECK_LIB([boost_thread], [main], [], [AC_MSG_ERROR([library boost_thread is missing])])

AC_CHECK_LIB([rt], [clock_gettime], [], [AC_MSG_ERROR([library rt is missing])])

# Tests for different memory allocation debuggers.
# Valgrind doesn't need any.
AS_IF([test "x$enable_debug" != "xno"],
        [AS_IF([test "x$with_debug_malloc" != "xno"],
            [AS_IF([test "x$with_debug_malloc" = "xyes"],
                [AC_CHECK_LIB([duma], [malloc], [],
                    [AC_CHECK_LIB([efence], [malloc])])],
                [AC_CHECK_LIB([$with_debug_malloc], [malloc])])]
                )])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h limits.h malloc.h memory.h netinet/in.h stdint.h stdlib.h string.h sys/time.h sys/timeb.h unistd.h])
AC_CHECK_DECLS([_mm_malloc], [], [], [#include <mm_malloc.h>])
AC_HEADER_STDC

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T
AC_HEADER_TIME
AC_MSG_CHECKING([for M_PIl])
AC_LANG_PUSH([C++])
AC_COMPILE_IFELSE(
        [AC_LANG_PROGRAM([[#include <math.h>]], [[double pi = M_PIl;]])],
        [AC_MSG_RESULT([yes])], [AC_MSG_RESULT([no]) AC_DEFINE([M_PIl], [M_PI], [Replacing define])])
AC_LANG_POP([C++])


# Check for SSE
AC_MSG_CHECKING(for SSE in current arch/CFLAGS)

save_CXXFLAGS="$CXXFLAGS"
CXXFLAGS="$CXXFLAGS -msse"
AC_MSG_CHECKING([whether we need to add -msse])
AC_COMPILE_IFELSE(  [AC_LANG_PROGRAM([[
#include <xmmintrin.h>
__m128 testfunc(float *a, float *b) {
  return _mm_add_ps(_mm_loadu_ps(a), _mm_loadu_ps(b));
}
]])],
[has_sse=yes],
[has_sse=no]
)
AC_MSG_RESULT($has_sse)
if test x"$has_sse" = xno; then
    CXXFLAGS="$save_CXXFLAGS"
fi


AM_CONDITIONAL([HAVE_SSE], [test "x$has_sse" = "xyes"])

# TODO: Check for NEON

AC_TYPE_SIGNAL
AC_CHECK_FUNCS([bzero floor ftime gettimeofday memset sqrt strchr strerror strtol])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

echo
echo "***********************************************"
echo
enabled=""
disabled=""
for feat in debug prof trace output_uhd zeromq
do
    eval var=\$enable_$feat
    AS_IF([test "x$var" = "xyes"],
          [enabled="$enabled $feat"],
          [disabled="$disabled $feat"])
done
echo "  Features"
echo "  Enabled: $enabled"
echo "  Disabled: $disabled"
echo
echo
enabled=""
disabled=""
for feat in with_debug_malloc has_sse
do
    eval var=\$$feat
    AS_IF([test "x$var" != "xno"],
          [enabled="$enabled $feat"],
          [disabled="$disabled $feat"])
done
echo "  Options"
echo "  Active: $enabled"
echo "  Disabled: $disabled"

echo
echo "***********************************************"
echo
