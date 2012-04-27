/*
 *  mhux - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <poll.h>
#include <time.h>
#include <pthread.h>
#include <fuse_opt.h>
#include <cuse_lowlevel.h>
#include <fuse.h>

#include "linux_termios.h"
#include "vsp.h"
#include "util.h"
#include "pglist.h"
#include "logger.h"
#include "opts.h"
#include "buffer.h"
#include "ctlmsg.h"

#define CTL_MSG_SIZE 20

static struct PGList vsp_list;
static int vsp_list_init = 0;

struct vsp {
	struct PGNode node;
	int flags;
	char *devname;
	struct fuse_session *se;
	struct fuse_pollhandle *ph;
	pthread_mutex_t tm_mutex;
	pthread_cond_t tc_socktx;
	pthread_cond_t tc_read;
	pthread_cond_t tc_write;

	pthread_t th_worker;
	pthread_t th_sockrx;
	pthread_t th_socktx;
	int fd_data;
	int fd_ctl;
	int open_cnt;
	int terminate;
	int client_pid;

	struct termios termios;
	int	mbits;

	struct buffer drbuf, dwbuf;
	struct buffer crbuf;

};

static int set_bits(struct vsp *vsp, int bits);


/* Blocking timed write on a nonblocking fd. Timeout in ms.
  */
/*
static int write_full(int fd, struct buffer *b, int timeout) {
	struct pollfd fds[1];
	int r;

	if(fd == -1)
		return -1;

	fds[0].fd = fd;
	fds[0].events = POLLOUT;

	while(b->rpos < b->size) {
		r = poll(fds, 1, timeout);
		if(r < 0)
			return -1;
		if(r == 0) {
			errno = ETIME;
			return -1;
		}

		r = write(fd, b->data + b->rpos, b->size - b->rpos);
		if(r < 0 && errno != EAGAIN && errno != EINTR)
			return -1;
		if(r > 0) {
			buf_inc_rpos(b, b->size - b->rpos);
			if(b->rpos == b->size) {
				buf_reset(b);
				return 0;
			}
		}
	}
	return 0;
}
*/
/* Tx thread for ctl and data sockets.
  */
static void *t_socktx(void *arg) {
	struct vsp *	vsp = arg;
	struct buffer *b;
	struct pollfd fds[1];
	int r;

	dbg0("VSP %s socktx thread start", vsp->devname);

	b = &vsp->dwbuf;

	pthread_mutex_lock(&vsp->tm_mutex);

	while(!vsp->terminate && (vsp->fd_data != -1 || vsp->fd_ctl != -1)) {

		fds[0].fd = vsp->fd_data;
		fds[0].events = 0;

		if(vsp->fd_data != -1 && b->rpos < b->size)
			fds[0].events |= POLLOUT;

		if(!fds[0].events) {
			pthread_cond_wait(&vsp->tc_socktx, &vsp->tm_mutex);
			continue;
		}
		pthread_mutex_unlock(&vsp->tm_mutex);
		if(poll(fds, 1, -1) < 0) {
			pthread_mutex_lock(&vsp->tm_mutex);
			if(errno == EINTR)
				continue;
			fatal("VSP %s poll() failed: %s", vsp->devname, strerror(errno));
		}
		pthread_mutex_lock(&vsp->tm_mutex);


		if((fds[0].revents & POLLOUT) && vsp->fd_data != -1 && b->rpos < b->size) {
			r = write(vsp->fd_data, b->data + b->rpos, b->size - b->rpos);
			if(r > 0) {
				buf_inc_rpos(b, r);
				if(b->rpos == b->size)
					buf_reset(b);
				pthread_cond_signal(&vsp->tc_write);
			}

			if(r < 0) {
				err_e(-errno, "VSP %s Error writing to data socket!", vsp->devname);
				pthread_mutex_unlock(&vsp->tm_mutex);
				goto err;
			}
		}
	}

	pthread_mutex_unlock(&vsp->tm_mutex);
	dbg0("VSP %s socktx thread end", vsp->devname);
	return NULL;

err:
	close(vsp->fd_data);
	close(vsp->fd_ctl);
	vsp->fd_data = -1;
	vsp->fd_ctl = -1;
	fuse_session_exit(vsp->se);
	err("VSP %s socktx thread end", vsp->devname);
	return (void*)-1;
}

