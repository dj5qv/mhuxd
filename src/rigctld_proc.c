/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#include <ev.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "rigctld_proc.h"
#include "app_ctx.h"
#include "mhcontrol.h"
#include "eventbus.h"
#include "events.h"
#include "util.h"
#include "logger.h"

#define MOD_ID "rigpr"

#define RIGPR_RESPAWN_INITIAL_MS (100)
#define RIGPR_RESPAWN_MAX_MS     (60000)
/* A rigctld that ran this long before it exited is respawned without backoff. */
#ifndef RIGPR_STABLE_S
#define RIGPR_STABLE_S           (10.)
#endif

struct rigctld_proc {
	struct ev_loop *loop;
	char *serial;
	uint8_t radio;
	int port;
	char *options;
	int running;                 /* keyer online, rigctld wanted */
	pid_t child_pid;
	ev_tstamp child_start;       /* ev_now() at spawn */
	ev_child child_watcher;
	ev_timer respawn_timer;
	int respawn_fails;           /* consecutive child exits (log display only) */
	int respawn_delay_ms;        /* current backoff delay, doubled on each exit */
	int exec_errno;              /* errno of last failed rigctld exec, 0 if last exec succeeded */
	int child_log_fd;
	ev_io child_log_w;
	char child_log_buf[512];
	size_t child_log_len;
	eventbus_sub_t *kscb;
};

static const char *radio_str(uint8_t radio) {
	return radio == 2 ? "r2" : "r1";
}

static void rigpr_log_child_line(struct rigctld_proc *proc) {
	proc->child_log_buf[proc->child_log_len] = '\0';
	dbg0("%s rigctld[%s]: %s", proc->serial, radio_str(proc->radio), proc->child_log_buf);
	proc->child_log_len = 0;
}

static void rigpr_feed_child_log(struct rigctld_proc *proc, const char *buf, size_t len) {
	size_t i;

	if(!proc || !buf || !len)
		return;

	for(i = 0; i < len; i++) {
		unsigned char ch = (unsigned char)buf[i];

		if(ch == '\r')
			continue;

		if(ch == '\n') {
			rigpr_log_child_line(proc);
			continue;
		}

		if(proc->child_log_len + 1 >= sizeof(proc->child_log_buf))
			rigpr_log_child_line(proc);

		proc->child_log_buf[proc->child_log_len++] = (char)ch;
	}
}

static void rigpr_drain_child_log(struct rigctld_proc *proc) {
	char buf[256];

	if(!proc || proc->child_log_fd < 0)
		return;

	while(1) {
		ssize_t r = 0;
		int errsv = 0;
		enum mhuxd_io_rw_result io_res = io_read_nonblock(proc->child_log_fd, buf, sizeof(buf), &r, &errsv);

		if(io_res == MHUXD_IO_RW_PROGRESS) {
			rigpr_feed_child_log(proc, buf, (size_t)r);
			continue;
		}

		if(io_res == MHUXD_IO_RW_ERROR)
			dbg0("%s rigctld log read failed (%s) err=%d", proc->serial, radio_str(proc->radio), errsv);

		break;
	}
}

static void rigpr_close_child_log(struct rigctld_proc *proc, int flush_partial) {
	if(!proc)
		return;

	rigpr_drain_child_log(proc);

	if(proc->child_log_fd != -1) {
		ev_io_stop(proc->loop, &proc->child_log_w);
		fd_close(&proc->child_log_fd);
	}

	if(flush_partial && proc->child_log_len > 0)
		rigpr_log_child_line(proc);
	else if(!flush_partial)
		proc->child_log_len = 0;
}

static void rigpr_child_log_cb(struct ev_loop *loop, ev_io *w, int revents) {
	(void)loop;
	(void)revents;
	struct rigctld_proc *proc = w->data;

	if(!proc)
		return;

	rigpr_drain_child_log(proc);

	if(proc->child_pid == 0)
		rigpr_close_child_log(proc, 1);
}

static int rigpr_make_child_log_pipe(int *read_fd, int *write_fd) {
	int pipefd[2] = { -1, -1 };
	int flags;

	if(!read_fd || !write_fd) {
		errno = EINVAL;
		return -1;
	}

	if(pipe(pipefd) != 0)
		return -1;

	flags = fcntl(pipefd[0], F_GETFL, 0);
	if(flags < 0 || fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK) < 0)
		goto fail;

	flags = fcntl(pipefd[0], F_GETFD, 0);
	if(flags < 0 || fcntl(pipefd[0], F_SETFD, flags | FD_CLOEXEC) < 0)
		goto fail;

	flags = fcntl(pipefd[1], F_GETFD, 0);
	if(flags < 0 || fcntl(pipefd[1], F_SETFD, flags | FD_CLOEXEC) < 0)
		goto fail;

	*read_fd = pipefd[0];
	*write_fd = pipefd[1];
	return 0;

