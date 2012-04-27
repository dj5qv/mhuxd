AC_INIT([mhuxd], [0.30], [mhuxd@maxec.de])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_PROG_CC
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
 Makefile
 src/Makefile
])

AC_PROG_CC
AC_PROG_MAKE_SET

AC_CHECK_HEADERS([arpa/inet.h fcntl.h stddef.h stdint.h stdlib.h string.h strings.h sys/ioctl.h sys/socket.h sys/time.h syslog.h termios.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

#FUSE
PKG_CHECK_MODULES(FUSE,fuse,[ have_fuse=true ],[
   AC_MSG_ERROR([FUSE: no FUSE pkg-config(1) module was found. Install 'libfuse-dev' package.])
])

FUSE_CFLAGS="$FUSE_CFLAGS -DFUSE_USE_VERSION=28"
AC_SUBST(FUSE_CFLAGS)
AC_SUBST(FUSE_LIBS)

#UDEV
PKG_CHECK_MODULES(UDEV,libudev,[ have_udev=true ],[
   AC_MSG_ERROR([UDEV: no UDEV pkg-config(1) module was found. Install 'libudev-dev' package.])
])
 
UDEV_CFLAGS="$UDEV_CFLAGS"
AC_SUBST(UDEV_CFLAGS)
AC_SUBST(UDEV_LIBS) 



# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_CHECK_FUNCS([bzero gettimeofday localtime_r memset strcasecmp strdup strerror strrchr strstr])

AC_OUTPUT

