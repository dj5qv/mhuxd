/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <ev.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>
#include "rigctld_client.h"
#include "mhcontrol.h"
#include "mhinfo.h"
#include "util.h"
#include "logger.h"

#ifndef MOD_ID
#define MOD_ID "rigcl"
#endif

#define RIGCL_DEF_HOST "127.0.0.1"
#define RIGCL_DEF_BACKEND "rigctld"
#define RIGCL_DEF_CONNECT_TIMEOUT_MS (1500)
#define RIGCL_DEF_IO_TIMEOUT_MS (1500)
#define RIGCL_DEF_POLL_MS (500)

#define RIGCL_NUM_COMMANDS 3
#define RIGCL_CMD_STRING "+\\get_vfo_info VFOA\n+\\get_vfo_info VFOB\n+\\get_vfo_info currVFO\n"

enum rigcl_io_phase {
	RIGCL_PHASE_IDLE = 0,
	RIGCL_PHASE_CONNECTING,
	RIGCL_PHASE_WRITING,
	RIGCL_PHASE_READING,
};

struct rigctld_client {
	struct ev_loop *loop;
	struct mh_control *ctl;
	char *serial;
	char *backend;
	char *host;
	uint8_t radio;
	int port;
	int connect_timeout_ms;
	int io_timeout_ms;
	int poll_ms;
	int enabled;
	int running;
	int fd;
	enum rigcl_io_phase phase;
	char tx_buf[80];
	size_t tx_len;
	size_t tx_off;
	char rx_buf[512];
	size_t rx_len;
	struct mhc_radio_info last_info;
	ev_tstamp req_start_ts;

	ev_timer poll_timer;
	ev_timer op_timeout_timer;
	ev_io io_w;
	struct mhc_keyer_state_callback *kscb;
};

static const char *radio_str(uint8_t radio) {
	return radio == 2 ? "r2" : "r1";
}

static void rigcl_finish_request(struct rigctld_client *client) {
	ev_io_stop(client->loop, &client->io_w);
	ev_timer_stop(client->loop, &client->op_timeout_timer);
	client->phase = RIGCL_PHASE_IDLE;
	client->tx_off = 0;
	client->rx_len = 0;
}

static void rigcl_arm_op_timeout(struct rigctld_client *client, int timeout_ms) {
	if(!client)
		return;
	if(timeout_ms < 100)
		timeout_ms = 100;
	ev_timer_set(&client->op_timeout_timer, (double)timeout_ms / 1000.0, 0.0);
	ev_timer_start(client->loop, &client->op_timeout_timer);
}

static void rigcl_reset_io(struct rigctld_client *client) {
	rigcl_finish_request(client);
	if(client->fd != -1)
		fd_close(&client->fd);
}

static int mode_from_rigctld_token(const char *token) {
	if(!token || !*token)
		return -1;

	if(!strcasecmp(token, "CW") || !strcasecmp(token, "CWR"))
		return MOD_CW;

	if(!strcasecmp(token, "USB") || !strcasecmp(token, "LSB") ||
	   !strcasecmp(token, "AM") || !strcasecmp(token, "FM") ||
	   !strcasecmp(token, "WFM") || !strcasecmp(token, "AMS"))
		return MOD_VOICE;

	if(!strcasecmp(token, "RTTY") || !strcasecmp(token, "RTTYR"))
		return MOD_FSK;

	if(!strcasecmp(token, "PKTUSB") || !strcasecmp(token, "PKTLSB") ||
	   !strcasecmp(token, "PKTFM") || !strcasecmp(token, "PKTAM") ||
	   !strcasecmp(token, "FAX"))
		return MOD_DIGITAL;

	return -1;
}