/* Rx thread for ctl and data sockets.
  */
static void *t_sockrx(void *arg) {
	struct vsp *vsp = arg;
	struct pollfd fds[2];
	int nfds = 2;
	int r;

	dbg0("VSP %s sockrx thread start", vsp->devname);

	while(!vsp->terminate && (vsp->fd_data != -1 || vsp->fd_ctl != -1)) {
		fds[0].fd = vsp->fd_data;
		fds[1].fd = vsp->fd_ctl;
		fds[0].events = 0;
		fds[1].events = 0;

		if(vsp->fd_data != -1 && buf_size_avail(&vsp->drbuf))
			fds[0].events |= POLLIN;

		if(vsp->fd_ctl != -1 && buf_size_avail(&vsp->crbuf))
			fds[1].events |= POLLIN;


		if(poll(fds, nfds, -1) < 0) {
			if(errno == EINTR)
				continue;
			fatal("VSP %s poll() failed: %s", vsp->devname, strerror(errno));
		}

		pthread_mutex_lock(&vsp->tm_mutex);
		if(fds[0].revents & POLLIN) {
			r = read(vsp->fd_data, vsp->drbuf.data + vsp->drbuf.size, buf_size_avail(&vsp->drbuf));

			if(r > 0) {
				dbg1_h("VSP-R ", vsp->drbuf.data + vsp->drbuf.size, r);
				pthread_cond_signal(&vsp->tc_read);
			}

			if(r > 0) {
				if(vsp->open_cnt)
					buf_inc_size(&vsp->drbuf, r);
			}
			if(r < 0 && errno != EAGAIN && errno != EINTR) {
				err_e(-errno, "VSP %s error reading from data socket!", vsp->devname);
				goto err;
			}

		}

		if(fds[1].revents & POLLIN && buf_size_avail(&vsp->crbuf) >= CTL_MSG_SIZE) {
			r = read(vsp->fd_ctl, vsp->crbuf.data + vsp->crbuf.size, CTL_MSG_SIZE);
			if(r > 0) {
				if(vsp->open_cnt)
					buf_inc_size(&vsp->crbuf, r);
			}

			if(r < 0 && errno != EAGAIN && errno != EINTR) {
				err_e(-errno, "VSP %s error reading from ctl socket!", vsp->devname);
				goto err;
			}
		}

		if(vsp->ph) {
			fuse_lowlevel_notify_poll(vsp->ph);
			fuse_pollhandle_destroy(vsp->ph);
			vsp->ph = NULL;
		}
		pthread_mutex_unlock(&vsp->tm_mutex);
	}

	dbg0("VSP %s sockrx thread end", vsp->devname);
	return NULL;

err:
	close(vsp->fd_data);
	close(vsp->fd_ctl);
	vsp->fd_data = -1;
	vsp->fd_ctl = -1;
	pthread_mutex_unlock(&vsp->tm_mutex);
	fuse_session_exit(vsp->se);

	err("VSP %s sockrx thread end", vsp->devname);
	return (void*)-1;
}

/* CUSE worker thread.
  */
static void *t_worker(void *arg) {
	struct vsp *vsp = arg;
	int r;

	dbg0("VSP %s worker thread start", vsp->devname);
	r = fuse_session_loop_mt(vsp->se);
	dbg0("VSP %s worker thread end", vsp->devname);
	return (void *)r;
}

static void dv_open(fuse_req_t req, struct fuse_file_info *fi)
{
	struct vsp *vsp = fuse_req_userdata(req);
	int err = 0;
	info("VSP %s req open", vsp->devname);

	pthread_mutex_lock(&vsp->tm_mutex);

	if(vsp->open_cnt) {
		warn("VSP %s Open by PID %d failed, already opened by PID %d!", vsp->devname, fuse_req_ctx(req)->pid, vsp->client_pid);
		err = EBUSY;
	} else {
		vsp->open_cnt++;
		vsp->client_pid = fuse_req_ctx(req)->pid;
	}

	pthread_mutex_unlock(&vsp->tm_mutex);

        if(!err) 
                fuse_reply_open(req, fi);
        else 
                fuse_reply_err(req, err);
}