fail:
	fd_close(&pipefd[0]);
	fd_close(&pipefd[1]);
	return -1;
}

/* Respawn after the current backoff delay, then double the delay up to RIGPR_RESPAWN_MAX_MS. */
static void rigpr_schedule_respawn(struct rigctld_proc *proc) {
	dbg0("%s rigctld gone, will respawn in %d ms (attempt %d)",
	     proc->serial, proc->respawn_delay_ms, proc->respawn_fails + 1);
	ev_timer_stop(proc->loop, &proc->respawn_timer);
	ev_timer_set(&proc->respawn_timer, (ev_tstamp)proc->respawn_delay_ms / 1000.0, 0.);
	ev_timer_start(proc->loop, &proc->respawn_timer);
	proc->respawn_fails++;
	proc->respawn_delay_ms <<= 1;
	if(proc->respawn_delay_ms > RIGPR_RESPAWN_MAX_MS)
		proc->respawn_delay_ms = RIGPR_RESPAWN_MAX_MS;
}

static void rigpr_child_cb(struct ev_loop *loop, ev_child *w, int revents) {
	(void)revents;
	struct rigctld_proc *proc = w->data;
	ev_child_stop(loop, w);

	if(proc->exec_errno) {
		/* already reported by rigpr_spawn() */
		dbg0("%s rigctld pid=%d exited after exec failure", proc->serial, w->rpid);
	} else if(WIFEXITED(w->rstatus)) {
		warn("%s auto-started rigctld pid=%d exited code=%d",
		     proc->serial, w->rpid, WEXITSTATUS(w->rstatus));
	} else if(WIFSIGNALED(w->rstatus)) {
		warn("%s auto-started rigctld pid=%d killed by signal %d",
		     proc->serial, w->rpid, WTERMSIG(w->rstatus));
	} else {
		warn("%s auto-started rigctld pid=%d exited status=%d",
		     proc->serial, w->rpid, w->rstatus);
	}

	proc->child_pid = 0;
	rigpr_close_child_log(proc, 1);

	if(!proc->running)
		return;

	/* It ran fine for a while, so the next crash starts the backoff fresh. */
	if(!proc->exec_errno && ev_now(proc->loop) - proc->child_start >= RIGPR_STABLE_S) {
		proc->respawn_fails = 0;
		proc->respawn_delay_ms = RIGPR_RESPAWN_INITIAL_MS;
	}
	rigpr_schedule_respawn(proc);
}

/* Child side of fork(): pass errno to the parent through the CLOEXEC pipe and exit. */
static void rigpr_child_report_exec_failure(int fd, int errsv) {
	ssize_t r;
	do {
		r = write(fd, &errsv, sizeof(errsv));
	} while(r < 0 && errno == EINTR);
	_exit(127);
}

