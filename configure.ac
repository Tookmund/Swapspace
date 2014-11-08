#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([swapspace], [1.11], [tookmund@gmail.com])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src])
AC_CONFIG_HEADERS([src/config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h limits.h locale.h stdlib.h string.h sys/param.h sys/statvfs.h syslog.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_REALLOC
AC_CHECK_FUNCS([atexit getcwd getpagesize memset setlocale strchr strerror strtol strtoul])

# Check for swapon and swapoff
AC_CHECK_FUNC([swapon],[
		AC_DEFINE([HAVE_SWAPON],[],[Define if system has swapon ()])
		])
		
AC_CHECK_FUNC([swapoff],[
		AC_DEFINE([HAVE_SWAPOFF],[],[Define if system has swapoff()])
		])
# Define Version and Date
# Will be removed by release!!
#AC_DEFINE([VERSION],[AC_PACKAGE_VERSION],[Current Version Number])
#AC_DEFINE_UNQUOTED([DATE],[`date`],[Date of Compile])

AC_CONFIG_FILES([Makefile
                 src/Makefile])
AC_OUTPUT