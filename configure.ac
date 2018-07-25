AC_PREREQ([2.69])
AC_INIT([slip-forwarder], [0.1], [giuseppe@scrivano.org])
AC_CONFIG_SRCDIR([main.c])
AC_CONFIG_HEADERS([config.h])

AC_CONFIG_AUX_DIR([build-aux])

AM_INIT_AUTOMAKE([1.9 foreign])

AC_PROG_CC

AC_CHECK_LIB([slirp], [slirp_start])
AC_CHECK_HEADERS([arpa/inet.h fcntl.h stdlib.h string.h sys/ioctl.h sys/mount.h sys/socket.h sys/time.h unistd.h])

AC_PROG_RANLIB

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
