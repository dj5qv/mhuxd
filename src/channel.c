#include <string.h>
#include "channel.h"
#include "util.h"

struct channel_map {
	const char *channel_str;
	int channel;
};


struct channel_map channel_map[]= {
	{ "CTL", MH_CHANNEL_CONTROL },
	{ "FLAGS", MH_CHANNEL_FLAGS },
	{ "R1", MH_CHANNEL_R1 },
	{ "R2", MH_CHANNEL_R2 },
	{ "CAT1", MH_CHANNEL_R1 },
	{ "CAT2", MH_CHANNEL_R2 },
	{ "AUX", MH_CHANNEL_R2 },
	{ "FSK1", MH_CHANNEL_R1_FSK },
	{ "FSK2", MH_CHANNEL_R2_FSK },
	{ "WK", MH_CHANNEL_WINKEY },
	{ "PTT1", CH_PTT1 },
	{ "PTT2", CH_PTT2 },
	{ "MCP", CH_MCP },
};

int ch_ptt_channel(int channel) {
	int ptt_channel;
	switch(channel) {
	case MH_CHANNEL_R1:
	case MH_CHANNEL_R1_FSK:
		ptt_channel = CH_PTT1;
		break;
	case MH_CHANNEL_R2:
	case MH_CHANNEL_R2_FSK:
		ptt_channel = CH_PTT2;
		break;
	case CH_PTT1:
	case CH_PTT2:
		ptt_channel = channel;
		break;
	default:
		ptt_channel = -1;
		break;
	}
	return ptt_channel;
}


const char *ch_channel2str(int channel) {
	unsigned i;
	for(i = 0; i < ARRAY_SIZE(channel_map); i++)
		if(channel_map[i].channel == channel)
			return channel_map[i].channel_str;
	return "Unknown";
}

int ch_str2channel(const char *str) {
	unsigned i;
	for(i = 0; i < ARRAY_SIZE(channel_map); i++) {
		if(!strcasecmp(channel_map[i].channel_str, str)) {
			return channel_map[i].channel;
		}
	}
	return -1;
}
