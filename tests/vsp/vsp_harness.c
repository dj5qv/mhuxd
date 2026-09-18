/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2026  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

/*
 * vsp_harness - drives con_vsp.c standalone, without the rest of the daemon.
 *
 * con_vsp.c only needs a socketpair and a libev loop, so the harness creates the
 * pair, hands the connector side to vsp_create() and keeps the router side for
 * itself. Anything mhrouter would normally do to that fd, the harness does on
 * command - including the things a well behaved router never does, like refusing
 * to read so the connector's buffers fill up.
 *
 * Commands arrive as lines on stdin, one response line per command on stdout.
 * The logger is pointed at a file so it cannot corrupt the protocol.
 *
 *   ping                 -> ok
 *   send <hex>           write bytes to the router end        -> ok <n>
 *   flood <n>            queue n bytes of a 0..255 ramp       -> ok <n>
 *   paced <bps> <ms>     emit a ramp at <bps> bytes/s         -> ok <total>
 *   recv                 take everything received so far      -> data <hex>
 *   pending              bytes received but not taken yet     -> ok <n>
 *   sink on|off          count+discard instead of buffering   -> ok
 *   stats                                -> ok <sent> <received> <seqerr> <queued>
 *   reset                zero the counters                    -> ok
 *   stopreading          stop draining the router end         -> ok
 *   resume               resume draining                      -> ok
 *   shutdown             close the router end                 -> ok
 *   quit                 destroy the vsp and exit             -> ok
 *
 * usage: vsp_harness <devname> [maxcon]
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <ev.h>
#include "conmgr.h"
#include "con_vsp.h"
#include "logger.h"

/* Keep the socket buffers small so a client can fill them without shifting
 * megabytes. The connector's own struct buffer is 512 bytes, so this keeps
 * backpressure tests quick and deterministic. */
#define SOCK_BUF_SIZE 1024

struct dynbuf {
	unsigned char *data;
	size_t size;
	size_t rpos;
	size_t capacity;
};

struct harness {
	struct ev_loop *loop;
	struct vsp *vsp;
	int router_fd;
	int reading;

	ev_io w_router_in;
	ev_io w_router_out;
	ev_io w_cmd;

	struct dynbuf rx;	/* received from the connector, waiting for "recv" */
	struct dynbuf tx;	/* queued by "send"/"flood", waiting for the socket */
	unsigned char cmd[4096];
	size_t cmd_len;
	unsigned char ramp;	/* next value the flood generator emits */

	/* paced generator: emit a ramp at a fixed rate, self correcting against
	 * timer jitter by deriving the target from elapsed time. */
	ev_timer w_pace;
	double pace_start;
	double pace_bps;
	long pace_total;
	long pace_emitted;

	/* counters, for the throughput ramp */
	unsigned long long sent;
	unsigned long long received;
	unsigned long long seq_errors;
	int sink;		/* count and discard instead of buffering */
	int sink_primed;
	unsigned char sink_next;
};

static void buf_need(struct dynbuf *b, size_t extra) {
	if(b->size + extra <= b->capacity)
		return;
	size_t want = b->capacity ? b->capacity : 4096;
	while(want < b->size + extra)
		want *= 2;
	b->data = realloc(b->data, want);
	if(!b->data) {
		fprintf(stderr, "harness: out of memory\n");
		exit(1);
	}
	b->capacity = want;
}

static void buf_put(struct dynbuf *b, const unsigned char *p, size_t len) {
	buf_need(b, len);
	memcpy(b->data + b->size, p, len);
	b->size += len;
}

static void buf_took(struct dynbuf *b, size_t len) {
	b->rpos += len;
	if(b->rpos == b->size)
		b->rpos = b->size = 0;
}

static void reply(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	putchar('\n');
	fflush(stdout);
}

static int hex_to_bin(const char *hex, unsigned char *out, size_t out_size, size_t *len) {
	size_t n = strlen(hex);
	size_t i;

	if(n % 2 || n / 2 > out_size)
		return -1;

	for(i = 0; i < n; i += 2) {
		unsigned int byte;
		if(sscanf(hex + i, "%2x", &byte) != 1)
			return -1;
		out[i / 2] = (unsigned char)byte;
	}
	*len = n / 2;
	return 0;
}

static void print_hex(const unsigned char *p, size_t len) {
	size_t i;
	printf("data ");
	for(i = 0; i < len; i++)
		printf("%02x", p[i]);
	putchar('\n');
	fflush(stdout);
}

static void router_out_cb(struct ev_loop *loop, ev_io *w, int revents);

static void tx_kick(struct harness *h) {
	if(h->tx.rpos < h->tx.size && h->router_fd != -1)
		ev_io_start(h->loop, &h->w_router_out);
}

