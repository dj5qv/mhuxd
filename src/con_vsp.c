/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2015  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ev.h>
#include <fuse.h>
#include <cuse_lowlevel.h>
#include "con_vsp.h"
#include "linux_termios.h"
#include "pglist.h"
#include "util.h"
#include "logger.h"
#include "buffer.h"
#include "conmgr.h"

#define MOD_ID "vsp"

struct vsp {
	/* Connector-owned endpoint from socketpair (connector side). */
	int fd_data;
	/* Connector-owned PTT endpoint from socketpair (connector side), may alias fd_data. */
	int fd_ptt;
	enum mhuxd_io_state state;

	ev_io w_data_in;
	ev_io w_data_out;
	ev_io w_chan_in;

	int max_con;
	int rts_is_ptt : 1;
	int dtr_is_ptt : 1;

	int open_cnt;
	struct PGList session_list;

	struct termios termios;
	struct termios2 termios2;
	int mbits;

	struct ev_loop *loop;
	char *devname;
	struct fuse_session *se;
	char *chan_buf;
	size_t chan_buf_size;
	int force_cuse_fail_once;
	unsigned int dbg_cb_after_terminal;
	unsigned int dbg_watch_invalid_fd;
};

struct vsp_session {
	struct PGNode node;
	struct vsp *vsp;
	struct fuse_pollhandle *ph;
	int fd;
	int fd_flags;
	int client_pid;
	struct buffer buf_out;  // VSP -> client app
	struct buffer buf_in;   // client app -> VSP

	fuse_req_t pending_in_req;
	size_t pending_in_size;
	size_t pending_in_processed;
	const char *pending_in_buf;

	fuse_req_t pending_out_req;
	size_t pending_out_size;
	size_t pending_out_processed;
	size_t pending_out_buf_capacity;
	char *pending_out_buf;

	struct serial_icounter_struct sis;

	uint32_t ptt_status;
};


static struct vsp_session *find_vs(struct vsp *vsp, int fd) {
	struct vsp_session *vs;
	PG_SCANLIST(&vsp->session_list, vs) {
		if(vs->fd == fd)
			return vs;
	}
	return NULL;
}

#define RTS_DTR (TIOCM_RTS|TIOCM_DTR)

static int vsp_is_terminal(const struct vsp *vsp) {
	return vsp->state == MHUXD_IO_FAILED || vsp->state == MHUXD_IO_CLOSED;
}

static void vsp_dbg_terminal_cb(struct vsp *vsp, const char *cb_name) {
	vsp->dbg_cb_after_terminal++;
	dbg0("%s callback %s after terminal state %s (count=%u)",
	     vsp->devname, cb_name, io_state_to_str(vsp->state), vsp->dbg_cb_after_terminal);
}

static void vsp_set_state(struct vsp *vsp, enum mhuxd_io_state state) {
	if(vsp->state == state)
		return;
	if(!io_state_transition(&vsp->state, state)) {
		dbg0("%s illegal state transition %s -> %s",
		     vsp->devname, io_state_to_str(vsp->state), io_state_to_str(state));
	}
}

static void vsp_watch_start(struct vsp *vsp, ev_io *watcher) {
	if(vsp_is_terminal(vsp)) {
		dbg1("%s refusing watcher start in terminal state %s",
		     vsp->devname, io_state_to_str(vsp->state));
		return;
	}
	if(watcher->fd < 0) {
		vsp->dbg_watch_invalid_fd++;
		dbg0("%s refusing watcher start with invalid fd=%d (count=%u)",
		     vsp->devname, watcher->fd, vsp->dbg_watch_invalid_fd);
		return;
	}
	ev_io_start(vsp->loop, watcher);
}

static void vsp_watch_stop(struct vsp *vsp, ev_io *watcher) {
	ev_io_stop(vsp->loop, watcher);
}

static void vsp_watch_stop_all(struct vsp *vsp) {
	vsp_watch_stop(vsp, &vsp->w_chan_in);
	vsp_watch_stop(vsp, &vsp->w_data_in);
	vsp_watch_stop(vsp, &vsp->w_data_out);
}

static void vsp_close_endpoints(struct vsp *vsp) {
	if(vsp->fd_ptt == vsp->fd_data)
		vsp->fd_ptt = -1;
	fd_close(&vsp->fd_data);
	fd_close(&vsp->fd_ptt);
}

