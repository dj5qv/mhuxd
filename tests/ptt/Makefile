# PTT tests.
#
# router_ptt_test drives the real router (src/mhrouter.c) with an emulated keyer and
# connectors on socketpairs, built straight from the sources, no mhuxd build needed:
#
#   make -C tests/ptt check
#
# ptt_hold.py holds PTT through a TCP PTT connector of a running mhuxd.

SRCDIR   ?= ../../src
CPPFLAGS += -I$(SRCDIR) -D_GNU_SOURCE
CFLAGS   ?= -g -O2 -Wall -Wextra

ROUTER_SRCS = $(SRCDIR)/mhrouter.c $(SRCDIR)/mux.c $(SRCDIR)/demux.c $(SRCDIR)/buffer.c $(SRCDIR)/pglist.c
ROUTER_HDRS = $(SRCDIR)/mhrouter.h $(SRCDIR)/mhflags.h $(SRCDIR)/ptt_byte.h $(SRCDIR)/channel.h

all: router_ptt_test

router_ptt_test: router_ptt_test.c $(ROUTER_SRCS) $(ROUTER_HDRS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ router_ptt_test.c $(ROUTER_SRCS) -lev

check: router_ptt_test
	./router_ptt_test

clean:
	rm -f router_ptt_test

.PHONY: all check clean
