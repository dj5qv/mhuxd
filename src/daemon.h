#ifndef DAEMON_H
#define DAEMON_H

struct cfg;

void dmn_daemonize(void);
int16_t dmn_set(struct cfg *cfg);

FILE *dmn_pidfile_lock(const char *name);
void dmn_pidfile_unlock(FILE *f, const char *name);

#endif // DAEMON_H