static void vsp_fail(struct vsp *vsp, int errnum, const char *msg) {
	if(vsp_is_terminal(vsp))
		return;

	vsp_set_state(vsp, MHUXD_IO_FAILED);
	vsp_watch_stop_all(vsp);
	vsp_close_endpoints(vsp);
	if(vsp->se)
		fuse_session_exit(vsp->se);

	if(errnum)
		err_e(errnum, "%s (%s)", msg, vsp->devname);
	else
		err("%s (%s)", msg, vsp->devname);
}

static int env_enabled(const char *name) {
	const char *val = getenv(name);
	if(!val)
		return 0;
	if(val[0] == '1')
		return 1;
	if(val[0] == 'y' || val[0] == 'Y')
		return 1;
	if(val[0] == 't' || val[0] == 'T')
		return 1;
	if(val[0] == 'o' || val[0] == 'O')
		return 1;
	return 0;
}


// cuse_lowlevel_setup writes nice error messages to stderr which is useless for us running as a
// daemon. Check /dev/cuse availability here and do some helpful error reporting to the log.
static int check_cuse_dev(const char *devname) {
	const char *cusepath = "/dev/cuse";
	int fd;

	dbg1("%s()", __func__);

	fd = open(cusepath, O_RDWR);
	if (fd == -1) {
		if (errno == ENODEV || errno == ENOENT)
			err("/dev/cuse device not found, try 'modprobe cuse' first\n");
		else
			err_e(errno, "can't open /dev/cuse!");
		return -1;
	}
	close(fd);

	struct stat buf;
	char devpath[128];
	snprintf(devpath, sizeof(devpath)-1, "/dev/mhuxd/%s", devname);
	if(-1 == stat(devpath, &buf)) {
		if(errno == ENOENT)
			return 0;
		err_e(errno, "stat() on %s failed!", devpath);
	}
	err("%s already exist!", devpath);

	return -1;
}

static int set_bits(struct vsp_session *vs, int bits) {
	struct vsp *vsp = vs->vsp;
	if(vsp_is_terminal(vsp))
		return -1;

	if(bits == vsp->mbits)
                return 0;
	if(!vsp->rts_is_ptt && !vsp->dtr_is_ptt)
		return 0;

	ssize_t res;
	int errsv = 0;
	enum mhuxd_io_rw_result io_res;
	int data = 0;
	if((bits & TIOCM_RTS) && vsp->rts_is_ptt)
		data |= 1;
	if((bits & TIOCM_DTR) && vsp->dtr_is_ptt)
		data |= 1;
	dbg1("%s() %s RTS: %d DTR %d", __func__, vsp->devname, bits & TIOCM_RTS ? 1:0, bits & TIOCM_DTR ? 1:0);

	uint8_t state = data ? '1' : '0';
	//	buf_append(&vs->buf_in, &state, 1);
	io_res = io_write_nonblock(vsp->fd_ptt, &state, 1, &res, &errsv);
	if(io_res == MHUXD_IO_RW_PROGRESS && res == 1)
		vs->ptt_status = state;
	if(io_res != MHUXD_IO_RW_PROGRESS || res != 1) {
		if(io_res == MHUXD_IO_RW_ERROR)
			err_e(errsv, "%s could not write to ptt channel!", vsp->devname);
		else
			err("%s could not write to ptt channel!", vsp->devname);
	}

	//ev_io_start(vsp->loop, &vsp->w_data_out);

        vsp->mbits = bits;

        return 0;
}


static void chan_in_cb (struct ev_loop *loop, struct ev_io *w, int revents) {
	(void)loop; (void)revents;
	struct vsp *vsp = w->data;
	struct fuse_session *se = vsp->se;

	if(vsp_is_terminal(vsp)) {
		vsp_dbg_terminal_cb(vsp, __func__);
		return;
	}

	if(vsp->force_cuse_fail_once) {
		vsp->force_cuse_fail_once = 0;
		errno = EIO;
		vsp_fail(vsp, errno, "cuse read error (fault injection)");
		return;
	}

	dbg1("%s()", __func__);
	
        int res = 0;
        struct fuse_chan *ch = fuse_session_next_chan(se, NULL);

	struct fuse_chan *tmpch = ch;
	res = fuse_chan_recv(&tmpch, vsp->chan_buf, vsp->chan_buf_size);
	if(res > 0)
		fuse_session_process(se, vsp->chan_buf, res, tmpch);

	if (res <= 0 && res != -EINTR) {
		// Now this should normally never happen.
		// We can provoke it for testing by setting the chan_buf_size to a small value and access
		// the VSP.
		(void)se;
		vsp_fail(vsp, errno, "cuse read error");
	}
}