static int rigpr_spawn(struct rigctld_proc *proc) {
	dbg0("%s rigctld spawn requested, child_pid: %d", proc->serial, proc->child_pid);

	if(proc->child_pid > 0)
		return 0;

	char port_str[16];
	snprintf(port_str, sizeof(port_str), "%d", proc->port);

	/* Tokenize options string into argv (simple whitespace split, no shell expansion) */
	char *args_buf = NULL;
	int child_log_rd = -1;
	int child_log_wr = -1;
	int exec_err_pipe[2] = { -1, -1 };
	char *argv_ptrs[68]; /* "rigctld" + up to 64 opts + "-t" + port + NULL */
	int argc = 0;
	argv_ptrs[argc++] = "rigctld";
	if(proc->options && *proc->options) {
		args_buf = strdup(proc->options);
		if(args_buf) {
			char *saveptr = NULL;
			char *tok = strtok_r(args_buf, " \t", &saveptr);
			while(tok && argc < 65) {
				argv_ptrs[argc++] = tok;
				tok = strtok_r(NULL, " \t", &saveptr);
			}
		}
	}
	argv_ptrs[argc++] = "-t";
	argv_ptrs[argc++] = port_str;
	argv_ptrs[argc] = NULL;

	dbg0("%s auto-starting rigctld (port=%d options: %s)",
	     proc->serial, proc->port, proc->options ? proc->options : "");

	if(rigpr_make_child_log_pipe(&child_log_rd, &child_log_wr) != 0) {
		dbg0("%s rigctld auto-start pipe failed: err=%d", proc->serial, errno);
		free(args_buf);
		return -1;
	}

	/* A successful execvp() closes the write end (EOF for the parent),
	 * otherwise the child writes errno into it. */
	if(pipe2(exec_err_pipe, O_CLOEXEC) != 0) {
		dbg0("%s rigctld auto-start exec error pipe failed: err=%d", proc->serial, errno);
		fd_close(&child_log_rd);
		fd_close(&child_log_wr);
		free(args_buf);
		return -1;
	}

	rigpr_close_child_log(proc, 0);

	pid_t mhuxd_pid = getpid();
	pid_t pid = fork();
	if(pid < 0) {
		dbg0("%s rigctld auto-start fork failed: err=%d", proc->serial, errno);
		fd_close(&child_log_rd);
		fd_close(&child_log_wr);
		fd_close(&exec_err_pipe[0]);
		fd_close(&exec_err_pipe[1]);
		free(args_buf);
		return -1;
	}
	if(pid == 0) {
		fd_close(&child_log_rd);
		fd_close(&exec_err_pipe[0]);
		if(dup2(child_log_wr, STDOUT_FILENO) < 0)
			rigpr_child_report_exec_failure(exec_err_pipe[1], errno);
		if(dup2(child_log_wr, STDERR_FILENO) < 0)
			rigpr_child_report_exec_failure(exec_err_pipe[1], errno);
		if(child_log_wr > STDERR_FILENO)
			close(child_log_wr);

		/* child: ask the kernel to send SIGKILL when the parent (mhuxd) dies,
		 * regardless of how it dies (including SIGKILL).  This prevents rigctld
		 * from outliving mhuxd and holding the VSP device open. */
		prctl(PR_SET_PDEATHSIG, SIGKILL);
		/* Guard against the race where mhuxd died between fork() and prctl():
		 * if the parent already changed, kill ourselves now. */
		if(getppid() != mhuxd_pid)
			_exit(0);
		execvp("rigctld", argv_ptrs);
		rigpr_child_report_exec_failure(exec_err_pipe[1], errno);
	}
	/* parent */
	fd_close(&child_log_wr);
	fd_close(&exec_err_pipe[1]);
	free(args_buf);

	/* Blocks only until the child reaches execvp(): EOF on success, errno on failure. */
	int exec_errno = 0;
	ssize_t r;
	do {
		r = read(exec_err_pipe[0], &exec_errno, sizeof(exec_errno));
	} while(r < 0 && errno == EINTR);
	fd_close(&exec_err_pipe[0]);
	if(r != (ssize_t)sizeof(exec_errno))
		exec_errno = 0;

	if(exec_errno) {
		/* Report as error once, repeats during respawn backoff at debug level only */
		if(exec_errno != proc->exec_errno) {
			const char *path = getenv("PATH");
			if(exec_errno == ENOENT)
				err("%s cannot auto-start rigctld: executable not found in PATH=%s",
				    proc->serial, path ? path : "<unset>");
			else
				err("%s cannot auto-start rigctld: %s", proc->serial, strerror(exec_errno));
		} else {
			dbg0("%s cannot auto-start rigctld: %s", proc->serial, strerror(exec_errno));
		}
	}
	proc->exec_errno = exec_errno;

	proc->child_pid = pid;
	proc->child_start = ev_now(proc->loop);
	proc->child_log_fd = child_log_rd;
	proc->child_log_len = 0;
	ev_io_set(&proc->child_log_w, proc->child_log_fd, EV_READ);
	ev_io_start(proc->loop, &proc->child_log_w);
	ev_child_init(&proc->child_watcher, rigpr_child_cb, pid, 0);
	proc->child_watcher.data = proc;
	// libev child watcher works in default loop only
	ev_child_start(EV_DEFAULT_ &proc->child_watcher);
	if(!exec_errno)
		dbg0("%s rigctld auto-started pid=%d", proc->serial, (int)proc->child_pid);
	return 0;
}

static void rigpr_respawn_cb(struct ev_loop *loop, ev_timer *w, int revents) {
	(void)loop;
	(void)revents;
	struct rigctld_proc *proc = w->data;

	if(!proc->running)
		return;
	dbg0("%s rigctld gone, respawning", proc->serial);
	if(rigpr_spawn(proc))
		rigpr_schedule_respawn(proc);
}

static void rigpr_kill(struct rigctld_proc *proc) {
	if(proc->child_pid <= 0)
		return;
	dbg0("%s stopping auto-started rigctld pid=%d", proc->serial, (int)proc->child_pid);
	ev_child_stop(EV_DEFAULT_ &proc->child_watcher);
	kill(proc->child_pid, SIGTERM);
	proc->child_pid = 0;
	/* libev's SIGCHLD handler will reap the child when it exits */
}

