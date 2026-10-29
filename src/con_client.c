/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2026  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "con_client.h"

// These run the program they got as first argument that is no option.
static const char *const interpreters[] = {
	"python", "perl", "ruby", "node", "nodejs", "java", "mono", "dotnet",
	"tclsh", "wish", "lua", "sh", "bash", "dash", NULL
};

static void client_init(struct con_client *cl) {
	memset(cl, 0, sizeof(*cl));
	cl->since = time(NULL);
	cl->uid = -1;
}

// Read a /proc file into buf, NUL terminated. Returns the length, -1 if it can't be read.
static ssize_t read_proc(const char *path, char *buf, size_t size) {
	ssize_t len = 0, n;
	int fd = open(path, O_RDONLY | O_CLOEXEC);
	if(fd == -1)
		return -1;
	while((size_t)len < size - 1) {
		n = read(fd, buf + len, size - 1 - len);
		if(n < 0 && errno == EINTR)
			continue;
		if(n <= 0)
			break;
		len += n;
	}
	close(fd);
	buf[len] = 0;
	return len;
}

// Backslash too: under Wine argv[0] is the Windows path of the program.
static const char *base_name(const char *path) {
	const char *base = path;
	for(const char *p = path; *p; p++)
		if(*p == '/' || *p == '\\')
			base = p + 1;
	return base;
}

static int is_interpreter(const char *name) {
	size_t len = strlen(name);
	// python3.11, lua5.4
	while(len && (isdigit((unsigned char)name[len - 1]) || name[len - 1] == '.'))
		len--;
	for(const char *const *i = interpreters; *i; i++)
		if(strlen(*i) == len && !strncmp(name, *i, len))
			return 1;
	return 0;
}

// Only the program goes out, not the arguments: the Web UI may be reachable from the
// network, and an argument could be a password.
static void read_program(int pid, char *out, size_t size) {
	char path[32];
	char buf[4096];
	ssize_t len;

	*out = 0;
	snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
	len = read_proc(path, buf, sizeof(buf));
	if(len > 0) {
		// NUL separated arguments
		const char *arg0 = base_name(buf);
		const char *script = NULL;
		if(is_interpreter(arg0)) {
			for(const char *p = buf + strlen(buf) + 1; p < buf + len; p += strlen(p) + 1) {
				if(*p && *p != '-') {
					script = p;
					break;
				}
			}
		}
		if(script)
			snprintf(out, size, "%s %s", arg0, base_name(script));
		else
			snprintf(out, size, "%s", arg0);
	}

	// No command line, e.g. the process just exited.
	if(!*out) {
		snprintf(path, sizeof(path), "/proc/%d/comm", pid);
		if(read_proc(path, buf, sizeof(buf)) > 0) {
			buf[strcspn(buf, "\n")] = 0;
			snprintf(out, size, "%s", buf);
		}
	}

	for(char *p = out; *p; p++)
		if((unsigned char)*p < 0x20 || (unsigned char)*p > 0x7e)
			*p = '?';
}

// The process a thread belongs to.
static int tgid_of(int tid) {
	char path[32];
	char buf[1024];
	snprintf(path, sizeof(path), "/proc/%d/status", tid);
	if(read_proc(path, buf, sizeof(buf)) > 0) {
		const char *p = strstr(buf, "\nTgid:");
		int tgid = p ? atoi(p + 6) : 0;
		if(tgid > 0)
			return tgid;
	}
	return tid;
}

static void set_user(struct con_client *cl, int uid) {
	struct passwd pw, *res = NULL;
	char buf[1024];

	cl->uid = uid;
	if(uid >= 0 && !getpwuid_r((uid_t)uid, &pw, buf, sizeof(buf), &res) && res)
		snprintf(cl->user, sizeof(cl->user), "%s", pw.pw_name);
}

void con_client_from_process(struct con_client *cl, int tid, int uid) {
	client_init(cl);
	set_user(cl, uid);
	if(tid <= 0)
		return;
	cl->pid = tgid_of(tid);
	read_program(cl->pid, cl->program, sizeof(cl->program));
}

static int hex_words(const char *hex, uint32_t *w, int n) {
	char word[9];
	if(strlen(hex) != (size_t)n * 8)
		return -1;
	for(int i = 0; i < n; i++) {
		memcpy(word, hex + i * 8, 8);
		word[8] = 0;
		w[i] = (uint32_t)strtoul(word, NULL, 16);
	}
	return 0;
}

