SOURCES += \
    src/main.c \
    src/pglist.c \
    src/logger.c \
    src/opts.c \
    src/vsp.c \
    src/buffer.c \
    src/dispatcher.c \
    src/tty.c \
    src/ctl.c \
    src/launcher.c \
    src/mhinfo.c \
    src/mux.c \
    src/demux.c \
    src/ctlmsg.c \
    src/util.c \
    src/cfgfile.c \
    src/kcfg.c \
    src/radiotypes.c \
    src/linux_termios.c \
    src/linux_udev.c
QMAKE_CFLAGS += -Wno-unused-parameter -Wno-unused-function
CONFIG += link_pkgconfig
PKGCONFIG += fuse
HEADERS += \
    src/pglist.h \
    src/logger.h \
    src/opts.h \
    src/vsp.h \
    src/logger.h \
    src/global.h \
    src/buffer.h \
    src/dispatcher.h \
    src/mhproto.h \
    src/tty.h \
    src/ctl.h \
    src/launcher.h \
    src/mhinfo.h \
    src/demux.h \
    src/mux.h \
    src/ctlmsg.h \
    src/util.h \
    src/cfgfile.h \
    src/kcfg.h \
    src/radiotypes.h \
    src/linux_termios.h \
    src/linux_udev.h

OTHER_FILES += \
    doc/COPYING \
    lic.template \
    doc/README \
    make-release.sh \
    VERSION \
    doc/CHANGES \
    man/mhuxd.8 \
    configure.ac \
    Makefile.am \
    src/Makefile.am \
    debian/source/format \
    debian/rules.dh7 \
    debian/rules \
    debian/README \
    debian/manpage.8 \
    debian/docs \
    debian/copyright \
    debian/control \
    debian/compat \
    debian/changelog \
    etc/mhuxd.conf \
    debian/mhuxd.init \
    udev/59-mhuxd.rules \
    debian/postinst \
    debian/postrm \
    configure

LIBS += -ludev