static void data_in_cb (struct ev_loop *loop, struct ev_io *w, int revents) {
	(void)loop; (void)revents;
	struct vsp *vsp = w->data;
	struct vsp_session *vs;
	uint8_t buf[1024];
	ssize_t size, avail;
	int errsv = 0;
	enum mhuxd_io_rw_result io_res;

	if(vsp_is_terminal(vsp)) {
		vsp_dbg_terminal_cb(vsp, __func__);
		return;
	}

	dbg1("%s()", __func__);
	
	do {
		io_res = io_read_nonblock(w->fd, buf, sizeof(buf), &size, &errsv);
		if(io_res == MHUXD_IO_RW_ERROR) {
			vsp_fail(vsp, errsv, "error reading data from router");
			break;
		}

		if(io_res == MHUXD_IO_RW_WOULD_BLOCK || io_res == MHUXD_IO_RW_EOF)
			break;

		dbg1_h(vsp->devname, "k => vsp", buf, size);

		PG_SCANLIST(&vsp->session_list, vs) {
			struct buffer *b = &vs->buf_out;

			if(vs->fd_flags & O_WRONLY)
				continue;

			avail = buf_size_avail(b);

			if(size > avail) {
				warn("%s not enough buffer space available (%zd/%zd) client pid %d",
				     vsp->devname, size, avail, vs->client_pid);
				size = avail;
			}

			buf_append(b, buf, size);

			if(vs->pending_out_req) {
				size_t quanta = b->size - b->rpos;
				if(quanta > (vs->pending_out_size - vs->pending_out_processed))
					quanta = vs->pending_out_size - vs->pending_out_processed;

				memcpy(vs->pending_out_buf + vs->pending_out_processed,
				       b->data + b->rpos, quanta);
				buf_consume(b, quanta);
				vs->pending_out_processed += quanta;
				if(vs->pending_out_processed == vs->pending_out_size) {
					fuse_reply_buf(vs->pending_out_req, vs->pending_out_buf, vs->pending_out_size);
					vs->pending_out_req = NULL;
					vs->pending_out_size = 0;
					vs->pending_out_processed = 0;
				}
			}

			if(vs->ph) {
				fuse_lowlevel_notify_poll(vs->ph);
				fuse_pollhandle_destroy(vs->ph);
				vs->ph = NULL;
			}

			vs->sis.rx += size;
		}

	} while(size > 0);
}

static void data_out_cb (struct ev_loop *loop, struct ev_io *w, int revents) {
	(void)loop; (void)revents;
	struct vsp *vsp = w->data;
	struct vsp_session *vs;
	ssize_t size;
	int need_to_write = 0;
	int errsv = 0;
	enum mhuxd_io_rw_result io_res;

	if(vsp_is_terminal(vsp)) {
		vsp_dbg_terminal_cb(vsp, __func__);
		return;
	}

	dbg1("%s()", __func__);

	PG_SCANLIST(&vsp->session_list, vs) {
		struct buffer *b = &vs->buf_in;
		if(b->rpos == b->size)
			continue;
		io_res = io_write_nonblock(vsp->fd_data, b->data + b->rpos, b->size - b->rpos, &size, &errsv);
		if(io_res == MHUXD_IO_RW_WOULD_BLOCK) {
			need_to_write = 1;
			continue;
		}
		if(io_res == MHUXD_IO_RW_ERROR) {
			//FIXME: handle error.
			vsp_fail(vsp, errsv, "error writing data to router");
			return;
		}
		if(io_res == MHUXD_IO_RW_EOF)
			continue;

		if(size > 0) {
			dbg1_h(vsp->devname, "vsp => k", b->data + b->rpos, size);

			// If PTT channel, track PTT status
			if(vsp->fd_data == vsp->fd_ptt) {
				int i;
				for(i = 0; i < (b->size - b->rpos); i++) {
					switch( b->data[i]) {
					case '0':
						vs->ptt_status = 0;
						break;
					case '1':
						vs->ptt_status = 1;
						break;
					}
				}
			}

			buf_consume(b, size);
			vs->sis.tx += size;

			if(vs->pending_in_req) {
				vs->pending_in_processed += buf_append(b,
					   (uint8_t*)vs->pending_in_buf + vs->pending_in_processed,
					   vs->pending_in_size - vs->pending_in_processed);
				if(vs->pending_in_processed == vs->pending_in_size) {
					fuse_reply_write(vs->pending_in_req, vs->pending_in_size);
					vs->pending_in_size = 0;
					vs->pending_in_req = NULL;
					vs->pending_in_buf = NULL;
				}
			}
		}

		if(b->rpos < b->size)
			need_to_write = 1;
	}

	if(!need_to_write)
		vsp_watch_stop(vsp, &vsp->w_data_out);
}