static void dv_release(fuse_req_t req, struct fuse_file_info *fi)
{
	struct vsp *vsp = fuse_req_userdata(req);
	int err = 0;
	info("VSP %s req release", vsp->devname);

	set_bits(vsp, 0);

	pthread_mutex_lock(&vsp->tm_mutex);
	if(!vsp->open_cnt)
		err = EBADF;
	else
		vsp->open_cnt--;

	if(!vsp->open_cnt) {
		/* Clear out buffers so that the next one connecting doesn't
		 * get a bunch of garbage.
		 */
		buf_reset(&vsp->crbuf);
		buf_reset(&vsp->drbuf);
	}

	pthread_mutex_unlock(&vsp->tm_mutex);

	if(err)
		warn("VSP %s req release on not opened VSP!", vsp->devname);

	fuse_reply_err(req, err);
}

static void interrupt_func(fuse_req_t req, void *data) {
	pthread_cond_t *tc = data;
	pthread_cond_signal(tc);
}

static void dv_read(fuse_req_t req, size_t size, off_t off,
                         struct fuse_file_info *fi)
{
	struct vsp *vsp = fuse_req_userdata(req);
	struct buffer *b = &vsp->drbuf;
	size_t size_req = size;

	dbg1("VSP out %s --> App, request size: %d", vsp->devname, size);

	fuse_req_interrupt_func(req, interrupt_func, &vsp->tc_read);

	pthread_mutex_lock(&vsp->tm_mutex);

	while(!(fi->flags & O_NONBLOCK) && size > (b->size - b->rpos)) {
		pthread_cond_wait(&vsp->tc_read, &vsp->tm_mutex);
		if(fuse_req_interrupted(req) || vsp->terminate) {
			fuse_reply_err(req, EINTR);
			pthread_mutex_unlock(&vsp->tm_mutex);
			info("VSP I/O interrupt %s\n", vsp->devname);
			return;
		}
	}

	if(size > (b->size - b->rpos))
		size = (b->size - b->rpos);

	fuse_reply_buf(req, (char *)(b->data + b->rpos), size);

	buf_inc_rpos(b, size);

	if(b->rpos == b->size)
		buf_reset(b);
	pthread_mutex_unlock(&vsp->tm_mutex);

	dbg1("VSP %s --> App, size: %d/%d non-block: %d", vsp->devname, size, size_req, fi->flags & O_NONBLOCK ? 1 : 0);
}

static void dv_write(fuse_req_t req, const char *buf, size_t size,
                          off_t off, struct fuse_file_info *fi)
{
	struct vsp *vsp = fuse_req_userdata(req);
	struct buffer *b = &vsp->dwbuf;
	size_t size_req = size;

	dbg1("VSP out %s <-- App, request size: %d", vsp->devname, size);

	if(size > BUFFER_CAPACITY) {
		fuse_reply_err(req, EINVAL);
		return;
	}

	fuse_req_interrupt_func(req, interrupt_func, &vsp->tc_write);

	pthread_mutex_lock(&vsp->tm_mutex);

	while(!(fi->flags & O_NONBLOCK) && size > buf_size_avail(b)) {
		pthread_cond_wait(&vsp->tc_write, &vsp->tm_mutex);
		if(fuse_req_interrupted(req) || vsp->terminate) {
			fuse_reply_err(req, EINTR);
			pthread_mutex_unlock(&vsp->tm_mutex);
			info("VSP I/O interrupt %s\n", vsp->devname);
			return;
		}
	}

	if(size > buf_size_avail(b))
		size = buf_size_avail(b);

	buf_append(b, (unsigned char*)buf, size);
	fuse_reply_write(req, size);
	pthread_mutex_unlock(&vsp->tm_mutex);
	if(size)
		pthread_cond_signal(&vsp->tc_socktx);

	dbg1("VSP %s <-- App, size: %d/%d non-block: %d", vsp->devname, size, size_req, fi->flags & O_NONBLOCK ? 1 : 0);
}

