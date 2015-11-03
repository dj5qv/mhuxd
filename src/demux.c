/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2015  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#include <stdlib.h>
#include "demux.h"
#include "util.h"
#include "logger.h"
#include "channel.h"

#define MOD_ID "demux"

struct dmx *dmx_create() {
	struct dmx *dmx = w_calloc(1, sizeof(*dmx));
	return dmx;
}

void dmx_destroy(struct dmx *dmx) {
	if(dmx)
	free(dmx);
}

int dmx_demux(struct dmx *dmx, unsigned char c) {
	int rv = -1;

	if((c & (1<<7)) == 0) {
		if((c & (1<<6)) == 0)
			dmx->frame_no = 0;
		else
			dmx->frame_no++;
		dmx->byte_no = 0;
		dmx->sync_byte = c;
		return -1;
	}

	dmx->byte_no++;

	if(dmx->sync_byte == -1)
		return -1;

	if(dmx->frame_no > 3) {
		warn("DMX Sequence too long (%d frames)!", dmx->frame_no);
		dmx->sync_byte = -1;
		return -1;
	}

	switch(dmx->byte_no) {
	case 1:
		if(dmx->sync_byte & (1<<5)) {
			dmx->result_byte = (c & 0x7f) | (((dmx->sync_byte >> 2) & 1) << 7);
			rv = MH_CHANNEL_R1;
		}
		break;

	case 2:
		if(dmx->sync_byte & (1<<4)) {
			dmx->result_byte = (c & 0x7f) | (((dmx->sync_byte >> 1) & 1) << 7);
			rv = MH_CHANNEL_R2;
		}
		break;

	case 3:
		if(dmx->frame_no != 1) {
			if((dmx->sync_byte >> 3) & 1) {
				dmx->result_byte = (c & 0x7f) | ((dmx->sync_byte & 1) << 7);
				rv = dmx->frame_no; /* FLAGS, WINKEY or PS2 */
			}
			break;
		}

		/* Control channel, different symantic of validity flags. */

		unsigned char byte = (c & 0x7f) | ((dmx->sync_byte & 1) << 7);

		if(!(dmx->sync_byte & (1<<3))) {
			if(!(byte & (1<<7))) {
				/* first command byte. */
				dmx->cmd = byte;
				dmx->cmd_length = 0;
			} else {
				/* last command byte */
				if(byte == (dmx->cmd| (1<<7)))
					rv = MH_CHANNEL_CONTROL;
				else
					warn("DMX Command mismatch: %x / %x", dmx->cmd, byte);
			}
		}

		if(dmx->cmd != -1) {
			if(dmx->cmd_length == sizeof(dmx->cmd_buffer)) {
				warn("DMX Command buffer full!");
				return -1;
			}
			dmx->cmd_buffer[dmx->cmd_length++] = byte;

			if(rv == MH_CHANNEL_CONTROL)
				dmx->cmd = -1;
		}

		break;
	}

	return rv;
}
