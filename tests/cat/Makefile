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

CAT_SRCS = $(SRCDIR)/cat_civ.c $(SRCDIR)/cat_ascii.c $(SRCDIR)/cat_state.c $(SRCDIR)/cat_trace.c
CAT_HDRS = $(SRCDIR)/cat.h $(SRCDIR)/cat_civ.h $(SRCDIR)/cat_ascii.h $(SRCDIR)/cat_state.h \
	   $(SRCDIR)/cat_trace.h $(SRCDIR)/mhcontrol.h

all: cat_replay native_test router_test

cat_replay: cat_replay.c $(CAT_SRCS) $(CAT_HDRS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ cat_replay.c $(CAT_SRCS)

# cat_native.c with router, mhcontrol and event bus stubbed (needs libev), with short
# keepalive and own query intervals.
NATIVE_TEST_DEFS = -DCATN_KEEPALIVE=0.5 -DCATN_OWN_STALE=0.6 -DCATN_OWN_RETRY=2.0 -DLOGDIR=\"/nonexistent\"
NATIVE_SRCS = stubs.c $(SRCDIR)/cat_native.c $(SRCDIR)/radiotypes.c $(CAT_SRCS)
NATIVE_DEPS = stubs.h $(NATIVE_SRCS) $(SRCDIR)/cat_native.h $(SRCDIR)/mhrouter.h $(CAT_HDRS)
native_test: native_test.c $(NATIVE_DEPS)
	$(CC) $(CPPFLAGS) $(NATIVE_TEST_DEFS) $(CFLAGS) -o $@ native_test.c $(NATIVE_SRCS) -lev

# The same with the real router, emulated keyer, radio and client on socketpairs.
ROUTER_SRCS = $(SRCDIR)/mhrouter.c $(SRCDIR)/mux.c $(SRCDIR)/demux.c $(SRCDIR)/buffer.c $(SRCDIR)/pglist.c
router_test: router_test.c $(NATIVE_DEPS) $(ROUTER_SRCS)
	$(CC) $(CPPFLAGS) $(NATIVE_TEST_DEFS) $(CFLAGS) -o $@ router_test.c $(NATIVE_SRCS) $(ROUTER_SRCS) -lev

check: cat_replay native_test router_test
	./cat_replay traces/*.trace
	rm -rf monitor-out && mkdir monitor-out
	./native_test monitor-out
	./cat_replay monitor-out/cat-TEST0001-r1.trace
	./router_test

clean:
	rm -rf cat_replay native_test router_test monitor-out

.PHONY: all check clean