static void dv_poll(fuse_req_t req, struct fuse_file_info *fi,
		    struct fuse_pollhandle *ph) {
	struct vsp *vsp = fuse_req_userdata(req);
	unsigned events = 0;

	dbg1("VSP %s Poll, ph: %0lx", vsp->devname, (unsigned long)ph);

	pthread_mutex_lock(&vsp->tm_mutex);

	if(ph != vsp->ph) {
		if(vsp->ph)
			fuse_pollhandle_destroy(vsp->ph);
		vsp->ph = ph;
	}

	if(vsp->drbuf.rpos < vsp->drbuf.size)
		events |= POLLIN;
	if(buf_size_avail(&vsp->dwbuf))
		events |= POLLOUT;

	pthread_mutex_unlock(&vsp->tm_mutex);

	fuse_reply_poll(req, events);
}

#define RTS_DTR (TIOCM_RTS|TIOCM_DTR)

static int set_bits(struct vsp *vsp, int bits) {
	if(bits == vsp->mbits)
		return 0;
	if(vsp->fd_ctl != -1 && ((vsp->mbits & RTS_DTR) != (bits & RTS_DTR))){
		int data = 0;
		if((bits & TIOCM_RTS) && (vsp->flags & VSPFL_RTS_IS_PTT))
			data |= 1;
		if((bits & TIOCM_DTR) && (vsp->flags & VSPFL_DTR_IS_PTT))
			data |= 1;
		dbg1("VSP %s RTS: %d DTR %d", vsp->devname, bits & TIOCM_RTS ? 1:0, bits & TIOCM_DTR ? 1:0);
		if(send_ctlmsg(vsp->fd_ctl, CMID_PTT, data) < 0) {
			err_e(-errno, "VSP %s Could not send ctl message!", vsp->devname);
			close(vsp->fd_ctl);
			vsp->fd_ctl = -1;
			return -1;
		}
	}

	vsp->mbits = bits;

	return 0;
}