static void dv_open(fuse_req_t req, struct fuse_file_info *fi)
{
        struct vsp *vsp = fuse_req_userdata(req);
        int err = 0;
        info("%s req open", vsp->devname);

	if(vsp->open_cnt >= vsp->max_con) {
                warn("%s open by pid %d failed, maximum number of connections reached!", 
		     vsp->devname, fuse_req_ctx(req)->pid);
                err = EBUSY;
		goto out;
	}

	vsp->open_cnt++;
	struct vsp_session *vs = w_calloc(1, sizeof(*vs));
	vs->vsp = vsp;
	vs->fd = fi->fh;
	vs->fd_flags = fi->flags;
	vs->client_pid = fuse_req_ctx(req)->pid;;

	PG_AddTail(&vsp->session_list, &vs->node);

	// ev_io_start(vsp->loop, &vsp->w_data_in);
 out:
        if(!err)
                fuse_reply_open(req, fi);
        else
                fuse_reply_err(req, err);


}

static void dv_release(fuse_req_t req, struct fuse_file_info *fi)
{
        struct vsp *vsp = fuse_req_userdata(req);
        int err = 0;
        info("%s req release", vsp->devname);

	struct vsp_session *vs = find_vs(vsp, fi->fh);
	if(vs == NULL) {
		err = EBADF;
		err("attempt to close a non-existent connection!");
		goto out;
	}

	if(vs->ptt_status == 1) {
		uint8_t state = '0';
		ssize_t res;
		int errsv = 0;
		enum mhuxd_io_rw_result io_res = io_write_nonblock(vsp->fd_ptt, &state, 1, &res, &errsv);
		if(io_res != MHUXD_IO_RW_PROGRESS || res != 1)
			err("%s() %s could not send PTT off", __func__, vsp->devname);
	}

	if(vs->pending_in_req)
		fuse_reply_err(vs->pending_in_req, EINTR);
	if(vs->pending_out_req)
		fuse_reply_err(vs->pending_out_req, EINTR);

	if(vs->pending_out_buf)
		free(vs->pending_out_buf);

	vsp->open_cnt--;
	PG_Remove(&vs->node);
	if(vs->ph)
		fuse_pollhandle_destroy(vs->ph);
	free(vs);

//	if(!vsp->open_cnt)
//		ev_io_stop(vsp->loop, &vsp->w_data_in);
// better continue to read & discard.

 out:
	fuse_reply_err(req, err);
}

static void interrupt_func(fuse_req_t req, void *data) {
	(void)req;
	struct vsp_session *vs = data;
	dbg1("%s() %s", __func__, vs->vsp->devname);
	if(vs->pending_in_req) {
		fuse_reply_err(vs->pending_in_req, EINTR);
		vs->pending_in_req = NULL;
		vs->pending_in_size = 0;
	}
	if(vs->pending_out_req) {
		fuse_reply_err(vs->pending_out_req, EINTR);
		vs->pending_out_req = NULL;
		vs->pending_out_size = 0;
		vs->pending_out_processed = 0;
	}
}

static void dv_read(fuse_req_t req, size_t size, off_t off,
		    struct fuse_file_info *fi)
{
	(void)off;
	struct vsp *vsp = fuse_req_userdata(req);
	int err = 0;
	dbg1("%s() %s request size: %zd", __func__, vsp->devname, size);

	struct vsp_session *vs = find_vs(vsp, fi->fh);
	if(vs == NULL) {
		err = EBADF;
		err("attempt to read from a non-existent connection!");
		goto out;
	}

	if(vs->pending_out_size) {
		err = EINVAL;
		err("read called while blocking read pending!");
		goto out;
	}

	struct buffer *b = &vs->buf_out;

	if(!(fi->flags & O_NONBLOCK) && size > (size_t)(b->size - b->rpos)) {
		// Blocking read request. Since request size may exceed the
		// size of our struct buffer use a separate dynamic buffer.
		vs->pending_out_req = req;
		vs->pending_out_size = size;
		if(vs->pending_out_buf_capacity < size) {
			if(vs->pending_out_buf)
				free(vs->pending_out_buf);
			vs->pending_out_buf = w_malloc(size);
			vs->pending_out_buf_capacity = size;
		}
		memcpy(vs->pending_out_buf, b->data + b->rpos, b->size - b->rpos);
		vs->pending_out_processed = b->size - b->rpos;
		buf_reset(b);


		fuse_req_interrupt_func(req, interrupt_func, vs);
		return;
	}

