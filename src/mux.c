/*
 *  mhux - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#include <stdlib.h>
#include "mux.h"
#include "util.h"

#define MAX_SEQUENCE_SIZE 5*4

struct mx *mx_create() {
	struct mx *mx = w_calloc(1, sizeof(*mx));
	return mx;
}

void mx_destroy(struct mx *mx) {
	if(mx)
		free(mx);
}

int mx_mux(struct mx *mx, struct buffer *bout) {
	unsigned char seq[MAX_SEQUENCE_SIZE];
	int c, channel, frame_no;
	int size, out_size = 0;

	if(buf_size_avail(bout) < MAX_SEQUENCE_SIZE)
		return 0;

	do {
		size = -1;
		memset(seq, 0, sizeof(seq));

		for(frame_no = 0; frame_no <= 4; frame_no++ ) {

			if(frame_no)
				seq[frame_no*4+0] = (1<<6); // Always set MSB for sync bytes if not 1st frame in sequence.


			seq[frame_no*4+1] = (1<<7); // Always set MSB on payload bytes.
			seq[frame_no*4+2] = (1<<7);
			seq[frame_no*4+3] = (1<<7);

			// R1 channel for all frames
			if(-1 != (c = buf_get_c(mx->ibuf[MH_CHANNEL_R1]))) {
				seq[frame_no*4]     |= (1<<5);      // Validity
				seq[frame_no*4]     |= ((c>>7)<<2); // MSB in sync byte
				seq[frame_no*4+1]   |=  c;          // Payload
				size = (frame_no+1)*4;
			}

			// R2 channel for all frames
			if(-1 != (c = buf_get_c(mx->ibuf[MH_CHANNEL_R2]))) {
				seq[frame_no*4]     |= (1<<4);      // Validity
				seq[frame_no*4]     |= ((c>>7)<<1); // MSB in sync byte
				seq[frame_no*4+2]   |=  c;          // Payload
				size = (frame_no+1)*4;
			}

			switch(frame_no) {
			case 0:
				channel = MH_CHANNEL_FLAGS;
				break;
			case 1:
				channel = MH_CHANNEL_CONTROL;
				break;
			case 2:
				channel = MH_CHANNEL_WINKEY;
				break;
			case 3:
				channel = MH_CHANNEL_R1_FSK;
				break;
			case 4:
				channel = MH_CHANNEL_R2_FSK;
				break;
			}

			// Shared channel for this frame

			struct buffer *b = mx->ibuf[channel];

			if(-1 != (c = buf_get_c(b))) {
				// Set validity bit only if this is not first or last byte of a command sequence.
				if(channel != MH_CHANNEL_CONTROL || (b->rpos > 1 && b->rpos < b->size))
					seq[frame_no*4]     |= (1<<3);  // Validity

				seq[frame_no*4]     |= ((c>>7)<<0); // MSB in sync byte
				seq[frame_no*4+3]   |=  c;          // Payload
				size = (frame_no+1)*4;
			}
		}

		if(size != -1) {
			buf_append(bout, seq, size);
			out_size += size;
		}

	} while (buf_size_avail(bout) >= MAX_SEQUENCE_SIZE && size != -1);

	return out_size;
}