static int parse_mode_token(const char *line, char *token, size_t token_sz) {
	if(!line || !token || token_sz < 2)
		return -1;

	while(*line == ' ' || *line == '\t')
		line++;

	if(*line == '\0' || *line == '\n' || *line == '\r')
		return -1;

	size_t i = 0;
	while(line[i] && line[i] != ' ' && line[i] != '\t' && line[i] != '\n' && line[i] != '\r') {
		if(i + 1 >= token_sz)
			break;
		token[i] = line[i];
		i++;
	}
	token[i] = '\0';
	return i > 0 ? 0 : -1;
}

static int count_rprt_lines(const char *buf, size_t len) {
	int count = 0;
	const char *p = buf;
	const char *end = buf + len;

	if(len >= 5 && strncmp(p, "RPRT ", 5) == 0)
		count++;

	while(p < end) {
		p = memchr(p, '\n', end - p);
		if(!p) break;
		p++;
		if(p + 5 <= end && strncmp(p, "RPRT ", 5) == 0)
			count++;
	}
	return count;
}

enum rigcl_section {
	RIGCL_SECT_NONE = 0,
	RIGCL_SECT_VFOA,
	RIGCL_SECT_VFOB,
	RIGCL_SECT_CURRVFO,
};

static enum rigcl_section rigcl_detect_section(const char *line) {
	if(strncmp(line, "get_vfo_info: ", 14) != 0)
		return RIGCL_SECT_NONE;
	const char *vfo = line + 14;
	if(strncmp(vfo, "VFOA", 4) == 0) return RIGCL_SECT_VFOA;
	if(strncmp(vfo, "VFOB", 4) == 0) return RIGCL_SECT_VFOB;
	return RIGCL_SECT_CURRVFO;
}

static void rigcl_parse_response(struct rigctld_client *client) {
	struct mhc_radio_info info = client->last_info;
	info.radio = client->radio;
	int split = -1;
	int errors = 0;
	enum rigcl_section sect = RIGCL_SECT_NONE;
	char *saveptr = NULL;
	char *line;

	for(line = strtok_r(client->rx_buf, "\n", &saveptr); line;
	    line = strtok_r(NULL, "\n", &saveptr)) {
		size_t len = strlen(line);
		if(len > 0 && line[len - 1] == '\r')
			line[--len] = '\0';

		dbg1("%s parse: <%s> (%s)", client->serial, line, radio_str(client->radio));

		enum rigcl_section new_sect = rigcl_detect_section(line);
		if(new_sect != RIGCL_SECT_NONE) {
			sect = new_sect;
			continue;
		}

		if(strncmp(line, "Freq: ", 6) == 0) {
			uint32_t freq = (uint32_t)strtoul(line + 6, NULL, 10);
			if(sect == RIGCL_SECT_VFOA)
				info.vfoAFreq = freq;
			else if(sect == RIGCL_SECT_VFOB)
				info.vfoBFreq = freq;
			else if(sect == RIGCL_SECT_CURRVFO) {
				info.rxFreq = freq;
				info.operFreq = freq;
			}
		} else if(strncmp(line, "Mode: ", 6) == 0 && sect == RIGCL_SECT_CURRVFO) {
			char token[32];
			if(parse_mode_token(line + 6, token, sizeof(token)) == 0) {
				int m = mode_from_rigctld_token(token);
				if(m >= 0)
					info.mode = m;
				else
					dbg0("%s unsupported rigctld mode '%s' (%s)",
					     client->serial, token, radio_str(client->radio));
			}
		} else if(strncmp(line, "Split: ", 7) == 0 && sect == RIGCL_SECT_CURRVFO) {
			split = atoi(line + 7);
		} else if(strncmp(line, "RPRT ", 5) == 0) {
			if(atoi(line + 5) != 0)
				errors++;
			sect = RIGCL_SECT_NONE;
		}
	}

	/* Derive txFreq from split status */
	if(split >= 0) {
		if(split == 0) {
			info.txFreq = info.rxFreq;
		} else {
			if(info.rxFreq == info.vfoAFreq)
				info.txFreq = info.vfoBFreq;
			else if(info.rxFreq == info.vfoBFreq)
				info.txFreq = info.vfoAFreq;
			else
				info.txFreq = info.vfoBFreq; /* default: assume VFOB is TX */
		}
	}

	if(errors > 0)
		dbg0("%s rigctld %d/%d commands returned errors (%s)",
		     client->serial, errors, RIGCL_NUM_COMMANDS, radio_str(client->radio));

	int elapsed_ms = (int)((ev_now(client->loop) - client->req_start_ts) * 1000.0);
	dbg0("%s poll result: mode=%d rx=%u tx=%u vfoA=%u vfoB=%u split=%d %dms (%s)",
	     client->serial, info.mode, info.rxFreq, info.txFreq,
	     info.vfoAFreq, info.vfoBFreq, split, elapsed_ms, radio_str(client->radio));

	client->last_info = info;
	mhc_update_radio_info(client->ctl, MOD_ID, &info);
}

