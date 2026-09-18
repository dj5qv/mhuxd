# Standalone build for the VSP test harness.
#
# con_vsp.c does not depend on conmgr/mhrouter/devmgr, so the harness only needs
# a handful of objects from the normal build. Build mhuxd first, then:
#
#   make -C tests/vsp
#   sudo ./tests/vsp/run_tests.py
#
# BUILDDIR can point at another build tree if you do not use ../../build.

BUILDDIR ?= ../../build
SRCDIR   ?= ../../src
OBJDIR   ?= $(BUILDDIR)/src

OBJS = \
	$(OBJDIR)/mhuxd-con_vsp.o \
	$(OBJDIR)/mhuxd-buffer.o \
	$(OBJDIR)/mhuxd-pglist.o \
	$(OBJDIR)/mhuxd-util.o \
	$(OBJDIR)/mhuxd-logger.o \
	$(OBJDIR)/mhuxd-linux_termios.o

CPPFLAGS += -I$(SRCDIR) -I$(BUILDDIR) -I/usr/include/fuse
CPPFLAGS += -D_GNU_SOURCE -DEV_NO_THREADS -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=28
CFLAGS   ?= -g -O2 -Wall
LDLIBS   += -lfuse -lev -ljansson

all: vsp_harness

vsp_harness: vsp_harness.c $(OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< $(OBJS) $(LDLIBS)

$(OBJS):
	@echo "Missing $@ - build mhuxd first (make -C $(BUILDDIR))." >&2
	@false

clean:
	rm -f vsp_harness

.PHONY: all clean
