/*
 *  mhux - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#ifndef DISPATCHER_H
#define DISPATCHER_H

#include <sys/time.h>
#include "pglist.h"
#include "buffer.h"

struct dispatcher;
struct timer;

#define CRSF_PENDING (1<<7)

enum {
	CRS_NEW      = (1 | CRSF_PENDING),
	CRS_SUCCESS  =  2,
	CRS_WRITE    = (3 | CRSF_PENDING),
	CRS_READ     = (4 | CRSF_PENDING),
	CRS_TIMEOUT  =  5,
	CRS_ERROR    =  6,
};

struct cmd_request {
	struct PGNode	node;
	void	(*req_done)(struct cmd_request *);
	void	*data;
	struct buffer   b_cmd;
	struct buffer   b_resp;
	int state;		/* CRS_... */
	struct timer *timer;
};

extern int sig_term;
struct	dispatcher *dp_create();
void	dp_destroy(struct dispatcher *dp);
void    dp_terminate(struct dispatcher *dp);
int     dp_loop(struct dispatcher *dp, int return_on_no_req);
void    dp_terminate(struct dispatcher *dp);
int     dp_submit_request(struct dispatcher *dp, struct cmd_request *req);
void	dp_set_keyer_fd(struct dispatcher *dp, int fd);
void    dp_set_fd(struct dispatcher *dp, int channel, int fd);
void    dp_set_ctl_fd(struct dispatcher *dp, int channel, int fd);
void	dp_set_ptt_fd(struct dispatcher *dp, int radio, int fd);
void	dp_set_dev_timeout(struct dispatcher *dp, int timeout);
void	dp_set_hup_handler(void (*handler)(void *), void *userdata);

struct timer *dp_create_timer(struct dispatcher *dp, int ival, int single_shot,
			void (*timeout)(struct timer *, void *userdata),
			void *userdata);
void dp_destroy_timer(struct timer *t);
void dp_submit_timer(struct dispatcher *dp, struct timer *t);
void dp_cancel_timer(struct dispatcher *dp, struct timer *t);
void dp_cancel_timer_by_userdata(struct dispatcher *dp, void *userdata);

#endif // DISPATCHER_H
