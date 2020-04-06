# Copyright (C) 2007, 2008, 2009, 2010, 2011, 2012 Her Majesty the
# Queen in Right of Canada (Communications Research Center Canada)

# Copyright (C) 2020 Matthias P. Braendli, http://opendigitalradio.org

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

AC_PREREQ(2.69)
AC_INIT([ODR-DabMod], [2.4.1], [matthias.braendli@mpb.li])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE([-Wall foreign subdir-objects])
AC_CONFIG_SRCDIR([src/DabMod.cpp])
AC_CONFIG_HEADER([config.h])
AM_SILENT_RULES([yes])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL

AX_CXX_COMPILE_STDCXX_11(noext,mandatory)

EXTRA=""
AC_ARG_ENABLE([prof],
        [AS_HELP_STRING([--enable-prof], [Enable profiling])],
        [], [enable_prof=no])
AC_ARG_ENABLE([fast-math],
        [AS_HELP_STRING([--enable-fast-math], [Set -ffast-math])],
        [], [enable_fast_math=no])
AC_ARG_ENABLE([trace],
        [AS_HELP_STRING([--enable-trace], [Enable trace output])],
        [], [enable_trace=no])
AC_ARG_ENABLE([zeromq],
        [AS_HELP_STRING([--disable-zeromq], [Disable ZeroMQ input, output and remote control])],
        [], [enable_zeromq=yes])
AC_ARG_ENABLE([native],
        [AS_HELP_STRING([--disable-native], [Do not compile with -march=native])],
        [], [enable_native=yes])
AC_ARG_ENABLE([easydabv3],
        [AS_HELP_STRING([--enable-easydabv3], [Build for EasyDABv3 board])],
        [], [enable_easydabv3=no])
AC_ARG_ENABLE([limesdr],
        [AS_HELP_STRING([--enable-limesdr], [Build for LimeSDR board])],
        [], [enable_limesdr=no])

# UHD support control
AC_ARG_ENABLE([output_uhd],
        [AS_HELP_STRING([--disable-output-uhd], [Disable UHD output])],
        [], [enable_output_uhd=yes])

AC_LANG_PUSH([C++])
AX_CHECK_COMPILE_FLAG([-Wduplicated-cond], [CXXFLAGS="$CXXFLAGS -Wduplicated-cond"], [], ["-Werror"])
AX_CHECK_COMPILE_FLAG([-Wduplicated-branches], [CXXFLAGS="$CXXFLAGS -Wduplicated-branches"], [], ["-Werror"])
AX_CHECK_COMPILE_FLAG([-Wlogical-op], [CXXFLAGS="$CXXFLAGS -Wlogical-op"], [], ["-Werror"])
AX_CHECK_COMPILE_FLAG([-Wrestrict], [CXXFLAGS="$CXXFLAGS -Wrestrict"], [], ["-Werror"])
AX_CHECK_COMPILE_FLAG([-Wno-pragmas], [CXXFLAGS="$CXXFLAGS -Wno-pragmas"], [], ["-Werror"])
AX_CHECK_COMPILE_FLAG([-Wdouble-promotion], [CXXFLAGS="$CXXFLAGS -Wdouble-promotion"], [], ["-Werror"])
AX_CHECK_COMPILE_FLAG(["-Wformat=2"], [CXXFLAGS="$CXXFLAGS -Wformat=2"], [], ["-Werror"])
AC_LANG_POP([C++])


AS_IF([test "x$enable_easydabv3" = "xno"],
    [PKG_CHECK_MODULES([FFTW], [fftw3f], [], [AC_MSG_ERROR([FFTW is required])])])

echo "Checking zeromq"

AS_IF([test "x$enable_zeromq" = "xyes"],
         [AC_CHECK_LIB([zmq], [zmq_init], [ZMQ_LIBS="-lzmq"],
                       [AC_MSG_ERROR([ZeroMQ libzmq is required])])])

AS_IF([test "x$enable_zeromq" = "xyes"],
        [AC_DEFINE(HAVE_ZEROMQ, [1], [Define if ZeroMQ is enabled])])

AS_IF([test "x$enable_prof" != "xno"],
        [EXTRA="$EXTRA -pg"])

AS_IF([test "x$enable_fast_math" != "xno"],
        [EXTRA="$EXTRA -ffast-math"])

AS_IF([test "x$enable_trace" != "xno"],
        AC_DEFINE(TRACE, [1], [Enable trace output for all blocks]))

# Define conditionals for Makefile.am
AM_CONDITIONAL([IS_GIT_REPO], [test -d '.git'])
AM_CONDITIONAL([COMPILE_FOR_EASYDABV3], [test "x$enable_easydabv3" = "xyes"])

# Defines for config.h
AX_PTHREAD([], AC_MSG_ERROR([requires pthread]))

AS_IF([test "x$enable_easydabv3" = "xno"],
    [PKG_CHECK_MODULES([SOAPYSDR], [SoapySDR], enable_soapysdr=yes, enable_soapysdr=no)])

AS_IF([test "x$enable_limesdr" = "xyes"],
         [AC_CHECK_LIB([LimeSuite], [LMS_Init], [LIMESDR_LIBS="-lLimeSuite"],
                       [AC_MSG_ERROR([LimeSDR LimeSuite is required])])])