	if(size > (size_t)(b->size - b->rpos))
		size = b->size - b->rpos;
out:
	if(!err) {
		fuse_reply_buf(req, (char *)(b->data + b->rpos), size);
		buf_consume(b, size);
	} else {
		fuse_reply_err(req, err);
	}
}

static void dv_write(fuse_req_t req, const char *buf, size_t size,
		     off_t off, struct fuse_file_info *fi)
{
	(void)off;
	struct vsp *vsp;
	int err = 0;
	struct vsp_session *vs;

	vsp = fuse_req_userdata(req);
	vs = find_vs(vsp, fi->fh);

	if(vs == NULL) {
		err = EBADF;
		err("attempt to write to a non-existent connection!");
		goto out;
	}

	if(vs->pending_in_size) {
		err = EINVAL;
		err("write called while blocking write pending!");
		goto out;
	}

	struct buffer *b = &vs->buf_in;

	if(!(fi->flags & O_NONBLOCK) && size > buf_size_avail(b)) {
		vs->pending_in_req = req;
		vs->pending_in_size = size;
		vs->pending_in_buf = buf;
		vs->pending_in_processed = buf_append(b, (uint8_t*)buf, buf_size_avail(b));
		vsp_watch_start(vsp, &vsp->w_data_out);
		fuse_req_interrupt_func(req, interrupt_func, vs);
		return;
	}

	if(size > buf_size_avail(b))
		size = buf_size_avail(b);

	buf_append(b, (uint8_t*)buf, size);
	vsp_watch_start(vsp, &vsp->w_data_out);

out:
	if(!err)
		fuse_reply_write(req, size);
	else
		fuse_reply_err(req, err);
}

static void dv_poll(fuse_req_t req, struct fuse_file_info *fi,
                    struct fuse_pollhandle *ph) 
{
	struct vsp *vsp = fuse_req_userdata(req);
        int err = 0;
        unsigned events = 0;

	dbg1("%s() %s, ph: %0lx", __func__, vsp->devname, (unsigned long)ph);

	struct vsp_session *vs = find_vs(vsp, fi->fh);
	if(vs == NULL) {
		err = EBADF;
		err("attempt to poll a non-existent connection!");
		goto out;
	}

	if(ph != vs->ph) {
                if(vs->ph)
                        fuse_pollhandle_destroy(vs->ph);
                vs->ph = ph;
        }

	if(vs->buf_out.rpos < vs->buf_out.size)
		events |= POLLIN;

	if(buf_size_avail(&vs->buf_in))
		events |= POLLOUT;

out:
        if(!err)
                fuse_reply_poll(req, events);
        else
                fuse_reply_err(req, err);
}

