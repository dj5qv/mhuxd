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
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
	struct sockaddr_un sun;
	struct sockaddr *saddr;
	socklen_t saddrlen;
	int domain;

	errno = EINVAL;

	if(!constr || !*constr)
		return NULL;

	if(*constr == '/') {
		// UNIX domain socket
		if(strlen(constr) > (sizeof(sun.sun_path) - 1))
			return NULL;

		sun.sun_family = AF_LOCAL;
		strcpy(sun.sun_path, constr);
		saddr = (struct sockaddr *)&sun;
		saddrlen = sizeof(sun);
		domain = AF_LOCAL;
		unlink(sun.sun_path);
	} else {
		// TCP socket
		struct hostent *hostent;
		char *host_port = w_strdup(constr);
		char *port_str;
		char *host_str;
		int port;

		port_str = strchr(host_port, ':');

		if(port_str == NULL || strlen(port_str) <= 1) {
			free(host_port);
			return NULL;
		}

		domain = AF_INET;

		*port_str++ = 0;
		host_str = host_port;

		port = atoi(port_str);

		hostent = gethostbyname(host_str);

		if(hostent == NULL)
			goto error;

		if(hostent->h_addrtype == AF_INET) {
			sin.sin_family = hostent->h_addrtype;
			sin.sin_port = htons(port);
			memcpy(&sin.sin_addr, hostent->h_addr_list[0], sizeof(sin.sin_addr));
			saddr = (struct sockaddr *)&sin;
			saddrlen = sizeof(sin);
		} else if(hostent->h_addrtype == AF_INET6) {
			sin6.sin6_family = hostent->h_addrtype;
			sin6.sin6_port = htons(port);
			memcpy(&sin6.sin6_addr, hostent->h_addr_list[0], sizeof(sin6.sin6_addr));
			saddr = (struct sockaddr *)&sin6;
			saddrlen = sizeof(sin6);
		} else
			goto error;

		free(host_port);
	}


	fd = socket(domain, SOCK_STREAM, 0);

	int opt = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void*)&opt, sizeof(opt));

	if(fd == -1)
		goto error;

	int res = bind(fd, saddr, saddrlen);

	if(res == -1)
		goto error;

	if(-1 == fcntl(fd, F_SETFL, O_NONBLOCK))
		goto error;

	if(-1 == listen(fd, 8))
		goto error;

	if(domain == AF_LOCAL)
		chmod(constr, 0777);

	struct netlsnr *lsnr = w_calloc(1, sizeof(*lsnr));
	lsnr->domain = domain;
	lsnr->fd = fd;
	if(domain == AF_LOCAL)
		lsnr->spath = w_strdup(sun.sun_path);
	return lsnr;

error:
	{
		int tmp_errno = errno;
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

