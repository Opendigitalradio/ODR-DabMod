# Copyright (C) 2007, 2008, 2009, 2010, 2011, 2012 Her Majesty the
# Queen in Right of Canada (Communications Research Center Canada)

# This file is part of CRC-DADMOD.
#
# CRC-DADMOD is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
#
# CRC-DADMOD is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with CRC-DADMOD.  If not, see <http://www.gnu.org/licenses/>.

AC_PREREQ(2.59)
AC_INIT([CRC-DabMod], [0.3.3], [pascal.charest@crc.ca])
AC_CONFIG_AUX_DIR([build-aux])
AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE([-Wall])
AC_CONFIG_SRCDIR([src/DabMod.cpp])
AM_CONFIG_HEADER([config.h])

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

EXTRA=""
AC_ARG_ENABLE([debug],
        [AS_HELP_STRING([--disable-debug], [Disable debugger symbols])],
        [], [enable_debug=yes])
AC_ARG_ENABLE([prof],
        [AS_HELP_STRING([--enable-prof], [Enable profiling])],
        [], [enable_prof=no])
AC_ARG_WITH([debug-malloc],
        [AS_HELP_STRING([--with-debug-malloc[=yes|no|duma|efence|...]],
            [Add malloc debugger support])],
        [], [with_debug_malloc=yes])
AC_ARG_ENABLE([trace],
        [AS_HELP_STRING([--enable-trace], [Enable trace output])],
        [], [enable_trace=no])
AC_ARG_ENABLE([fft_simd],
        [AS_HELP_STRING([--enable-fft-simd], [Enable SIMD instructions for kiss-fft (unstable)])],
        [], [enable_fft_simd=no])
AS_IF([test "x$enable_debug" = "xno"],
        [OPTIM="-O2" DEBUG="" EXTRA="$EXTRA -DNDEBUG"],
        [OPTIM="-O0" DEBUG="-ggdb" EXTRA="$EXTRA -Wall"])
AS_IF([test "x$enable_prof" != "xno"],
        [EXTRA="$EXTRA -pg"])
AS_IF([test "x$enable_trace" != "xno"],
        [EXTRA="$EXTRA -DDEBUG"])
AS_IF([test "x$enable_fft_simd" != "xno"],
        [EXTRA="$EXTRA -DUSE_SIMD"])

AC_SUBST([CFLAGS], ["$OPTIM $DEBUG $EXTRA"])
AC_SUBST([CXXFLAGS], ["$OPTIM $DEBUG $EXTRA"])


# Checks for libraries.
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

# Checks for library functions.
# Commented to remove rpl_malloc linking error
#AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_REALLOC
AC_TYPE_SIGNAL
AC_CHECK_FUNCS([bzero floor ftime gettimeofday memset sqrt strchr strerror strtol])

AC_CONFIG_FILES([Makefile
                 lib/Makefile
                 src/Makefile])
AC_OUTPUT