/* Called at destroy time (outside the ev loop) to reap the child.
 * PR_SET_PDEATHSIG ensures rigctld is killed if mhuxd is killed externally,
 * but on a graceful shutdown the parent is still alive so we must drive the
 * kill ourselves.  Give SIGTERM ~1 s to take effect, then escalate to SIGKILL. */
static void rigpr_ensure_dead(const char *serial, pid_t pid) {
	int i;
	for(i = 0; i < 5; i++) {
		int r = waitpid(pid, NULL, WNOHANG);
		if(r == pid || (r < 0 && errno == ECHILD))
			return; /* already exited */
		usleep(100 * 1000); /* 100 ms */
	}
	dbg0("%s rigctld pid=%d did not exit after SIGTERM; sending SIGKILL", serial, (int)pid);
	kill(pid, SIGKILL);
	/* Do NOT block-wait here: rigctld may be in D state (uninterruptible sleep)
	 * waiting on mhuxd's own CUSE fds.  Blocking would deadlock — mhuxd waits
	 * for rigctld, rigctld waits for mhuxd's fuse loop.  Once mhuxd finishes
	 * shutdown and closes the CUSE fds, rigctld unblocks, dies, and is reaped
	 * by init. */
}

static void rigpr_start(struct rigctld_proc *proc) {
	if(proc->running)
		return;

	proc->running = 1;
	proc->respawn_fails = 0;
	proc->respawn_delay_ms = RIGPR_RESPAWN_INITIAL_MS;
	proc->exec_errno = 0;
	info("%s starting rigctld (%s port=%d options: %s)", proc->serial, radio_str(proc->radio),
	     proc->port, proc->options ? proc->options : "");
	if(rigpr_spawn(proc))
		rigpr_schedule_respawn(proc);
}

static void rigpr_stop(struct rigctld_proc *proc) {
	if(!proc->running)
		return;

	proc->running = 0;
	ev_timer_stop(proc->loop, &proc->respawn_timer);
	rigpr_kill(proc);
}

static void rigpr_keyer_state_changed_cb(enum app_event_type type, const void *data, void *user_data) {
	struct rigctld_proc *proc = user_data;
	const struct ev_keyer_state *ev = data;

	if(!proc || type != EV_KEYER_STATE || !ev)
		return;

	if(ev->serial && strcmp(ev->serial, proc->serial))
		return;

	if(ev->state == MHC_KEYER_STATE_ONLINE) {
		rigpr_start(proc);
		return;
	}

	rigpr_stop(proc);
}

struct rigctld_proc *rigctld_proc_create(struct app_ctx *ctx, struct mh_control *ctl,
					 const struct rigctld_proc_cfg *cfg) {
	if(!ctx || !ctl || !cfg || !cfg->serial || !cfg->serial[0])
		return NULL;

	struct rigctld_proc *proc = w_calloc(1, sizeof(*proc));
	proc->loop = app_ctx_get_loop(ctx);
	proc->serial = w_strdup(cfg->serial);
	proc->radio = cfg->radio == 2 ? 2 : 1;
	proc->port = cfg->port > 0 ? cfg->port : (proc->radio == 2 ? 4533 : 4532);
	proc->options = (cfg->options && *cfg->options) ? w_strdup(cfg->options) : NULL;
	proc->child_log_fd = -1;
	proc->respawn_delay_ms = RIGPR_RESPAWN_INITIAL_MS;

	ev_timer_init(&proc->respawn_timer, rigpr_respawn_cb, 0., 0.);
	proc->respawn_timer.data = proc;
	ev_io_init(&proc->child_log_w, rigpr_child_log_cb, -1, EV_READ);
	proc->child_log_w.data = proc;

	proc->kscb = eventbus_subscribe(app_ctx_get_eventbus(ctx), EV_KEYER_STATE, rigpr_keyer_state_changed_cb, proc);
	if(mhc_is_online(ctl))
		rigpr_start(proc);

	dbg0("%s rigctld auto-start created (%s port=%d)", proc->serial, radio_str(proc->radio), proc->port);

	return proc;
}

void rigctld_proc_destroy(struct rigctld_proc *proc) {
	if(!proc)
		return;

	/* Save pid before rigpr_stop() → rigpr_kill() zeros it.
	 * We need it below to guarantee the process is dead before we exit,
	 * since the CUSE virtual serial ports disappear with mhuxd. */
	pid_t child_pid = proc->child_pid;

	rigpr_stop(proc);
	if(proc->kscb)
		eventbus_unsubscribe(proc->kscb);

	if(child_pid > 0)
		rigpr_ensure_dead(proc->serial, child_pid);
	rigpr_close_child_log(proc, 1);

	dbg0("%s rigctld auto-start destroyed (%s)", proc->serial, radio_str(proc->radio));

	free(proc->serial);
	free(proc->options);
	free(proc);
}
