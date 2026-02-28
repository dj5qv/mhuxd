#ifndef CHANNEL_H
#define CHANNEL_H

// microHam channels
enum {
	MH_CHANNEL_FLAGS   = 0, /* 0-3 must match byte position in keyer frame. */
	MH_CHANNEL_CONTROL = 1,
	MH_CHANNEL_WINKEY  = 2,
	MH_CHANNEL_PS2     = 3,
	MH_CHANNEL_R1      = 4,
	MH_CHANNEL_R2      = 5,
	MH_CHANNEL_R1_FSK  = 6,
	MH_CHANNEL_R2_FSK  = 7,
	MH_NUM_CHANNELS    = 8,

	CH_PTT1 = 9,		// routed to radio 1 via flags channel
	CH_PTT2 = 10,		// routed to radio 2 via flags channel
	CH_PTT_FOCUS = 11,	// routed to focused radio via flags channel (MK2R), otherwise like CH_PTT1
	CH_MCP = 12, 
	CH_ROTATOR = 13,
	ALL_NUM_CHANNELS
};

struct mh_info;

const char *ch_channel2str(int channel);
const char *ch_channel2str_new(int channel, const struct mh_info *mhi);
int ch_str2channel(const char *str);
int ch_ptt_channel(int channel);

#endif /* CHANNEL_H */
