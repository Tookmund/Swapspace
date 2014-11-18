#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([swapspace], [1.11], [tookmund@gmail.com],[swapspace],[https://github.com/Tookmund/swapspace])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src])
AC_CONFIG_HEADERS([src/config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MKDIR_P

# Enable large files
AC_SYS_LARGEFILE

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

# Check args for NO_CONFIG and NDEBUG
AC_ARG_ENABLE([noconfig],[
		AS_HELP_STRING([--enable-noconfig],
		[Enable ultra-lightweight unconfigurable version]
		)],
		[
		AC_DEFINE([NO_CONFIG],[],[Ultra-lightweight unconfigurable version])
		],
		[])

AC_ARG_ENABLE([nodebug],[
		AS_HELP_STRING([--enable-nodebug],
		[Disable assertions and consistancy checking]
		)],
		[
		AC_DEFINE([NDEBUG],[],[Disable assertions and consistancy checking])
		],
		[])
		
AC_CONFIG_FILES([Makefile
                 src/Makefile])
AC_OUTPUT