static void dv_ioctl(fuse_req_t req, int cmd, void *arg,
		     struct fuse_file_info *fi, unsigned flags,
		     const void *in_buf, size_t in_bufsz, size_t out_bufsz)
{
	(void)flags;
	struct vsp *vsp = fuse_req_userdata(req);
	dbg1("%s() %s, cmd: 0x%0x, arg: 0x%0lx, in_buf: 0x%0lx, in_buf size: %zd, out_bufsz: %zd", __func__, 
	     vsp->devname, cmd, (unsigned long)arg, (unsigned long)in_buf, in_bufsz, out_bufsz);

	struct vsp_session *vs = find_vs(vsp, fi->fh);
	if(vs == NULL) {
		err("attempt to ioctl on a non-existent connection!");
		fuse_reply_err(req, EBADF);
		return;
	}

	switch(cmd) {
	case TCGETS:
		if(!out_bufsz) {
			struct iovec iov = { arg, sizeof(vsp->termios) };
			fuse_reply_ioctl_retry(req, NULL, 0, &iov, 1);
		} else {
			fuse_reply_ioctl(req, 0, &vsp->termios, sizeof(vsp->termios));
		}
		break;

	case TCGETS2:
		if(!out_bufsz) {
			struct iovec iov = { arg, sizeof(vsp->termios2) };
			fuse_reply_ioctl_retry(req, NULL, 0, &iov, 1);
		} else {
			fuse_reply_ioctl(req, 0, &vsp->termios2, sizeof(vsp->termios2));
		}
		break;

	case TCSETSF2:
		buf_reset(&vs->buf_out);
	case TCSETSW2:
	case TCSETS2:
		if(!in_bufsz) {
			struct iovec iov = { arg, sizeof(vsp->termios2) };
			fuse_reply_ioctl_retry(req, &iov, 1, NULL, 0);
		} else {
			int baud;
			int err = 0;
			if(in_bufsz > sizeof(vsp->termios2))
				in_bufsz = sizeof(vsp->termios2);
			memcpy(&vsp->termios2, in_buf, in_bufsz);
			memcpy(&vsp->termios, in_buf, sizeof(vsp->termios));
			if(vsp->termios2.c_cflag & BOTHER)
				baud = vsp->termios2.c_ospeed;
			else
				baud = termios_baud_rate(&vsp->termios);
			if(baud > 0) {
				vsp->termios2.c_ispeed = baud;
				vsp->termios2.c_ospeed = baud;
				info("%s TCSETS2 Baud rate set to %d", vsp->devname, baud);
			} else {
				warn("%s TCSETS2 unsupported baud rate!", vsp->devname);
				err = -1;
			}

			fuse_reply_ioctl(req, err, NULL, 0);
		}
		break;

	case TCSETSF:
		buf_reset(&vs->buf_out);
	case TCSETSW:
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
			memcpy(&vsp->termios2, in_buf, in_bufsz);
			baud = termios_baud_rate(&vsp->termios);
			if(baud > 0) {
				vsp->termios2.c_ispeed = baud;
				vsp->termios2.c_ospeed = baud;
				info("%s TCSETS Baud rate set to %d", vsp->devname, baud);
			} else {
				warn("%s TCSETS unsupported baud rate!", vsp->devname);
				err = -1;
			}

			fuse_reply_ioctl(req, err, NULL, 0);
		}
		break;

	case TIOCOUTQ:
		if(!out_bufsz) {
			struct iovec iov = { arg, sizeof(int) };
			fuse_reply_ioctl_retry(req, NULL, 0, &iov, 1);
		} else {
			int avail;
			avail = vs->buf_in.size - vs->buf_in.rpos;
			fuse_reply_ioctl(req, 0, &avail, sizeof(int));
		}
		break;

	case FIONREAD:
		if(!out_bufsz) {
			struct iovec iov = { arg, sizeof(int) };
			fuse_reply_ioctl_retry(req, NULL, 0, &iov, 1);
		} else {
			int avail;
			avail = vs->buf_out.size - vs->buf_out.rpos;
			dbg1("%s FIONREAD returned %d, sizeof(int) %d", vsp->devname, avail, (int)sizeof(int));
			fuse_reply_ioctl(req, 0, &avail, sizeof(int));
		}
		break;

	case TIOCMGET:
		if(!out_bufsz) {
			struct iovec iov = { arg, sizeof(int) };
			fuse_reply_ioctl_retry(req, NULL, 0, &iov, 1);
		} else {
			fuse_reply_ioctl(req, 0, &vsp->mbits, sizeof(int));
		}
		break;

	case TIOCMSET:
		if(!in_bufsz) {
			struct iovec iov = { arg, sizeof(int) };
			fuse_reply_ioctl_retry(req, &iov, 1, NULL, 0);
		} else {
			set_bits(vs, *((int*)in_buf));
			fuse_reply_ioctl(req, 0, NULL, 0);
		}
		break;

	case TIOCMBIS:
		if(!in_bufsz) {
			struct iovec iov = { arg, sizeof(int) };
			fuse_reply_ioctl_retry(req, &iov, 1, NULL, 0);
		} else {
			set_bits(vs, (*((int*)in_buf) | vsp->mbits));
			fuse_reply_ioctl(req, 0, NULL, 0);
		}
		break;

	case TIOCMBIC:
		if(!in_bufsz) {
			struct iovec iov = { arg, sizeof(int) };
			fuse_reply_ioctl_retry(req, &iov, 1, NULL, 0);
		} else {
			set_bits(vs, ~(*((int*)in_buf)) & vsp->mbits);
			fuse_reply_ioctl(req, 0, NULL, 0);
		}
		break;

	case TCFLSH:
		switch((int)(long)in_buf) {
		case TCIFLUSH:
			buf_reset(&vs->buf_out);
			break;
		case TCOFLUSH:
			buf_reset(&vs->buf_in);
			break;
		case TCIOFLUSH:
			buf_reset(&vs->buf_in);
			buf_reset(&vs->buf_out);
			break;
		}

		fuse_reply_ioctl(req, 0, NULL, 0);
		break;

	case TIOCMIWAIT:
		warn("TIOCMIWAIT 1!");
		if(!in_bufsz) {
			struct iovec iov = { arg, sizeof(int) };
			fuse_reply_ioctl_retry(req, &iov, 1, NULL, 0);
		} else {
			int arg = *((int*)in_buf);
			warn("TIOCMIWAIT 2!");

			if(arg & TIOCM_LE)
				dbg1("TIOCMIWAIT / TIOCM_LE");
			if(arg & TIOCM_RTS)
				dbg1("TIOCMIWAIT / TIOCM_RTS");
			if(arg & TIOCM_ST)
				dbg1("TIOCMIWAIT / TIOCM_ST");
			if(arg & TIOCM_SR)
				dbg1("TIOCMIWAIT / TIOCM_SR");
			if(arg & TIOCM_CTS)
				dbg1("TIOCMIWAIT / TIOCM_CTS");
			if(arg & TIOCM_CAR)
				dbg1("TIOCMIWAIT / TIOCM_CAR");
			if(arg & TIOCM_RNG)
				dbg1("TIOCMIWAIT / TIOCM_RNG");
			if(arg & TIOCM_DSR)
				dbg1("TIOCMIWAIT / TIOCM_DSR");

			fuse_reply_ioctl(req, 0, NULL, 0);
		}
		break;

	case TIOCGICOUNT:
		if(!in_bufsz) {
			struct iovec iov = { arg, sizeof(struct serial_icounter_struct) };
			fuse_reply_ioctl_retry(req, &iov, 1, NULL, 0);
		} else {
			dbg1("TIOCGICOUNT");
			fuse_reply_ioctl(req, 0, &vs->sis, sizeof(struct serial_icounter_struct));
		}
		break;

    case TIOCGSERIAL:
        if(!out_bufsz) {
            struct iovec iov = { arg, sizeof(struct serial_struct) };
            fuse_reply_ioctl_retry(req, NULL, 0, &iov, 1);
        } else {
            struct serial_struct ss;
            memset(&ss, 0, sizeof(ss));
            ss.type = PORT_16550A;
            ss.baud_base = 115200;
            fuse_reply_ioctl(req, 0, &ss, sizeof(ss));
        }
        break;

    case TIOCSSERIAL:
        if(!in_bufsz) {
            struct iovec iov = { arg, sizeof(struct serial_struct) };
            fuse_reply_ioctl_retry(req, &iov, 1, NULL, 0);
        } else {
            fuse_reply_ioctl(req, 0, NULL, 0);
        }
        break;

    case TIOCSBRK:
    case TIOCCBRK:
        fuse_reply_ioctl(req, 0, NULL, 0);
        break;

    case TIOCGSOFTCAR:
        if(!out_bufsz) {
            struct iovec iov = { arg, sizeof(int) };
            fuse_reply_ioctl_retry(req, NULL, 0, &iov, 1);
        } else {
            int softcar = 0;
            fuse_reply_ioctl(req, 0, &softcar, sizeof(int));
        }
        break;

    case TIOCSSOFTCAR:
        if(!in_bufsz) {
            struct iovec iov = { arg, sizeof(int) };
            fuse_reply_ioctl_retry(req, &iov, 1, NULL, 0);
        } else {
            fuse_reply_ioctl(req, 0, NULL, 0);
        }
        break;

    case TCSBRK:
		fuse_reply_ioctl(req, 0, NULL, 0);
		break;

        // FIXME: dummy implementation, maybe should do it correctly.
	case TIOCEXCL:
	case TIOCNXCL:
		fuse_reply_ioctl(req, 0, NULL, 0);
		break;


	default:
		warn("%s ioctl 0x%x not implemented!", vsp->devname, cmd);
		fuse_reply_err(req, EINVAL);
	}

	// fuse_reply_ioctl(req, /* result */ 0, /* buffer */ NULL, /* size */ 0);
}