static void dv_ioctl(fuse_req_t req, int cmd, void *arg,
			struct fuse_file_info *fi, unsigned flags,
			const void *in_buf, size_t in_bufsz, size_t out_bufsz)
{
	struct vsp *vsp = fuse_req_userdata(req);
	dbg1("VSP %s Ioctl, cmd: 0x%0x, arg: 0x%0x, in_buf: 0x%0x, in_buf size: %d, out_bufsz: %d", vsp->devname, cmd, (unsigned)arg, (unsigned)in_buf, in_bufsz, out_bufsz);

	switch(cmd) {
	case TCGETS:
		if(!out_bufsz) {
			struct iovec iov = { arg, sizeof(vsp->termios) };
			fuse_reply_ioctl_retry(req, NULL, 0, &iov, 1);
		} else {
			fuse_reply_ioctl(req, 0, &vsp->termios, sizeof(vsp->termios));
		}
		break;

	case TCSETS:
		if(!in_bufsz) {
			struct iovec iov = { arg, sizeof(vsp->termios) };
			fuse_reply_ioctl_retry(req, &iov, 1, NULL, 0);
		} else {
			int baud;
			int err = 0;
			if(in_bufsz > sizeof(vsp->termios))
				in_bufsz = sizeof(vsp->termios);
			memcpy(&vsp->termios, in_buf, in_bufsz);
			baud = termios_baud_rate(&vsp->termios);
			if(baud > 0) {
				info("VSP %s Baud rate set to %d", vsp->devname, baud);
			} else {
				warn("VSP %s unsupported baud rate!", vsp->devname);
				err = -1;
			}

			fuse_reply_ioctl(req, err, NULL, 0);
		}
		break;

	case FIONREAD:
		if(!out_bufsz) {
			struct iovec iov = { arg, sizeof(int) };
			fuse_reply_ioctl_retry(req, NULL, 0, &iov, 1);
		} else {
			int avail;
			pthread_mutex_lock(&vsp->tm_mutex);
			avail = vsp->drbuf.size - vsp->drbuf.rpos;
			pthread_mutex_unlock(&vsp->tm_mutex);
			// dbg1("VSP %s FIONREAD returned %d", vsp->devname, avail);
			fuse_reply_ioctl(req, 0, &avail, sizeof(int));
		}
		break;

	case TIOCMGET:
		if(!out_bufsz) {
			struct iovec iov = { arg, sizeof(vsp->mbits) };
			fuse_reply_ioctl_retry(req, NULL, 0, &iov, 1);
		} else {
			fuse_reply_ioctl(req, 0, &vsp->mbits, sizeof(vsp->mbits));
		}
		break;

	case TIOCMSET:
		if(!in_bufsz) {
			struct iovec iov = { arg, sizeof(vsp->mbits) };
			fuse_reply_ioctl_retry(req, &iov, 1, NULL, 0);
		} else {
			set_bits(vsp, *((int*)in_buf));
			fuse_reply_ioctl(req, 0, NULL, 0);
		}
		break;

	case TIOCMBIS:
		if(!in_bufsz) {
			struct iovec iov = { arg, sizeof(int) };
			fuse_reply_ioctl_retry(req, &iov, 1, NULL, 0);
		} else {
			set_bits(vsp, (*((int*)in_buf) | vsp->mbits));
			fuse_reply_ioctl(req, 0, NULL, 0);
		}
		break;

	case TIOCMBIC:
		if(!in_bufsz) {
			struct iovec iov = { arg, sizeof(int) };
			fuse_reply_ioctl_retry(req, &iov, 1, NULL, 0);
		} else {
			set_bits(vsp, ~(*((int*)in_buf)) & vsp->mbits);
			fuse_reply_ioctl(req, 0, NULL, 0);
		}
		break;

	case TCFLSH:
		fuse_reply_ioctl(req, 0, NULL, 0);
		break;

	default:
		warn("VSP %s ioctl 0x%x not implemented!", vsp->devname, cmd);
		fuse_reply_err(req, EINVAL);
	}

	// fuse_reply_ioctl(req, /* result */ 0, /* buffer */ NULL, /* size */ 0);
}

static const struct cuse_lowlevel_ops vsp_clop = {
        .open           = dv_open,
	.release	= dv_release,
        .read           = dv_read,
        .write          = dv_write,
        .ioctl          = dv_ioctl,
	.poll           = dv_poll,
};

int vsp_create(const struct vsp_config *vcfg, int fd_data, int fd_ctl) {

	static const char *fargv[] = { "mhux", "-f", NULL };
	static const struct fuse_args fargs = {
		.argc = 2,
		.argv = (char **)fargv,
		.allocated = 0
	};

	char dev_name[128] = "DEVNAME=";
	const char *dev_info_argv[] = { dev_name };
	struct cuse_info ci;
	int err = 0;
	struct vsp *vsp;

	if(!vcfg->devname) {
		err("Could not create VSP. No device name specified!\n");
		return -1;
	}

	vsp = w_calloc(1, sizeof(*vsp));
	vsp->devname = w_strdup(vcfg->devname);
	vsp->flags = vcfg->flags;
	vsp->fd_data = fd_data;
	vsp->fd_ctl  = fd_ctl;
	pthread_mutex_init(&vsp->tm_mutex, NULL);
	pthread_cond_init(&vsp->tc_socktx, NULL);
	pthread_cond_init(&vsp->tc_read, NULL);
	pthread_cond_init(&vsp->tc_write, NULL);

	vsp->termios.c_cflag = B19200 | CS8 | CLOCAL | CREAD | CRTSCTS;

	strncat(dev_name, vcfg->devname, sizeof(dev_name) - 9);
	memset(&ci, 0, sizeof(ci));
        ci.dev_info_argc = 1;
        ci.dev_info_argv = dev_info_argv;
	ci.flags = CUSE_UNRESTRICTED_IOCTL;

	vsp->se = cuse_lowlevel_setup(fargs.argc, fargs.argv, &ci, &vsp_clop, NULL, vsp);
	if(!vsp->se) {
		err("Could not setup VSP %s!", vcfg->devname);
		goto failed;
	}

	if(!vsp_list_init) {
		PG_NewList(&vsp_list);
		vsp_list_init++;
	}
	PG_AddTail(&vsp_list, &vsp->node);

	err = pthread_create(&vsp->th_worker, NULL, &t_worker, vsp);
	if(err) {
		err("VSP %s Could not create worke!", vsp->devname);
		vsp_destroy(vsp->devname);
	}

	err = pthread_create(&vsp->th_sockrx, NULL, &t_sockrx, vsp);
	if(err) {
		err("VSP %s Could not create sockrx!", vsp->devname);
		vsp_destroy(vsp->devname);
	}

	err = pthread_create(&vsp->th_socktx, NULL, &t_socktx, vsp);
	if(err) {
		err("VSP %s Could not create socktx!", vsp->devname);
		vsp_destroy(vsp->devname);
	}

	return err;

	failed:
	if(vsp && vsp->se)
		fuse_session_destroy(vsp->se);
		//cuse_lowlevel_teardown(vsp->se);
	if(vsp)
		free(vsp);
	return -1;
}

