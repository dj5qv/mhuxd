#ifndef CHANNEL_H
#define CHANNEL_H

// microHam channels
enum {
	MH_CHANNEL_FLAGS   = 0, /* 1-3 must match byte position in keyer frame. */
	MH_CHANNEL_CONTROL = 1,
	MH_CHANNEL_WINKEY  = 2,
	MH_CHANNEL_PS2     = 3,
	MH_CHANNEL_R1      = 4,
	MH_CHANNEL_R2      = 5,
	MH_CHANNEL_R1_FSK  = 6,
	MH_CHANNEL_R2_FSK  = 7,
	MH_NUM_CHANNELS    = 8,

	CH_PTT1 = 9,
	CH_PTT2 = 10,
	CH_MCP = 11, 
	ALL_NUM_CHANNELS
};



const char *ch_channel2str(int channel);
int ch_str2channel(const char *str);
int ch_ptt_channel(int channel);

#endif /* CHANNEL_H */