static void rigcl_op_fail(struct rigctld_client *client, const char *where, int errsv) {
	if(errsv) {
		dbg0("%s rigctld %s failed (%s:%d) err=%d", client->serial, where, client->host, client->port, errsv);
	} else {
		dbg0("%s rigctld %s failed (%s:%d)", client->serial, where, client->host, client->port);
	}
	rigcl_reset_io(client);
}

static void rigcl_io_cb(struct ev_loop *loop, struct ev_io *w, int revents) {
	(void)loop;
	struct rigctld_client *client = w->data;
	if(!client || client->fd < 0)
		return;

    dbg1("%s for %s", __func__, client->serial);

	if(client->phase == RIGCL_PHASE_CONNECTING && (revents & EV_WRITE)) {
		int soerr = 0;
		socklen_t slen = sizeof(soerr);
		if(getsockopt(client->fd, SOL_SOCKET, SO_ERROR, &soerr, &slen) != 0 || soerr != 0) {
			rigcl_op_fail(client, "connect", soerr ? soerr : errno);
			return;
		}
		client->phase = RIGCL_PHASE_WRITING;
		rigcl_arm_op_timeout(client, client->io_timeout_ms);
	}

	if(client->phase == RIGCL_PHASE_WRITING && (revents & EV_WRITE)) {
		ssize_t size = 0;
		int errsv = 0;
		enum mhuxd_io_rw_result wr = io_write_nonblock(client->fd,
				client->tx_buf + client->tx_off,
				client->tx_len - client->tx_off,
				&size, &errsv);
		if(wr == MHUXD_IO_RW_WOULD_BLOCK)
			return;
		if(wr == MHUXD_IO_RW_ERROR || wr == MHUXD_IO_RW_EOF) {
			rigcl_op_fail(client, "write", errsv);
			return;
		}

		client->tx_off += (size_t)size;
		if(client->tx_off < client->tx_len)
			return;

		client->phase = RIGCL_PHASE_READING;
		ev_io_stop(client->loop, &client->io_w);
		ev_io_set(&client->io_w, client->fd, EV_READ);
		ev_io_start(client->loop, &client->io_w);
		rigcl_arm_op_timeout(client, client->io_timeout_ms);
		return;
	}

	if(client->phase == RIGCL_PHASE_READING && (revents & EV_READ)) {
		ssize_t size = 0;
		int errsv = 0;
		enum mhuxd_io_rw_result rr = io_read_nonblock(client->fd,
				client->rx_buf + client->rx_len,
				sizeof(client->rx_buf) - 1 - client->rx_len,
				&size, &errsv);
		if(rr == MHUXD_IO_RW_WOULD_BLOCK)
			return;
		if(rr == MHUXD_IO_RW_ERROR || rr == MHUXD_IO_RW_EOF) {
			rigcl_op_fail(client, "read", errsv);
			return;
		}

		client->rx_len += (size_t)size;
		client->rx_buf[client->rx_len] = '\0';

        dbg1("%s %s r%d read %zu/%zu bytes", __func__, client->serial, client->radio,
             client->rx_len, sizeof(client->rx_buf) - 1);

		if(count_rprt_lines(client->rx_buf, client->rx_len) < RIGCL_NUM_COMMANDS)
			return;

		rigcl_parse_response(client);
		rigcl_finish_request(client);
	}
}

