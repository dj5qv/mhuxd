/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2013  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include "util.h"

struct netlsnr {
	int fd;
	int domain;
	char *spath;
};

struct netlsnr *net_create_listener(const char *constr) {
	int fd = -1;
	struct sockaddr_un sun;
	int domain = AF_UNSPEC;
	char *host_port = NULL;
	char *host_str = NULL;
	char *port_str = NULL;
	struct addrinfo hints;
	struct addrinfo *res = NULL;
	struct addrinfo *ai = NULL;

	errno = EINVAL;

	if(!constr || !*constr)
		return NULL;

	if(*constr == '/') {
		// UNIX domain socket
		if(strlen(constr) > (sizeof(sun.sun_path) - 1))
			return NULL;

		sun.sun_family = AF_LOCAL;
		strcpy(sun.sun_path, constr);
		domain = AF_LOCAL;
		unlink(sun.sun_path);
	} else {
		// TCP socket
		host_port = w_strdup(constr);

		if(host_port[0] == '[') {
			char *end = strchr(host_port, ']');
			if(end == NULL || end[1] != ':' || end[2] == '\0')
				goto error;
			*end = '\0';
			host_str = host_port + 1;
			port_str = end + 2;
		} else {
			port_str = strrchr(host_port, ':');
			if(port_str == NULL || port_str == host_port || port_str[1] == '\0')
				goto error;
			*port_str++ = '\0';
			host_str = host_port;
		}

		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_ADDRCONFIG;

		if(getaddrinfo(host_str, port_str, &hints, &res) != 0)
			goto error;

		for(ai = res; ai != NULL; ai = ai->ai_next) {
			fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
			if(fd == -1)
				continue;

			int opt = 1;
			setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void*)&opt, sizeof(opt));

			if(bind(fd, ai->ai_addr, ai->ai_addrlen) == 0) {
				domain = ai->ai_family;
				break;
			}

			close(fd);
			fd = -1;
		}

		if(fd == -1)
			goto error;
	}

	if(domain == AF_LOCAL) {
		fd = socket(domain, SOCK_STREAM, 0);

		int opt = 1;
		setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void*)&opt, sizeof(opt));

		if(fd == -1)
			goto error;

		if(bind(fd, (struct sockaddr *)&sun, sizeof(sun)) == -1)
			goto error;
	}

	if(-1 == fcntl(fd, F_SETFL, O_NONBLOCK))
		goto error;

	if(-1 == listen(fd, 8))
		goto error;

	if(domain == AF_LOCAL)
		chmod(constr, 0777);

	if(res) {
		freeaddrinfo(res);
		res = NULL;
	}
	if(host_port) {
		free(host_port);
		host_port = NULL;
	}

	struct netlsnr *lsnr = w_calloc(1, sizeof(*lsnr));
	lsnr->domain = domain;
	lsnr->fd = fd;
	if(domain == AF_LOCAL)
		lsnr->spath = w_strdup(sun.sun_path);
	return lsnr;

error:
	{
		int tmp_errno = errno;
		if(res)
			freeaddrinfo(res);
		if(host_port)
			free(host_port);
		if(fd != -1)
			close(fd);
		errno = tmp_errno;
		return NULL;
	}
}

void net_destroy_lsnr(struct netlsnr *lsnr) {
	if(lsnr == NULL)
		return;
	if(lsnr->spath) {
		unlink(lsnr->spath);
		free(lsnr->spath);
	}
	close(lsnr->fd);
	free(lsnr);
}

int net_listener_get_fd(struct netlsnr *lsnr) {
	if(lsnr == NULL)
		return -1;
	return lsnr->fd;
}

int net_accept(int fd) {
	int con_fd = accept(fd, NULL, NULL);
	if(con_fd == -1)
		return -1;
	if(-1 == fcntl(con_fd, F_SETFL, O_NONBLOCK)) {
		close(con_fd);
		return -1;
	}

	return con_fd;
}