static const struct cuse_lowlevel_ops vsp_clop = {
        .open           = dv_open,
        .release        = dv_release,
        .read           = dv_read,
        .write          = dv_write,
        .ioctl          = dv_ioctl,
        .poll           = dv_poll,
};

struct vsp *vsp_create(const struct connector_spec *cspec) {
	struct vsp *vsp;
	char devname[128];
	const char *dev_info_argv[] = { devname };

        static const char *fargv[] = { "mhuxd", "-f", NULL };
        static const struct fuse_args fargs = {
                .argc = 2,
                .argv = (char **)fargv,
                .allocated = 0
        };

	dbg1("%s()", __func__);

	const char *p = cspec->vsp.devname;
	if(p == NULL || !*p) {
		err("could not create vsp device: missing device name!");
		return NULL;
	}
	snprintf(devname, sizeof(devname) - 1, "DEVNAME=mhuxd/%s", p);
	devname[127] = 0x00;
	vsp = w_calloc(1, sizeof(*vsp));
	vsp->devname = w_strdup(p);
	PG_NewList(&vsp->session_list);
	vsp->termios.c_cflag = B19200 | CS8 | CLOCAL | CREAD | CRTSCTS;
	memcpy(&vsp->termios2, &vsp->termios, sizeof(vsp->termios));
	vsp->termios2.c_ispeed = 19200;
	vsp->termios2.c_ospeed = 19200;
	vsp->loop = cspec->loop;
	//	vsp->ptt_on_msg = w_strdup(cspec->ptt_on_msg);
	//	vsp->ptt_off_msg = w_strdup(cspec->ptt_off_msg);
	vsp->rts_is_ptt = cspec->vsp.ptt_rts ? 1 : 0;
	vsp->dtr_is_ptt = cspec->vsp.ptt_dtr ? 1 : 0;
	vsp->max_con = (cspec->vsp.maxcon > 0) ? cspec->vsp.maxcon : 1;
	vsp->state = MHUXD_IO_OPEN;
	vsp->fd_data = cspec->fd_data;
	vsp->fd_ptt = cspec->fd_ptt;
	vsp->force_cuse_fail_once = env_enabled("MHUXD_VSP_FORCE_CUSE_FAIL_ONCE");
	if(vsp->force_cuse_fail_once)
		warn("%s fault injection enabled: first CUSE channel read will fail", vsp->devname);

