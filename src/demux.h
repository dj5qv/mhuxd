/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2013  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#ifndef MHDMX_H
#define MHDMX_H

#define MHDMX_CMD_BUFFER_SIZE 4096

struct byte_buffer;

struct	dmx {
	int	frame_no;
	int	byte_no;
	int	sync_byte;
	unsigned char	result_byte;
	unsigned char	cmd_buffer[MHDMX_CMD_BUFFER_SIZE];
	int	cmd_length;
	int	cmd;
};

struct dmx *dmx_create();
void dmx_destroy(struct dmx *dmx);
int dmx_demux(struct dmx *dmx, unsigned char c);


#endif // MHDMX_H
