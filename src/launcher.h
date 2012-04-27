/*
 *  mhux - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#ifndef LAUNCHER_H
#define LAUNCHER_H

struct launcher;

struct launcher * launch_start_all();
void launch_stop_all(struct launcher *lc);
void launch_loop(struct launcher *lc);

#endif // LAUNCHER_H
