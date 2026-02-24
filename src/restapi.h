/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2026
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#ifndef RESTAPI_H
#define RESTAPI_H 1

struct http_server;
struct cfgmgrj;

struct restapi;

struct restapi *restapi_create(struct http_server *hs, struct cfgmgrj *cfgmgrj);
void restapi_destroy(struct restapi *api);
void restapi_shutdown(struct restapi *api);

#endif /* RESTAPI_H */