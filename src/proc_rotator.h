#ifndef PROC_ROTATOR_H
#define PROC_ROTATOR_H

struct buffer;
struct mh_router;
struct mh_control;
struct proc_rotator;

struct proc_rotator *rot_create(struct mh_control *ctl);
void rot_destroy(struct proc_rotator *rot);
void rot_cb(struct mh_router *router, int channel, struct buffer *b, int fd, void *user_data);

#endif // PROC_ROTATOR_H