	struct cuse_info ci;
	memset(&ci, 0, sizeof(ci));
	ci.dev_info_argc = 1;
        ci.dev_info_argv = dev_info_argv;
        ci.flags = CUSE_UNRESTRICTED_IOCTL;
	if(check_cuse_dev(vsp->devname)) {
		err("could not setup VSP %s!", vsp->devname);
		goto failed;
	}
	vsp->se = cuse_lowlevel_setup(fargs.argc, fargs.argv, &ci, &vsp_clop, NULL, vsp);
        if(!vsp->se) {
                err("could not setup VSP %s!", vsp->devname);
                goto failed;
        }

	struct fuse_chan *ch = fuse_session_next_chan(vsp->se, NULL);
	if(ch == NULL) {
		err("could not obtain fuse channel!");
		goto failed;
	}

	//vsp->chan_buf_size = 5;
	vsp->chan_buf_size = fuse_chan_bufsize(ch);
	vsp->chan_buf = (char *) w_malloc(vsp->chan_buf_size);

	ev_io_init(&vsp->w_data_in, data_in_cb, cspec->fd_data, EV_READ);
	ev_io_init(&vsp->w_data_out, data_out_cb, cspec->fd_data, EV_WRITE);
	ev_io_init(&vsp->w_chan_in, chan_in_cb, fuse_chan_fd(ch), EV_READ);
	vsp->w_data_in.data = vsp;
	vsp->w_data_out.data = vsp;
	vsp->w_chan_in.data = vsp;

	vsp_watch_start(vsp, &vsp->w_chan_in);
	vsp_watch_start(vsp, &vsp->w_data_in);

	info("%s created", vsp->devname);

	return vsp;

 failed:
	if(vsp && vsp->se)
			fuse_session_destroy(vsp->se);
	//cuse_lowlevel_teardown(vsp->se);
	if(vsp) {
	if(vsp->devname)
		free(vsp->devname);
			free(vsp);
	}
	return NULL;
}

void vsp_destroy(struct vsp *vsp) {
	struct vsp_session *vs;

	dbg1("%s()", __func__);

	vsp_set_state(vsp, MHUXD_IO_CLOSED);

        while((vs = (void*)PG_FIRSTENTRY(&vsp->session_list))) {
		if(vs->ptt_status) {
			uint8_t state = '0';
			ssize_t res;
			int errsv = 0;
			enum mhuxd_io_rw_result io_res = io_write_nonblock(vsp->fd_ptt, &state, 1, &res, &errsv);
			if(io_res != MHUXD_IO_RW_PROGRESS || res != 1)
				err("%s() %s could not send PTT off", __func__, vsp->devname);
		}

		if(vs->ph)
			fuse_pollhandle_destroy(vs->ph);
		if(vs->pending_in_req)
			fuse_reply_err(vs->pending_in_req, EINTR);
		if(vs->pending_out_req)
			fuse_reply_err(vs->pending_out_req, EINTR);
		if(vs->pending_out_buf)
			free(vs->pending_out_buf);
		PG_Remove(&vs->node);
		free(vs);
        }

	vsp_watch_stop_all(vsp);
	vsp_close_endpoints(vsp);

	if(vsp->se)
		fuse_session_destroy(vsp->se);

	free(vsp->chan_buf);
	//	free(vsp->ptt_on_msg);
	//	free(vsp->ptt_off_msg);

	info("vsp connector %s closed", vsp->devname);
	free(vsp->devname);
	free(vsp);

	return;
}
