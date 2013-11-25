/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2013  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

struct http_request *httpc_get_request(const char *url);

#endif // HTTP_CLIENT_H
