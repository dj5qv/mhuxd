#include <string.h>
#include "channel.h"
#include "util.h"
#include "mhinfo.h"

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
{ "ROTATOR", CH_ROTATOR }
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


// Usage discouraged, it always returns R2 for shared R2/AUX channel. 
// Use ch_channel2str_new() when type is known.
const char *ch_channel2str(int channel) {
	unsigned i;
	for(i = 0; i < ARRAY_SIZE(channel_map); i++)
		if(channel_map[i].channel == channel)
			return channel_map[i].channel_str;
	return "Unknown";
}

// Support the differentiation between AUX and R2 which use the same channel,
// based on keyer type.
const char *ch_channel2str_new(int channel, const struct mh_info *mhi)
 {
	unsigned i;
	const char *channel_str = "UNKNOWN";

	// Edge case for shared AUX/R2 channel
	if(channel == MH_CHANNEL_R2) {
		if(mhi->flags & MHF_HAS_R2) {
			channel_str = "R2";
		} else {
			channel_str = "AUX";
		}
		return channel_str;
	}

	for(i = 0; i < ARRAY_SIZE(channel_map); i++)
		if(channel_map[i].channel == channel) {
			channel_str = channel_map[i].channel_str;
			break;
		}
	
	return channel_str;
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
