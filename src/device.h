#ifndef DEVICE_H
#define DEVICE_H

#include "pglist.h"
#include "mhinfo.h"

struct mh_router;
struct mh_control;
struct wkman;

struct device {
	struct PGNode node;
	struct mh_router *router;
	struct mh_control *ctl;
	struct wkman *wkman;
	char *serial;
};


#endif // DEVICE_H
