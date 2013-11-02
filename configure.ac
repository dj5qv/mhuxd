AC_PREREQ([2.69])
AC_INIT([mhuxd], [0.50rc1], [mhuxd@maxec.de])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

AC_PROG_CC
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
 Makefile
 src/Makefile
 src/clearsilver/Makefile
 webui/Makefile
])

AC_PROG_CC
AC_PROG_MAKE_SET

AC_PROG_RANLIB

AC_CHECK_HEADERS([fcntl.h limits.h netdb.h netinet/in.h stddef.h stdint.h stdlib.h string.h sys/file.h sys/ioctl.h sys/socket.h sys/time.h termios.h unistd.h libintl.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_INT8_T
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# PKG-CONFIG
AC_CHECK_PROG(PKGCONF,pkg-config,yes)
if test x"$PKGCONF" != x"yes" ; then
    AC_MSG_ERROR([pkg-config not found. Install 'pkg-config'.])
fi

#FUSE
PKG_CHECK_MODULES(FUSE,fuse,[ have_fuse=true ],[
   AC_MSG_ERROR([FUSE: no FUSE pkg-config(1) module was found. Install 'libfuse-dev' package.])
])

FUSE_CFLAGS="$FUSE_CFLAGS -DFUSE_USE_VERSION=28"
AC_SUBST(FUSE_CFLAGS)
AC_SUBST(FUSE_LIBS)
LIBS="${FUSE_LIBS} ${LIBS}"

#UDEV
PKG_CHECK_MODULES(UDEV,libudev,[ have_udev=true ],[
   AC_MSG_ERROR([UDEV: no UDEV pkg-config(1) module was found. Install 'libudev-dev' package.])
])
 
UDEV_CFLAGS="$UDEV_CFLAGS"
AC_SUBST(UDEV_CFLAGS)
AC_SUBST(UDEV_LIBS) 
LIBS="${UDEV_LIBS} ${LIBS}"

#EV
AC_CHECK_HEADERS([ev.h], [], [AC_MSG_ERROR("ev.h not found. Install 'libev-dev'.")])
AC_CHECK_LIB([ev], [ev_time], [
        LIBS="-lev ${LIBS}"
        AC_DEFINE([HAVE_LIBEV], [1], [ev_time in -lev])
        ], [AC_MSG_ERROR("libev not found. Install 'libev4'")])

EV_LIBS="-lev"

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([bzero dup2 gethostbyname gethostname gettimeofday localtime_r memchr memmove memset])
AC_CHECK_FUNCS([mkdir regcomp rmdir socket stpcpy strcasecmp strchr strdup strerror strncasecmp strpbrk])
AC_CHECK_FUNCS([strrchr strstr strtol strtoul vsnprintf snprintf mkstemp gmtime_r localtime_r strtok_r udev_monitor_filter_add_match_subsystem_devtype])


# Add logdir option
LOGDIR='${localstatedir}/log/mhuxd'
AC_ARG_WITH([logdir],
	[  --with-logdir=DIR       log files [[LOCALSTATEDIR/log/mhuxd]]],
	LOGDIR=$withval)
if test ! -z "$LOGDIR"; then
  if test "$LOGDIR" = "yes" -o "$LOGDIR" = "no"; then
    AC_MSG_ERROR([--with-logdir must specify a path])
  fi
fi
AC_SUBST(LOGDIR)

# Add statedir option
STATEDIR='${localstatedir}/lib/mhuxd'
AC_ARG_WITH([statedir],
        [  --with-statedir=DIR     local state [[LOCALSTATEDIR/lib/mhuxd]]],
        STATEDIR=$withval)
if test ! -z "$STATEDIR"; then
  if test "$STATEDIR" = "yes" -o "$STATEDIR" = "no"; then
    AC_MSG_ERROR([--with-statedir must specify a path])
  fi
fi
AC_SUBST(STATEDIR)

# Add rundir option
RUNDIR='${localstatedir}/run/mhuxd'
AC_ARG_WITH([rundir],
        [  --with-rundir=DIR       runtime files [[LOCALSTATEDIR/run/mhuxd]]],
        RUNDIR=$withval)
if test ! -z "$RUNDIR"; then
  if test "$RUNDIR" = "yes" -o "$RUNDIR" = "no"; then
    AC_MSG_ERROR([--with-rundir must specify a path])
  fi
fi
AC_SUBST(RUNDIR)

# Add webuidir option
WEBUIDIR='${datadir}/mhuxd/webui'
AC_ARG_WITH([webuidir],
        [  --with-webuidir=DIR     webui files [[DATADIR/mhuxd/webui]]],
        RUNDIR=$withval)
if test ! -z "$WEBUIDIR"; then
  if test "$WEBUIDIR" = "yes" -o "$WEBUIDIR" = "no"; then
    AC_MSG_ERROR([--with-webuidir must specify a path])
  fi
fi
AC_SUBST(WEBUIDIR)

# Add hdfdir option
HDFDIR='${datadir}/mhuxd/hdf'
AC_ARG_WITH([hdfdir],
        [  --with-hdfdir=DIR       hdf files [[DATADIR/mhuxd/hdf]]],
        RUNDIR=$withval)
if test ! -z "$HDFDIR"; then
  if test "$HDFDIR" = "yes" -o "$HDFDIR" = "no"; then
    AC_MSG_ERROR([--with-hdfdir must specify a path])
  fi
fi
AC_SUBST(HDFDIR)


# finish
AC_OUTPUT