static void rigcl_op_timeout_cb(struct ev_loop *loop, struct ev_timer *w, int revents) {
	(void)loop;
	(void)revents;
	struct rigctld_client *client = w->data;
	if(!client)
		return;
    dbg1("%s for %s", __func__, client->serial);
    rigcl_op_fail(client, "timeout", ETIMEDOUT);
}

static int rigcl_open_nonblock_socket(const char *host, int port, int *fd_out) {
	char portbuf[16];
	struct addrinfo hints;
	struct addrinfo *res = NULL;
	struct addrinfo *ai;
	int fd = -1;
	int rc;

	if(!host || port < 1 || port > 65535 || !fd_out) {
		errno = EINVAL;
		return -1;
	}

	snprintf(portbuf, sizeof(portbuf), "%d", port);
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	rc = getaddrinfo(host, portbuf, &hints, &res);
	if(rc != 0) {
		errno = EINVAL;
		return -1;
	}

	for(ai = res; ai; ai = ai->ai_next) {
		fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if(fd < 0)
			continue;

		int flags = fcntl(fd, F_GETFL, 0);
		if(flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
			close(fd);
			fd = -1;
			continue;
		}

		if(connect(fd, ai->ai_addr, ai->ai_addrlen) == 0 || errno == EINPROGRESS)
			break;

		close(fd);
		fd = -1;
	}

	freeaddrinfo(res);
	if(fd < 0)
		return -1;

	*fd_out = fd;
	return 0;
}

static void rigcl_start_request(struct rigctld_client *client) {
	if(!client || !client->running)
		return;

    dbg1("%s for %s", __func__, client->serial);

    if(client->phase != RIGCL_PHASE_IDLE)
		return;
	if(strcmp(client->backend, "rigctld") != 0)
		return;

	if(client->fd == -1) {
		if(rigcl_open_nonblock_socket(client->host, client->port, &client->fd) != 0) {
			dbg0("%s rigctld connect open failed (%s:%d)", client->serial, client->host, client->port);
			client->fd = -1;
			return;
		}
		client->phase = RIGCL_PHASE_CONNECTING;
	} else {
		client->phase = RIGCL_PHASE_WRITING;
	}

	client->req_start_ts = ev_now(client->loop);
	strcpy(client->tx_buf, RIGCL_CMD_STRING);
	client->tx_len = strlen(client->tx_buf);
	client->tx_off = 0;
	client->rx_len = 0;

	ev_io_set(&client->io_w, client->fd, EV_WRITE);
	ev_io_start(client->loop, &client->io_w);

	rigcl_arm_op_timeout(client,
		client->phase == RIGCL_PHASE_CONNECTING ? client->connect_timeout_ms : client->io_timeout_ms);
}

static void rigcl_stop(struct rigctld_client *client) {
    dbg1("%s for %s", __func__, client->serial);

    if(!client->running)
		return;
	ev_timer_stop(client->loop, &client->poll_timer);
	rigcl_reset_io(client);
	client->running = 0;
	dbg1("%s %s stop polling (%s %s:%d)", client->serial, __func__,
	     radio_str(client->radio), client->host, client->port);
}

static void rigcl_start(struct rigctld_client *client) {
    dbg1("%s for %s", __func__, client->serial);

    if(!client->enabled)
		return;
	if(client->running)
		return;
	ev_timer_set(&client->poll_timer, 0., (double)client->poll_ms / 1000.0);
	ev_timer_start(client->loop, &client->poll_timer);
	client->running = 1;
	dbg1("%s %s start polling (%s backend=%s %s:%d poll_ms=%d connect_timeout_ms=%d)",
	     client->serial, __func__, radio_str(client->radio), client->backend,
	     client->host, client->port, client->poll_ms, client->connect_timeout_ms);
}