static struct vsp *find_vsp(const char *devname) {
	struct vsp *vsp, *pvsp;

	if(!vsp_list_init)
		return NULL;

	vsp = NULL;
	PG_SCANLIST(&vsp_list, pvsp) {
		if(!strcmp(pvsp->devname, devname)) {
			vsp = pvsp;
			break;
		}
	}
	return vsp;
}

static int term_thread(pthread_t th, const char *devname, const char *name) {
	int i, err;
	struct timespec ts = { 0, 10000000 };

	for(i = 0; i < 100; i++) {
		if(ESRCH == pthread_kill(th, 0))
			break;
		nanosleep(&ts, NULL);
	}

	if(ESRCH != pthread_kill(th, 0)) {
		warn("VSP %s %s not terminated yet, canceling", devname, name);
		if(0 != (err = pthread_cancel(th)))
			warn_e(-err, "VSP %s Could not cancel %s", devname, name);
	}
	if(0 != (err = pthread_join(th, NULL)))
		warn_e(-err, "VSP %s Could not join %s", devname, name);
	dbg0("VSP %s thread %s terminated", devname, name);
	return 0;
}

int vsp_destroy(const char *devname) {
	struct vsp *vsp;

	vsp = find_vsp(devname);
	if(vsp) {
		PG_Remove(&vsp->node);

		vsp->terminate = 1;

		pthread_cond_signal(&vsp->tc_read);
		pthread_cond_signal(&vsp->tc_write);

		pthread_mutex_lock(&vsp->tm_mutex);
		if(vsp->ph) {
			fuse_pollhandle_destroy(vsp->ph);
			vsp->ph = NULL;
		}
		pthread_mutex_unlock(&vsp->tm_mutex);

		fuse_session_exit(vsp->se);
		pthread_cond_signal(&vsp->tc_socktx);
		pthread_kill(vsp->th_worker, SIGHUP);
		pthread_kill(vsp->th_sockrx, SIGHUP);
		pthread_kill(vsp->th_socktx, SIGHUP);

		term_thread(vsp->th_sockrx, vsp->devname, "socketrx");
		term_thread(vsp->th_socktx, vsp->devname, "sockettx");
		term_thread(vsp->th_worker, vsp->devname, "worker");

		if(vsp->fd_data != -1)
				close(vsp->fd_data);

		if(vsp->fd_ctl != -1)
			close(vsp->fd_ctl);

		if(vsp->se)
			fuse_session_destroy(vsp->se);
			//cuse_lowlevel_teardown(vsp->se);

		if(vsp->devname)
			free(vsp->devname);

		free(vsp);
	}


	return 0;
}


int vsp_destroy_all() {
	struct vsp *vsp;

	if(!vsp_list_init)
		return 0;

	while((vsp = (void*)PG_FIRSTENTRY(&vsp_list)))
		vsp_destroy(vsp->devname);
	return 0;
}