static void router_in_cb(struct ev_loop *loop, ev_io *w, int revents) {
	(void)loop; (void)revents;
	struct harness *h = w->data;
	unsigned char buf[4096];

	for(;;) {
		ssize_t r = read(w->fd, buf, sizeof(buf));
		if(r > 0) {
			h->received += (size_t)r;
			if(h->sink) {
				/* verify the client's ramp without storing anything */
				ssize_t i;
				for(i = 0; i < r; i++) {
					if(h->sink_primed && buf[i] != h->sink_next)
						h->seq_errors++;
					h->sink_next = buf[i] + 1;
					h->sink_primed = 1;
				}
			} else {
				buf_put(&h->rx, buf, (size_t)r);
			}
			continue;
		}
		if(r == 0) {
			/* connector closed its end */
			ev_io_stop(h->loop, &h->w_router_in);
			return;
		}
		if(errno == EAGAIN || errno == EWOULDBLOCK)
			return;
		if(errno == EINTR)
			continue;
		ev_io_stop(h->loop, &h->w_router_in);
		return;
	}
}

static void router_out_cb(struct ev_loop *loop, ev_io *w, int revents) {
	(void)loop; (void)revents;
	struct harness *h = w->data;

	while(h->tx.rpos < h->tx.size) {
		ssize_t n = write(w->fd, h->tx.data + h->tx.rpos, h->tx.size - h->tx.rpos);
		if(n > 0) {
			h->sent += (size_t)n;
			buf_took(&h->tx, (size_t)n);
			continue;
		}
		if(n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
			return;
		if(n < 0 && errno == EINTR)
			continue;
		/* give up on this queue, the test will notice */
		h->tx.rpos = h->tx.size = 0;
		break;
	}
	ev_io_stop(h->loop, &h->w_router_out);
}

static void queue_ramp(struct harness *h, long n) {
	long i;
	buf_need(&h->tx, (size_t)n);
	for(i = 0; i < n; i++)
		h->tx.data[h->tx.size + i] = h->ramp++;
	h->tx.size += (size_t)n;
	tx_kick(h);
}

/* Derive how much should have been emitted by now from the elapsed time, so a
 * late timer tick catches up instead of shifting the whole schedule. */
static void pace_cb(struct ev_loop *loop, ev_timer *w, int revents) {
	(void)revents;
	struct harness *h = w->data;
	double elapsed = ev_now(loop) - h->pace_start;
	long target = (long)(elapsed * h->pace_bps);

	if(target > h->pace_total)
		target = h->pace_total;
	if(target > h->pace_emitted) {
		queue_ramp(h, target - h->pace_emitted);
		h->pace_emitted = target;
	}
	if(h->pace_emitted >= h->pace_total)
		ev_timer_stop(loop, w);
}

static void handle_cmd(struct harness *h, char *line) {
	char *arg = strchr(line, ' ');
	if(arg)
		*arg++ = 0;

	if(!strcmp(line, "ping")) {
		reply("ok");

	} else if(!strcmp(line, "send")) {
		unsigned char bin[2048];
		size_t len;
		if(!arg || hex_to_bin(arg, bin, sizeof(bin), &len)) {
			reply("err badhex");
			return;
		}
		buf_put(&h->tx, bin, len);
		tx_kick(h);
		reply("ok %zu", len);

	} else if(!strcmp(line, "flood")) {
		long n = arg ? atol(arg) : 0;
		if(n <= 0) {
			reply("err badcount");
			return;
		}
		queue_ramp(h, n);
		reply("ok %ld", n);

	} else if(!strcmp(line, "paced")) {
		double bps = 0;
		long ms = 0;
		if(!arg || sscanf(arg, "%lf %ld", &bps, &ms) != 2 || bps <= 0 || ms <= 0) {
			reply("err badargs");
			return;
		}
		h->pace_bps = bps;
		h->pace_total = (long)(bps * ms / 1000.0);
		h->pace_emitted = 0;
		h->pace_start = ev_now(h->loop);
		ev_timer_again(h->loop, &h->w_pace);
		reply("ok %ld", h->pace_total);

	} else if(!strcmp(line, "sink")) {
		h->sink = (arg && !strcmp(arg, "on"));
		if(h->sink) {
			h->rx.rpos = h->rx.size = 0;
			h->sink_primed = 0;
		}
		reply("ok");

	} else if(!strcmp(line, "stats")) {
		reply("ok %llu %llu %llu %zu", h->sent, h->received, h->seq_errors,
		      h->tx.size - h->tx.rpos);

	} else if(!strcmp(line, "reset")) {
		h->sent = h->received = h->seq_errors = 0;
		h->sink_primed = 0;
		h->rx.rpos = h->rx.size = 0;
		h->tx.rpos = h->tx.size = 0;
		ev_timer_stop(h->loop, &h->w_pace);
		reply("ok");

	} else if(!strcmp(line, "recv")) {
		print_hex(h->rx.data + h->rx.rpos, h->rx.size - h->rx.rpos);
		buf_took(&h->rx, h->rx.size - h->rx.rpos);

	} else if(!strcmp(line, "pending")) {
		reply("ok %zu", h->rx.size - h->rx.rpos);

	} else if(!strcmp(line, "stopreading")) {
		if(h->reading) {
			ev_io_stop(h->loop, &h->w_router_in);
			h->reading = 0;
		}
		reply("ok");

	} else if(!strcmp(line, "resume")) {
		if(!h->reading && h->router_fd != -1) {
			ev_io_start(h->loop, &h->w_router_in);
			h->reading = 1;
		}
		reply("ok");

	} else if(!strcmp(line, "shutdown")) {
		if(h->router_fd != -1) {
			ev_io_stop(h->loop, &h->w_router_in);
			ev_io_stop(h->loop, &h->w_router_out);
			close(h->router_fd);
			h->router_fd = -1;
			h->reading = 0;
		}
		reply("ok");

	} else if(!strcmp(line, "quit")) {
		reply("ok");
		ev_break(h->loop, EVBREAK_ALL);

	} else if(!*line) {
		/* ignore empty lines */

	} else {
		reply("err unknown");
	}
}

static void cmd_cb(struct ev_loop *loop, ev_io *w, int revents) {
	(void)loop; (void)revents;
	struct harness *h = w->data;

	ssize_t r = read(w->fd, h->cmd + h->cmd_len, sizeof(h->cmd) - h->cmd_len - 1);
	if(r == 0) {
		ev_break(h->loop, EVBREAK_ALL);
		return;
	}
	if(r < 0) {
		if(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
			return;
		ev_break(h->loop, EVBREAK_ALL);
		return;
	}

	h->cmd_len += (size_t)r;
	h->cmd[h->cmd_len] = 0;

	for(;;) {
		char *nl = memchr(h->cmd, '\n', h->cmd_len);
		if(!nl)
			break;
		*nl = 0;
		handle_cmd(h, (char *)h->cmd);
		size_t consumed = (size_t)(nl - (char *)h->cmd) + 1;
		memmove(h->cmd, h->cmd + consumed, h->cmd_len - consumed);
		h->cmd_len -= consumed;
		h->cmd[h->cmd_len] = 0;
	}
}

int main(int argc, char **argv) {
	struct harness h;
	struct connector_spec cspec;
	int sv[2];
	int bufsize = SOCK_BUF_SIZE;

	if(argc < 2) {
		fprintf(stderr, "usage: %s <devname> [maxcon]\n", argv[0]);
		return 2;
	}

	memset(&h, 0, sizeof(h));

	/* The logger writes to stdout when opened with use_stdout, and log_open()
	 * announces the log file on stdout even when it does not. Either would land in
	 * the middle of the protocol, so send the log to a file and keep stdout covered
	 * while log_open() runs. */
	log_set_file_name(getenv("VSP_HARNESS_LOG") ? getenv("VSP_HARNESS_LOG")
						   : "/tmp/vsp_harness.log");
	{
		int saved = dup(STDOUT_FILENO);
		int devnull = open("/dev/null", O_WRONLY);
		int rc;
		if(devnull != -1)
			dup2(devnull, STDOUT_FILENO);
		rc = log_open(0);
		fflush(stdout);
		if(saved != -1) {
			dup2(saved, STDOUT_FILENO);
			close(saved);
		}
		if(devnull != -1)
			close(devnull);
		if(rc) {
			fprintf(stderr, "harness: could not open log file\n");
			return 1;
		}
	}
	log_set_level_by_str(getenv("VSP_HARNESS_LOGLEVEL") ? getenv("VSP_HARNESS_LOGLEVEL")
							   : "WARN");

	if(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0, sv)) {
		fprintf(stderr, "harness: socketpair failed: %s\n", strerror(errno));
		return 1;
	}
	setsockopt(sv[0], SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(bufsize));
	setsockopt(sv[0], SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize));
	setsockopt(sv[1], SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(bufsize));
	setsockopt(sv[1], SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize));

	h.loop = ev_default_loop(EVFLAG_AUTO);
	h.router_fd = sv[0];

	memset(&cspec, 0, sizeof(cspec));
	cspec.loop = h.loop;
	cspec.fd_data = sv[1];
	cspec.fd_ptt = sv[1];
	cspec.vsp.devname = argv[1];
	cspec.vsp.maxcon = argc > 2 ? atoi(argv[2]) : 4;

	h.vsp = vsp_create(&cspec);
	if(!h.vsp) {
		fprintf(stderr, "harness: vsp_create failed, see %s\n", log_get_file_name());
		reply("err create");
		return 1;
	}

	ev_timer_init(&h.w_pace, pace_cb, 0., 0.005);
	h.w_pace.data = &h;

	ev_io_init(&h.w_router_in, router_in_cb, h.router_fd, EV_READ);
	ev_io_init(&h.w_router_out, router_out_cb, h.router_fd, EV_WRITE);
	ev_io_init(&h.w_cmd, cmd_cb, STDIN_FILENO, EV_READ);
	h.w_router_in.data = &h;
	h.w_router_out.data = &h;
	h.w_cmd.data = &h;

	ev_io_start(h.loop, &h.w_router_in);
	ev_io_start(h.loop, &h.w_cmd);
	h.reading = 1;

	reply("ready");

	ev_run(h.loop, 0);

	vsp_destroy(h.vsp);
	if(h.router_fd != -1)
		close(h.router_fd);
	free(h.rx.data);
	free(h.tx.data);
	return 0;
}
