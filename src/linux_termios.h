/*
 *  mhux - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#ifndef LINUX_TERMIOS_H
#define LINUX_TERMIOS_H


/* We need to use the kernel version of struct termios. Glibc has a larger version and
 * does a translation in calls as tcgetattr().
 */

#include <asm/termios.h>

int termios_baud_rate(struct termios *ts);

#endif // LINUX_TERMIOS_H