static void rigcl_poll_cb(struct ev_loop *loop, struct ev_timer *w, int revents) {
    struct rigctld_client *client = w->data;
    if(!client)
        return;
    dbg1("%s for %s", __func__, client->serial);

	if(!client || !client->running)
		return;
	rigcl_start_request(client);
}

static void rigcl_keyer_state_changed_cb(const char *serial, int state, void *user_data) {
    struct rigctld_client *client = user_data;
	if(!client)
		return;
    dbg1("%s for %s", __func__, client->serial);

    if(serial && strcmp(serial, client->serial))
		return;

	if(state == MHC_KEYER_STATE_ONLINE) {
		rigcl_start(client);
		return;
	}

	rigcl_stop(client);
}

struct rigctld_client *rigctld_client_create(struct ev_loop *loop, struct mh_control *ctl,
					     const struct rigctld_client_cfg *cfg) {

    dbg1("%s for %s", __func__, cfg->serial);

    if(!loop || !ctl || !cfg || !cfg->serial || !cfg->serial[0])
		return NULL;

	struct rigctld_client *client = w_calloc(1, sizeof(*client));
	client->loop = loop;
	client->ctl = ctl;
	client->serial = w_strdup(cfg->serial);
	client->radio = cfg->radio == 2 ? 2 : 1;
	client->backend = w_strdup(cfg->backend && cfg->backend[0] ? cfg->backend : RIGCL_DEF_BACKEND);
	client->host = w_strdup(cfg->host && cfg->host[0] ? cfg->host : RIGCL_DEF_HOST);
	client->port = cfg->port > 0 ? cfg->port : (client->radio == 2 ? 4533 : 4532);
	client->connect_timeout_ms = cfg->connect_timeout_ms > 0 ? cfg->connect_timeout_ms : RIGCL_DEF_CONNECT_TIMEOUT_MS;
	client->io_timeout_ms = cfg->io_timeout_ms > 0 ? cfg->io_timeout_ms : RIGCL_DEF_IO_TIMEOUT_MS;
	client->poll_ms = cfg->poll_ms >= 10 ? cfg->poll_ms : RIGCL_DEF_POLL_MS;
	client->enabled = cfg->enabled ? 1 : 0;
	client->fd = -1;
	client->last_info.radio = client->radio;
	client->last_info.mode = -1;

	ev_timer_init(&client->poll_timer, rigcl_poll_cb, 0., (double)client->poll_ms / 1000.0);
	client->poll_timer.data = client;
	ev_timer_init(&client->op_timeout_timer, rigcl_op_timeout_cb, 0., 0.);
	client->op_timeout_timer.data = client;
	ev_io_init(&client->io_w, rigcl_io_cb, -1, EV_WRITE);
	client->io_w.data = client;

	client->kscb = mhc_add_keyer_state_changed_cb(ctl, rigcl_keyer_state_changed_cb, client);
	if(client->enabled && mhc_is_online(ctl))
		rigcl_start(client);

	dbg0("%s rigctld client created (%s backend=%s host=%s port=%d connect_timeout_ms=%d io_timeout_ms=%d)",
	     client->serial, radio_str(client->radio), client->backend, client->host, client->port,
	     client->connect_timeout_ms, client->io_timeout_ms);

	return client;
}

void rigctld_client_destroy(struct rigctld_client *client) {
	if(!client)
		return;

	rigcl_stop(client);
	if(client->kscb)
		mhc_rem_keyer_state_changed_cb(client->ctl, client->kscb);

	dbg0("%s rigctld client destroyed (%s)",
	     client->serial ? client->serial : "<unknown>", radio_str(client->radio));

	if(client->serial)
		free(client->serial);
	if(client->backend)
		free(client->backend);
	if(client->host)
		free(client->host);
	free(client);
}