// A client on this host has its end of the connection in /proc/net/tcp(6) as well, with
// our peer as local and us as remote address, owned by the user that runs the client. The
// kernel prints each 32 bit word of an address as it is in memory, the ports in host order.
static int local_peer_uid(const struct sockaddr_storage *peer, const struct sockaddr_storage *self) {
	uint32_t pa[4], sa[4];
	unsigned int pport, sport;
	int words;
	const char *table;
	char line[512];
	int uid = -1;
	FILE *f;

	if(peer->ss_family == AF_INET) {
		const struct sockaddr_in *p = (const void *)peer, *s = (const void *)self;
		memcpy(pa, &p->sin_addr, 4);
		memcpy(sa, &s->sin_addr, 4);
		pport = ntohs(p->sin_port);
		sport = ntohs(s->sin_port);
		words = 1;
		table = "/proc/net/tcp";
	} else if(peer->ss_family == AF_INET6) {
		const struct sockaddr_in6 *p = (const void *)peer, *s = (const void *)self;
		memcpy(pa, &p->sin6_addr, 16);
		memcpy(sa, &s->sin6_addr, 16);
		pport = ntohs(p->sin6_port);
		sport = ntohs(s->sin6_port);
		words = 4;
		table = "/proc/net/tcp6";
	} else {
		return -1;
	}

	f = fopen(table, "re");
	if(!f)
		return -1;
	while(fgets(line, sizeof(line), f)) {
		char la[33], ra[33];
		unsigned int lport, rport, luid;
		uint32_t lw[4], rw[4];

		// sl local_address rem_address st tx_queue:rx_queue tr:tm->when retrnsmt uid ...
		if(sscanf(line, " %*u: %32[0-9A-Fa-f]:%x %32[0-9A-Fa-f]:%x %*x %*x:%*x %*x:%*x %*x %u",
			  la, &lport, ra, &rport, &luid) != 5)
			continue;
		if(lport != pport || rport != sport)
			continue;
		if(hex_words(la, lw, words) || hex_words(ra, rw, words))
			continue;
		if(memcmp(lw, pa, words * 4) || memcmp(rw, sa, words * 4))
			continue;
		uid = (int)luid;
		break;
	}
	fclose(f);
	return uid;
}

void con_client_from_socket(struct con_client *cl, int fd) {
	struct sockaddr_storage peer, self;
	socklen_t plen = sizeof(peer), slen = sizeof(self);
	char addr[INET6_ADDRSTRLEN];

	client_init(cl);
	if(getpeername(fd, (struct sockaddr *)&peer, &plen) == -1)
		return;

	if(peer.ss_family == AF_INET) {
		const struct sockaddr_in *sin = (const void *)&peer;
		if(inet_ntop(AF_INET, &sin->sin_addr, addr, sizeof(addr)))
			snprintf(cl->peer, sizeof(cl->peer), "%s:%u", addr, ntohs(sin->sin_port));
	} else if(peer.ss_family == AF_INET6) {
		const struct sockaddr_in6 *sin6 = (const void *)&peer;
		if(inet_ntop(AF_INET6, &sin6->sin6_addr, addr, sizeof(addr)))
			snprintf(cl->peer, sizeof(cl->peer), "[%s]:%u", addr, ntohs(sin6->sin6_port));
	}

	if(getsockname(fd, (struct sockaddr *)&self, &slen) == 0 && self.ss_family == peer.ss_family)
		set_user(cl, local_peer_uid(&peer, &self));
}

const char *con_client_str(const struct con_client *cl, char *buf, size_t size) {
	const char *name = *cl->program ? cl->program : *cl->peer ? cl->peer : "unknown client";
	char pid[24] = "";
	char user[48] = "";

	if(cl->pid > 0)
		snprintf(pid, sizeof(pid), "pid %d", cl->pid);
	if(*cl->user)
		snprintf(user, sizeof(user), "user %s", cl->user);
	else if(cl->uid >= 0)
		snprintf(user, sizeof(user), "uid %d", cl->uid);

	if(*pid && *user)
		snprintf(buf, size, "%s (%s, %s)", name, pid, user);
	else if(*pid || *user)
		snprintf(buf, size, "%s (%s)", name, *pid ? pid : user);
	else
		snprintf(buf, size, "%s", name);
	return buf;
}

json_t *con_client_json(const struct con_client *cl) {
	json_t *obj = json_object();
	json_object_set_new(obj, "since", json_integer((json_int_t)cl->since));
	if(cl->pid > 0)
		json_object_set_new(obj, "pid", json_integer(cl->pid));
	if(cl->uid >= 0)
		json_object_set_new(obj, "uid", json_integer(cl->uid));
	if(*cl->user)
		json_object_set_new(obj, "user", json_string(cl->user));
	if(*cl->program)
		json_object_set_new(obj, "program", json_string(cl->program));
	if(*cl->peer)
		json_object_set_new(obj, "peer", json_string(cl->peer));
	return obj;
}
