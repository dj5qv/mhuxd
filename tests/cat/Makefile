# Native CAT decoder tests (docs/task_native_cat.md).
#
# The decoders are pure code, so the replay tool is built straight from the
# sources, no mhuxd build needed:
#
#   make -C tests/cat check
#
# Traces in traces/ hold CAT bytes plus '=' assertion lines, see README.md.

SRCDIR   ?= ../../src
CPPFLAGS += -I$(SRCDIR)
CFLAGS   ?= -g -O2 -Wall -Wextra

CAT_SRCS = $(SRCDIR)/cat_civ.c $(SRCDIR)/cat_state.c
CAT_HDRS = $(SRCDIR)/cat_civ.h $(SRCDIR)/cat_state.h $(SRCDIR)/mhcontrol.h

all: cat_replay

cat_replay: cat_replay.c $(CAT_SRCS) $(CAT_HDRS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ cat_replay.c $(CAT_SRCS)

check: cat_replay
	./cat_replay traces/*.trace

clean:
	rm -f cat_replay

.PHONY: all check clean
