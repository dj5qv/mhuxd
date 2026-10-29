/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2026  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#ifndef CON_CLIENT_H
#define CON_CLIENT_H

#include <stddef.h>
#include <time.h>
#include <jansson.h>

// The application on a port, as far as mhuxd can tell. Taken once when it connects.
// A VSP knows the process that opened it. A TCP port knows the peer address, and for a
// client on this host the user it runs as: finding its process would take permission to
// read the client's /proc/<pid>/fd, which mhuxd running as its own user doesn't have.
struct con_client {
	time_t since;			// when it connected
	int pid;			// VSP: process that opened the port, 0: unknown
	int uid;			// -1: unknown
	char user[33];			// name of uid, "": unknown
	char program[64];		// VSP: e.g. "wsjtx", "python3 foo.py", "": unknown
	char peer[64];			// TCP: "127.0.0.1:53422", "[::1]:53422"
};

// VSP: the thread id and uid a CUSE request carries. The kernel puts the id of the
// thread into the request, which is not the process id when a worker thread opens.
void con_client_from_process(struct con_client *cl, int tid, int uid);
// TCP: an accepted connection.
void con_client_from_socket(struct con_client *cl, int fd);
// For log messages, e.g. "wsjtx (pid 4711, user mattes)".
const char *con_client_str(const struct con_client *cl, char *buf, size_t size);
json_t *con_client_json(const struct con_client *cl);

#endif // CON_CLIENT_H
