# Native CAT decoder tests (docs/task_native_cat.md).
#
# The decoders are pure code, so the replay tool is built straight from the
# sources, no mhuxd build needed:
#
#   make -C tests/cat check
#
# Traces in traces/ hold CAT bytes plus '=' assertion lines, see README.md.

SRCDIR   ?= ../../src
CPPFLAGS += -I$(SRCDIR) -D_GNU_SOURCE
CFLAGS   ?= -g -O2 -Wall -Wextra

CAT_SRCS = $(SRCDIR)/cat_civ.c $(SRCDIR)/cat_state.c
CAT_HDRS = $(SRCDIR)/cat_civ.h $(SRCDIR)/cat_state.h $(SRCDIR)/mhcontrol.h

all: cat_replay native_test

cat_replay: cat_replay.c $(CAT_SRCS) $(CAT_HDRS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ cat_replay.c $(CAT_SRCS)

# cat_native.c with router, mhcontrol and event bus stubbed (needs libev), with a short
# keepalive interval.
native_test: native_test.c $(SRCDIR)/cat_native.c $(SRCDIR)/cat_native.h $(SRCDIR)/radiotypes.c $(CAT_SRCS) $(CAT_HDRS)
	$(CC) $(CPPFLAGS) -DCATN_KEEPALIVE=0.5 $(CFLAGS) -o $@ native_test.c $(SRCDIR)/cat_native.c $(SRCDIR)/radiotypes.c $(CAT_SRCS) -lev

check: cat_replay native_test
	./cat_replay traces/*.trace
	./native_test

clean:
	rm -f cat_replay native_test

.PHONY: all check clean