AC_SUBST([CFLAGS], ["$CFLAGS $EXTRA $FFTW_CFLAGS $SOAPYSDR_CFLAGS $PTHREAD_CFLAGS"])
AC_SUBST([CXXFLAGS], ["$CXXFLAGS $EXTRA $FFTW_CFLAGS $SOAPYSDR_CFLAGS $PTHREAD_CFLAGS"])
AC_SUBST([LIBS], ["$FFTW_LIBS $SOAPYSDR_LIBS $PTHREAD_LIBS $ZMQ_LIBS $LIMESDR_LIBS"])

AS_IF([test "x$enable_easydabv3" = "xyes" && test "x$enable_output_uhd" == "xyes"],
      AC_MSG_ERROR([Cannot enable both EasyDABv3 and UHD output]))

# Checks for UHD.
AS_IF([test "x$enable_output_uhd" = "xyes"],
      [ PKG_CHECK_MODULES([UHD], [uhd], [], [AC_MSG_ERROR([UHD is required])])
      ])

AS_IF([test "x$enable_output_uhd" = "xyes"],
      [AC_DEFINE(HAVE_OUTPUT_UHD, [1], [Define if UHD output is enabled])])

# I don't know why this is needed, I think the uhd pkg-config file should also
# tell us to link against boost thread, but it doesn't.
AS_IF([test "x$enable_output_uhd" = "xyes"],
    [ AX_BOOST_BASE([1.55.0], [], AC_MSG_ERROR([BOOST 1.55 or later is required]))
      AX_BOOST_THREAD ])

AS_IF([test "x$enable_soapysdr" = "xyes"],
      [AC_DEFINE(HAVE_SOAPYSDR, [1], [Define if SoapySDR output is enabled])])

AS_IF([test "x$enable_limesdr" = "xyes"],
      [AC_DEFINE(HAVE_LIMESDR, [1], [Define if LimeSDR input is enabled]) ])

AS_IF([test "x$enable_easydabv3" = "xyes"],
      AC_DEFINE(BUILD_FOR_EASYDABV3, [1], [Define if we are building for EasyDABv3]))


# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h limits.h memory.h netinet/in.h stdint.h stdlib.h string.h sys/time.h sys/timeb.h unistd.h])

AC_MSG_CHECKING(for M_PIl existence)
AC_LANG_PUSH([C++])
AC_COMPILE_IFELSE(
        [AC_LANG_PROGRAM([[#include <math.h>]], [[double pi = M_PIl;]])],
        [AC_MSG_RESULT([yes])], [AC_MSG_RESULT([no]) AC_DEFINE([M_PIl], [M_PI], [Replacing define])])
AC_LANG_POP([C++])

# Linux has prctl to set thread names
AC_MSG_CHECKING(for prctl and PR_SET_NAME)
AC_COMPILE_IFELSE([ AC_LANG_PROGRAM([[
                    #include <sys/prctl.h>
                    void set_thread_name() {
                        prctl(PR_SET_NAME,"test",0,0,0);
                    }
                    ]])],
                   [ AC_MSG_RESULT(yes)
                     AC_DEFINE(HAVE_PRCTL, 1, [Define this symbol if you have prctl and PR_SET_NAME]) ],
                   [ AC_MSG_RESULT(no) ])

# Linux defines MSG_NOSIGNAL, some other systems have SO_NOSIGPIPE instead
AC_MSG_CHECKING(for MSG_NOSIGNAL)
AC_COMPILE_IFELSE([ AC_LANG_PROGRAM([[
                    #include <sys/socket.h>
                    int f = MSG_NOSIGNAL;
                    ]])],
                   [ AC_MSG_RESULT(yes)
                     AC_DEFINE(HAVE_MSG_NOSIGNAL, 1, [Define this symbol if you have MSG_NOSIGNAL]) ],
                   [ AC_MSG_RESULT(no) ])

AC_MSG_CHECKING(for SO_NOSIGPIPE)
AC_COMPILE_IFELSE([ AC_LANG_PROGRAM([[
                    #include <sys/socket.h>
                    int f = SO_NOSIGPIPE;
                    ]])],
                   [ AC_MSG_RESULT(yes)
                     AC_DEFINE(HAVE_SO_NOSIGPIPE, 1, [Define this symbol if you have SO_NOSIGPIPE]) ],
                   [ AC_MSG_RESULT(no) ])

# Check for march
AS_IF([test "x$enable_native" = "xyes"],
      [AC_MSG_CHECKING(if we can add -march=native to CFLAGS)
       save_CXXFLAGS="$CXXFLAGS"
       CXXFLAGS="$CXXFLAGS -march=native"
       AC_COMPILE_IFELSE(  [AC_LANG_PROGRAM([[
                            void testfunc(void) {}
                            ]])],
                            [supports_march_native=yes],
                            [supports_march_native=no]
                            )
       AC_MSG_RESULT($supports_march_native)
       if test x"$supports_march_native" = xno; then
           CXXFLAGS="$save_CXXFLAGS"
       fi
      ])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

echo
echo "***********************************************"
echo
enabled=""
disabled=""
for feat in prof trace output_uhd zeromq soapysdr easydabv3 limesdr
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
for feat in supports_march_native enable_fast_math
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
