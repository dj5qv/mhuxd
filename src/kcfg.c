/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2013  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#include <stdint.h>
#include <string.h>
#include "kcfg.h"
#include "util.h"
#include "buffer.h"
#include "mhinfo.h"
#include "logger.h"

#define AUDIO_SW_D (0)
#define AUDIO_SW_B (1)
#define AUDIO_SW_A (2)
#define AUDIO_SW_C (3)

struct citem {
	const char	*key;
	uint8_t		off;
	uint8_t		base_bit;
	uint8_t		width;
	uint32_t	def;
};

static const struct citem c_base[] = {
	{ "r1FrBase_Digital.audioRx",	0,  1, 2, 1 }, /* MK2, DK2, MK2R */ /* B */
	{ "r1FrBase_Digital.audioTx",	0,  3, 2, 1 }, /* MK2, DK2, MK2R */ /* B */
	{ "r1FrBase_Digital.audioTxFootSw",	0,  5, 2, 1 }, /* MK2, DK2, MK2R */ /* B */
	{ "r1FrBase_Digital.ptt1",	0,  6, 1, 1 }, /* MK2, DK2, MK2R */
	{ "r1FrBase_Digital.ptt2",	0,  7, 1, 1 }, /* MK2, DK2, MK2R */

	{ "r1DelaySerialPtt",	1,  0, 1,  1 },
	{ "fskDiddle",		1,  1, 1,  1 },
	{ "fskUos",		1,  2, 1,  1 },
	{ "restorePtt",		1,  3, 1,  1 },
	{ "restoreCompCw",	1,  4, 1,  1 },
	{ "restoreCompFsk",	1,  5, 1,  1 },
	{ "r1LnaPtt",		1,  6, 1,  1 },
	{ "r1InvertFsk",	1,  7, 1,  0 },

	{ "r1PaPtt",		2,  0, 1,  1 },
	{ "muteCompCw",		2,  1, 1,  0 },
	{ "muteCompFsk",	2,  2, 1,  0 },
	{ "r1I2cCoupled",	2,  7, 1,  0 },

	{ "r1PttDelay",		3,  7, 8,  1 },

	{ "r1InvertLnaPtt",	4,  0, 1,  0 },
	{ "r1ImmediateCw",	4,  1, 1,  0 },
	{ "r1TrDelay",		4,  7, 6,  0 },

	{ "sideTone",		5,  2, 3,  0 },
	{ "speedStep",		5,  7, 4,  2 },

	{ "r1ForceKeyerMode",	6,  0, 1,  1 },
	{ "fskTypeAhead",	6,  1, 1,  0 },
	{ "cwTypeAhead",	6,  2, 1,  0 },
	{ "r1UseLnaPttTail",	6,  3, 1,  1 },
	{ "r1FollowTxMode",	6,  4, 1,  1 },
	{ "r1AllowCwInVoice",	6,  5, 1,  0 },
	{ "qwertz",		6,  6, 1,  0 },
	{ "r1Qsk",		6,  7, 1,  0 },

	{ "r1LineOutRight",	7,  0, 1,  0 },
	{ "numLeadZeroT",	7,  1, 1,  0 },
	{ "numZeroT",		7,  2, 1,  0 },
	{ "numOneA",		7,  3, 1,  0 },
	{ "numNineN",		7,  4, 1,  0 },
	{ "r1KeyerMode",	7,  6, 2,  0 },
	{ "numReport5NN",	7,  7, 1,  0 },

	{ "r1PaPttTail",	8,  7, 8,  1 },
	{ "r1LnaPttTail",	9,  7, 8,  1 },
#if 0
	{ "r1FrBase_Cw",	10, 7, 8,  0xEA }, /* AAA, PTT1|PTT2 */
#endif
	{ "r1FrBase_Cw.audioRx",	10,  1, 2, 2 }, /* MK2, DK2, MK2R */ /* A */
	{ "r1FrBase_Cw.audioTx",	10,  3, 2, 2 }, /* MK2, DK2, MK2R */ /* A */
	{ "r1FrBase_Cw.audioTxFootSw",	10,  5, 2, 2 }, /* MK2, DK2, MK2R */ /* A */
	{ "r1FrBase_Cw.ptt1",	10,  6, 1, 1 }, /* MK2, DK2, MK2R */
	{ "r1FrBase_Cw.ptt2",	10,  7, 1, 0 }, /* MK2, DK2, MK2R */
#if 0
	{ "r1FrBase_Voice",	11, 7, 8,  0xEA }, /* AAA, PTT1|PTT2 */
#endif
	{ "r1FrBase_Voice.audioRx",	11,  1, 2, 2 }, /* MK2, DK2, MK2R */ /* A */
	{ "r1FrBase_Voice.audioTx",	11,  3, 2, 3 }, /* MK2, DK2, MK2R */ /* C */
	{ "r1FrBase_Voice.audioTxFootSw",	11,  5, 2, 2 }, /* MK2, DK2, MK2R */ /* A */
	{ "r1FrBase_Voice.ptt1",	11,  6, 1, 1 }, /* MK2, MK2R */
	{ "r1FrBase_Voice.ptt2",	11,  7, 1, 0 }, /* MK2, DK2, MK2R */
};

struct citem c_mk1[] = {
	// MK1 doesn't recognize mode specific r1FrBase_ settings.
	// We need to update r1FrBase upon every mode change and mode
	// specific r1FrBase_ change. Handled in mk1_care_frbase().
	{ "r1FrBase.audioRx",	0,  1, 2, 1 },
	{ "r1FrBase.audioTx",	0,  3, 2, 1 },
	{ "r1FrBase.audioTxFootSw",	0,  5, 2, 1 },
	{ "r1FrBase.ptt1",	0,  6, 1, 1 },
	{ "r1FrBase.ptt2",	0,  7, 1, 1 },
};

struct citem c_dk2[] = {
	{ "useAutoPtt",		28, 2, 1,  0 },
	{ "paddleOnlySideTone",	28, 3, 1,  0 },
	{ "downstreamOverFootSw",28, 5, 1,  0 },
	{ "useQCw",             28, 6, 1,  0 },
	{ "usePFsk",            28, 7, 1,  0 },
	/* X30-X39 = keyFuncPs2[0] - keyFuncPs2[9] */
	{ "civAddress",		40, 7, 8,  0x70 }, /* IC-7000 */
	{ "civBaudRate",	41, 7, 8,  0x24 }, /* 9600 */
	{ "civFunc",		42, 7, 8,  0x00 }, /* none */
	/* X44-X55 = keyFuncFh2[0] - keyFuncFh2[11] */
};

struct citem c_mk2[] = {
#if 0
	{ "r1FrMpkExtra_Digital",12, 7, 8,  0 },
#endif
	{ "r1FrMpkExtra_Digital.pwmMonctr",12, 4, 5,  0 },
	{ "r1FrMpkExtra_Digital.reserve",12, 5, 1,  0 },
	{ "r1FrMpkExtra_Digital.onAirRecActive",12, 6, 1,  0 },
	{ "r1FrMpkExtra_Digital.onAirRecControlByRouter",12, 7, 1,  0 },

	{ "pwmContrast",	13, 4, 5, 15 },
	{ "pwmBlight",		14, 4, 5, 25 },
	{ "dispBg0",		15, 7, 8, 0x06 }, /* RX freq */
	{ "dispBg1",		16, 7, 8, 0x05 }, /* WPM / speed pot */

	{ "dispEv.recMsg",      17, 0, 1, 0 },
	{ "dispEv.playMsg",     17, 1, 1, 0 },
	{ "dispEv.txDecoder",   17, 2, 1, 0 },
	{ "dispEv.rxDecoder",   17, 3, 1, 0 },
	{ "dispEv.recDecoder",  17, 4, 1, 0 },
	{ "dispEv.wpmChange",   17, 5, 1, 0 },
	{ "dispEv.steppirCmd",  17, 6, 1, 0 },
	{ "dispEv.rxFreqChange",17, 7, 1, 0 },
	{ "dispEv.txFreqChange",17, 8, 1, 0 },
	{ "dispEv.modeChange",  17, 9, 1, 0 },
	{ "dispEv.micChange",   17, 10, 1, 0 },
	{ "dispEv.steppirLock", 17, 11, 1, 0 },
	{ "dispEv.sysPwr",      17, 12, 1, 1 },
	{ "dispEv.wpmInCwMode", 17, 13, 1, 0 },
	{ "dispEv.cfgOverriden",17, 14, 1, 0 },
	{ "dispEv.wpmSnControlChange",17, 15, 1, 0 },
	{ "dispEv.snChange",    17, 16, 1, 0 },
	{ "dispEv.snInCwMode",  17, 17, 1, 0 },
	{ "dispEv.wpmSnInCwMode",  17, 18, 1, 0 },
	{ "dispEv.operFreqChange", 17, 19, 1, 0 },
	{ "dispEv.vfoAFreqChange", 17, 20, 1, 0 },
	{ "dispEv.vfoBFreqChange", 17, 21, 1, 0 },
	{ "dispEv.steppirState",   17, 22, 1, 0 },
	{ "dispEv.smLock",         17, 23, 1, 0 },
	{ "dispEv.subRxFreqChange",17, 24, 1, 0 },
	{ "dispEv.presetChange",   17, 25, 1, 0 },

	{ "dispEvLn.recMsg",      21, 0, 1, 0 },
	{ "dispEvLn.playMsg",     21, 1, 1, 0 },
	{ "dispEvLn.txDecoder",   21, 2, 1, 0 },
	{ "dispEvLn.rxDecoder",   21, 3, 1, 0 },
	{ "dispEvLn.recDecoder",  21, 4, 1, 0 },
	{ "dispEvLn.wpmChange",   21, 5, 1, 0 },
	{ "dispEvLn.steppirCmd",  21, 6, 1, 0 },
	{ "dispEvLn.rxFreqChange",21, 7, 1, 0 },
	{ "dispEvLn.txFreqChange",21, 8, 1, 0 },
	{ "dispEvLn.modeChange",  21, 9, 1, 0 },
	{ "dispEvLn.micChange",   21, 10, 1, 0 },
	{ "dispEvLn.steppirLock", 21, 11, 1, 0 },
	{ "dispEvLn.sysPwr",      21, 12, 1, 1 },
	{ "dispEvLn.wpmInCwMode", 21, 13, 1, 0 },
	{ "dispEvLn.cfgOverriden",21, 14, 1, 0 },
	{ "dispEvLn.wpmSnControlChange",21, 15, 1, 0 },
	{ "dispEvLn.snChange",    21, 16, 1, 0 },
	{ "dispEvLn.snInCwMode",  21, 17, 1, 0 },
	{ "dispEvLn.wpmSnInCwMode",  21, 18, 1, 0 },
	{ "dispEvLn.operFreqChange", 21, 19, 1, 0 },
	{ "dispEvLn.vfoAFreqChange", 21, 20, 1, 0 },
	{ "dispEvLn.vfoBFreqChange", 21, 21, 1, 0 },
	{ "dispEvLn.steppirState",   21, 22, 1, 0 },
	{ "dispEvLn.smLock",         21, 23, 1, 0 },
	{ "dispEvLn.subRxFreqChange",21, 24, 1, 0 },
	{ "dispEvLn.presetChange",   21, 25, 1, 0 },

	/* off 17 - 24 to be implemented seperately (dispEv / dispEvLn) */
	{ "dispEvTime",		25, 7, 8, 200 },
#if 0
	{ "r1FrMpkExtra_Cw",	26, 7, 8,  0 },
#endif
	{ "r1FrMpkExtra_Cw.pwmMonctr",26, 4, 5,  0 },
	{ "r1FrMpkExtra_Cw.reserve",26, 5, 1,  0 },
	{ "r1FrMpkExtra_Cw.onAirRecActive",26, 6, 1,  0 },
	{ "r1FrMpkExtra_Cw.onAirRecControlByRouter",26, 7, 1,  0 },
#if 0
	{ "r1FrMpkExtra_Voice",	27, 7, 8,  0 },
#endif
	{ "r1FrMpkExtra_Voice.pwmMonctr",27, 4, 5,  0 },
	{ "r1FrMpkExtra_Voice.reserve",27, 5, 1,  0 },
	{ "r1FrMpkExtra_Voice.onAirRecActive",27, 6, 1,  1 },
	{ "r1FrMpkExtra_Voice.onAirRecControlByRouter",27, 7, 1,  1 },


	{ "micSelAuto",		28, 0, 1,  1 },
	{ "micSelFront",	28, 1, 1,  0 },
	{ "useAutoPtt",		28, 2, 1,  0 },
	{ "paddleOnlySideTone",	28, 3, 1,  0 },
	{ "enableSleepMode",	28, 4, 1,  0 },
	{ "downstreamOverFootSw",28, 5, 1,  0 },
	{ "extSerBaudRate",	29, 7, 8,  24 }, /* 9600 */
	/* off 30 - 39 keyFuncPs[0-9] to be implemented seperately. */

	{ "civAddress",		40, 7, 8,  0x70 }, /* IC-7000 */
	{ "civBaudRate",	41, 7, 8,  0x24 }, /* 9600 */
	{ "civFunc",		42, 7, 8,  0x00 }, /* none */
	/* 43 reserve */
	/* X44-X55 = keyFuncFh2[0] - keyFuncFh2[11], to be implemented seperately */
};

struct citem c_mk2r[] = {
#if 0
	{ "r1FrMokExtra_Digital", 12, 7, 8, 0 },
#endif
	{ "r1FrMokExtra_Digital.audioRxMic", 12, 0, 1, 0 },
	{ "r1FrMokExtra_Digital.reserve1", 12, 1, 1, 0 },
	{ "r1FrMokExtra_Digital.audioTxMic", 12, 2, 1, 0 },
	{ "r1FrMokExtra_Digital.reserve2", 12, 3, 1, 0 },
	{ "r1FrMokExtra_Digital.audioTxFootSwMic", 12, 4, 1, 0 },
	{ "r1FrMokExtra_Digital.reserve3", 12, 5, 1, 0 },
	{ "r1FrMokExtra_Digital.voiceCodec", 12, 6, 1, 0 },
	{ "r1FrMokExtra_Digital.reserve4", 12, 7, 1, 0 },


#if 0
	{ "r2FrBase_Digital",     13, 7, 8, 0xEA },
#endif
	{ "r2FrBase_Digital.audioRx",	13,  1, 2, 1 }, /* MK2, DK2, MK2R */ /* B */
	{ "r2FrBase_Digital.audioTx",	13,  3, 2, 1 }, /* MK2, DK2, MK2R */ /* B */
	{ "r2FrBase_Digital.audioTxFootSw",	13,  5, 2, 1 }, /* MK2, DK2, MK2R */ /* B */
	{ "r2FrBase_Digital.ptt1",	13,  6, 1, 1 }, /* MK2, DK2, MK2R */
	{ "r2FrBase_Digital.ptt2",	13,  7, 1, 1 }, /* MK2, DK2, MK2R */
#if 0
	{ "r2FrMokExtra_Digital", 14, 7, 8, 0 },
#endif
	{ "r2FrMokExtra_Digital.audioRxMic", 14, 0, 1, 0 },
	{ "r2FrMokExtra_Digital.reserve1", 14, 1, 1, 0 },
	{ "r2FrMokExtra_Digital.audioTxMic", 14, 2, 1, 0 },
	{ "r2FrMokExtra_Digital.reserve2", 14, 3, 1, 0 },
	{ "r2FrMokExtra_Digital.audioTxFootSwMic", 14, 4, 1, 0 },
	{ "r2FrMokExtra_Digital.reserve3", 14, 5, 1, 0 },
	{ "r2FrMokExtra_Digital.voiceCodec", 14, 6, 1, 0 },
	{ "r2FrMokExtra_Digital.reserve4", 14, 7, 1, 0 },


	{ "r2PaPtt",             15, 0, 1, 1 },
	{ "singleSerialCw",      15, 1, 1, 0 },
	{ "singleSerialPtt",     15, 2, 1, 0 },
	{ "singleFootSwitch",    15, 7, 1, 0 },

	{ "r2DelaySerialPtt",    16, 0, 1, 1 },
	{ "r2I2cCoupled",        16, 1, 1, 0 },
	{ "lastOneWins",         16, 2, 1, 0 },
	{ "dualTx",              16, 3, 1, 0 },
	{ "useLptPtt",           16, 4, 1, 0 },
	{ "useLptCw",            16, 5, 1, 0 },
	{ "r2LnaPtt",            16, 6, 1, 1 },
	{ "r2InvertFsk",         16, 7, 1, 0 },

	{ "r2LineOutRight",      17, 0, 1, 0 },
	{ "lptDvk",              17, 1, 1, 0 },
	{ "disableBandLock",     17, 4, 1, 0 },
	{ "r2KeyerMode",         17, 6, 2, 0 },
	{ "useLptFsIn",          17, 7, 1, 0 },

	{ "r2ForceKeyerMode",    18, 0, 1, 1 },
	{ "singleSerialFsk",     18, 1, 1, 0 },
	{ "useChanFocus",        18, 2, 1, 0 },
	{ "r2UseLnaPttTail",     18, 3, 1, 1 },
	{ "r2FollowTxMode",      18, 4, 1, 1 },
	{ "r2AllowCwInVoice",    18, 5, 1, 0 },
	{ "singleWinkeyCwPtt",   18, 6, 1, 0 },
	{ "r2Qsk",               18, 7, 1, 0 },

	{ "r2PttDelay",          19, 7, 8, 1 },
	{ "modeA",               20, 7, 4, 0 },
	{ "modeB",               21, 3, 4, 0 },
	{ "modeC",               21, 7, 4, 0 },

	{ "txFocusAutoCmd",      22, 0, 1, 0 },
	{ "rxFocusAutoCmd",      22, 1, 1, 0 },
	{ "stereoFocusAutoCmd",  22, 2, 1, 0 },
	{ "txFocusAuto2Ptt",     22, 3, 1, 0 },
	{ "rxFocusFollowTx",     22, 4, 1, 0 },
	{ "stereoFocusNone",     22, 5, 1, 0 },
	{ "txFocusPinInverted",  23, 0, 1, 0 },
	{ "rxFocusPinInverted",  23, 1, 1, 0 },
	{ "stereoFocusPinInverted",  23, 2, 1, 0 },
	{ "txFocusPin3",         23, 3, 1, 0 },
	{ "stereoFocusPin5",     23, 4, 1, 0 },

	{ "ser1Source",          24, 1, 2, 0 },
	{ "r1OutputDataType",    24, 6, 3, 0 },
	{ "r1BandDataInput",     24, 7, 1, 0 },
	{ "pin4Function",        24, 11, 4, 0 },
	{ "pin5Function",        24, 15, 1, 0 },
	{ "ser2Source",          24, 17, 2, 0 },
	{ "r2OutputDataType",    24, 22, 3, 0 },
	{ "r2BandDataInput",     24, 23, 1, 0 },
	{ "pin3Function",        24, 27, 4, 0 },
	{ "pin2Function",        24, 31, 4, 0 },

	{ "audioSwitchingDelay", 28, 7, 8, 0 },
	{ "r2InvertLnaPtt",      29, 0, 1, 0 },
	/* X30-X39 = keyFuncPs2[0] - keyFuncPs2[9] */
#if 0
	{ "r1FrMokExtra_Cw",     40, 7, 8, 0 },
#endif
	{ "r1FrMokExtra_Cw.audioRxMic", 40, 0, 1, 0 },
	{ "r1FrMokExtra_Cw.reserve1", 40, 1, 1, 0 },
	{ "r1FrMokExtra_Cw.audioTxMic", 40, 2, 1, 0 },
	{ "r1FrMokExtra_Cw.reserve2", 40, 3, 1, 0 },
	{ "r1FrMokExtra_Cw.audioTxFootSwMic", 40, 4, 1, 0 },
	{ "r1FrMokExtra_Cw.reserve3", 40, 5, 1, 0 },
	{ "r1FrMokExtra_Cw.voiceCodec", 40, 6, 1, 1 },
	{ "r1FrMokExtra_Cw.reserve4", 40, 7, 1, 0 },

#if 0
	{ "r2FrBase_Cw",         41, 7, 8, 0xEA },
#endif
	{ "r2FrBase_Cw.audioRx",	41,  1, 2, 2 }, /* MK2, DK2, MK2R */ /* A */
	{ "r2FrBase_Cw.audioTx",	41,  3, 2, 2 }, /* MK2, DK2, MK2R */ /* A */
	{ "r2FrBase_Cw.audioTxFootSw",	41,  5, 2, 2 }, /* MK2, DK2, MK2R */ /* A */
	{ "r2FrBase_Cw.ptt1",	41,  6, 1, 1 }, /* MK2, DK2, MK2R */
	{ "r2FrBase_Cw.ptt2",	41,  7, 1, 0 }, /* MK2, DK2, MK2R */
#if 0
	{ "r2FrMokExtra_Cw",     42, 7, 8, 0 },
#endif
	{ "r2FrMokExtra_Cw.audioRxMic", 42, 0, 1, 0 },
	{ "r2FrMokExtra_Cw.reserve1", 42, 1, 1, 0 },
	{ "r2FrMokExtra_Cw.audioTxMic", 42, 2, 1, 0 },
	{ "r2FrMokExtra_Cw.reserve2", 42, 3, 1, 0 },
	{ "r2FrMokExtra_Cw.audioTxFootSwMic", 42, 4, 1, 0 },
	{ "r2FrMokExtra_Cw.reserve3", 42, 5, 1, 0 },
	{ "r2FrMokExtra_Cw.voiceCodec", 42, 6, 1, 1 },
	{ "r2FrMokExtra_Cw.reserve4", 42, 7, 1, 0 },

#if 0
	{ "r1FrMokExtra_Voice",  43, 7, 8, 0 },
#endif
	{ "r1FrMokExtra_Voice.audioRxMic", 43, 0, 1, 0 },
	{ "r1FrMokExtra_Voice.reserve1", 43, 1, 1, 0 },
	{ "r1FrMokExtra_Voice.audioTxMic", 43, 2, 1, 1 },
	{ "r1FrMokExtra_Voice.reserve2", 43, 3, 1, 0 },
	{ "r1FrMokExtra_Voice.audioTxFootSwMic", 43, 4, 1, 0 },
	{ "r1FrMokExtra_Voice.reserve3", 43, 5, 1, 0 },
	{ "r1FrMokExtra_Voice.voiceCodec", 43, 6, 1, 1 },
	{ "r1FrMokExtra_Voice.reserve4", 43, 7, 1, 0 },


#if 0
	{ "r2FrBase_Voice",      44, 7, 8, 0xEA },
#endif
	{ "r2FrBase_Voice.audioRx",	44,  1, 2, 2 }, /* MK2, DK2, MK2R */ /* A */
	{ "r2FrBase_Voice.audioTx",	44,  3, 2, 3 }, /* MK2, DK2, MK2R */ /* C */
	{ "r2FrBase_Voice.audioTxFootSw",	44,  5, 2, 2 }, /* MK2, DK2, MK2R */ /* A */
	{ "r2FrBase_Voice.ptt1",	44,  6, 1, 1 }, /* MK2, DK2, MK2R */
	{ "r2FrBase_Voice.ptt2",	44,  7, 1, 0 }, /* MK2, DK2, MK2R */
#if 0
	{ "r2FrMokExtra_Voice",  45, 7, 8, 0 },
#endif
	{ "r2FrMokExtra_Voice.audioRxMic", 45, 0, 1, 0 },
	{ "r2FrMokExtra_Voice.reserve1", 45, 1, 1, 0 },
	{ "r2FrMokExtra_Voice.audioTxMic", 45, 2, 1, 1 },
	{ "r2FrMokExtra_Voice.reserve2", 45, 3, 1, 0 },
	{ "r2FrMokExtra_Voice.audioTxFootSwMic", 45, 4, 1, 0 },
	{ "r2FrMokExtra_Voice.reserve3", 45, 5, 1, 0 },
	{ "r2FrMokExtra_Voice.voiceCodec", 45, 6, 1, 1 },
	{ "r2FrMokExtra_Voice.reserve4", 45, 7, 1, 0 },


	{ "accSer1Func",         46, 7, 8, 0 },
	{ "accSer2Func",         47, 7, 8, 0 },

	{ "accSer1BaudRate",     48, 7, 8, 0x24 },
	{ "accSer1BaudRate",     49, 7, 8, 0x24 },

	{ "accSer1Par",          50, 7, 8, 0 },
	{ "accSer2Par",          51, 7, 8, 0 },

	{ "r2PaPttTail",         52, 7, 8, 1 },
	{ "r2LnaPttTail",        53, 7, 8, 1 },
};

struct citem c_u2r[] = {
	{ "r2FrBase",	        14,  7, 8, 0xEA },
	{ "singleSerialCw",     15,  1, 1, 0 },
	{ "singleSerialPtt",    15,  2, 1, 0 },
	{ "r2DelaySerialPtt",   16,  1, 1, 0 },
	{ "lastOneWins",        16,  2, 1, 0 },
	{ "dualTx",             16,  3, 1, 0 },
	{ "useLptPtt",          16,  4, 1, 0 },
	{ "useLptCw",           16,  5, 1, 0 },
	{ "r2InvertFsk",        16,  7, 1, 0 },

	{ "disableBandLock",    17,  4, 1, 0 },
	{ "r2KeyerMode",        17,  6, 2, 0 },

	{ "r2ForceKeyerMode",   18,  0, 1, 0 },
	{ "singleSerialFsk",    18,  1, 1, 0 },
	{ "r2AllowCwInVoice",   18,  5, 1, 0 },
	{ "singleWinkeyCwPtt",  18,  6, 1, 0 },

	{ "r2PttDelay",         19,  7, 8, 0 },

	{ "txFocusAutoCmd",      22, 0, 1, 0 },
	{ "rxFocusAutoCmd",      22, 1, 1, 0 },
	{ "stereoFocusAutoCmd",  22, 2, 1, 0 },
	{ "txFocusAuto2Ptt",     22, 3, 1, 0 },
	{ "rxFocusFollowTx",     22, 4, 1, 0 },
	{ "stereoFocusNone",     22, 5, 1, 0 },
	{ "txFocusPinInverted",  23, 0, 1, 0 },
	{ "rxFocusPinInverted",  23, 1, 1, 0 },
	{ "stereoFocusPinInverted",  23, 2, 1, 0 },
	{ "txFocusPin3",         23, 3, 1, 0 },
	{ "stereoFocusPin5",     23, 4, 1, 0 },

	{ "audioSwitchingDelay", 28, 7, 8, 0 },
	{ "r2InvertLnaPtt",      29, 0, 1, 0 },
	/* X30-X39 = keyFuncPs2[0] - keyFuncPs2[9] */

	{ "rxFocus_r1RxRx",      40, 0, 1, 0 },
	{ "split_r1RxRx",        40, 1, 1, 0 },
	{ "rxFocus_r1TxRxComputer",40, 2, 1, 0 },
	{ "split_r1TxRxComputer",40, 3, 1, 0 },
	{ "rxFocus_r2RxRx",      40, 4, 1, 0 },
	{ "split_r2RxRx",        40, 5, 1, 0 },
	{ "rxFocus_r2RxTxComputer",40, 6, 1, 0 },
	{ "split_r2RxTxComputer",40, 7, 1, 0 },
	{ "rxFocus_earsStereo",  40, 8, 1, 0 },
	{ "split_earsStereo",    40, 9, 1, 0 },
	{ "rxFocus_r1TxRxManual",40,12, 1, 0 },
	{ "split_r1TxRxManual",  40,13, 1, 0 },
	{ "rxFocus_r2RxTxManual",40,14, 1, 0 },
	{ "split_r2RxTxManual",  40,15, 1, 0 },
	/* X42 = u2rFlags ???? */
	/* X44-X55 = keyFuncFh2[0] - keyFuncFh2[11] */

};

#define MK1_SIZE (sizeof(c_mk1))
#define DK2_SIZE (sizeof(c_dk2))
#define MK2_SIZE (sizeof(c_mk2))
#define MK2R_SIZE (sizeof(c_mk2r))
#define U2R_SIZE (sizeof(c_u2r))

#define G1_SIZE (DK2_SIZE > MK2_SIZE ? DK2_SIZE : MK2_SIZE) 
#define G2_SIZE (MK2R_SIZE > U2R_SIZE ? MK2R_SIZE : U2R_SIZE)
#define MAX_NUM_CITEMS (((G1_SIZE > G2_SIZE ? G1_SIZE : G2_SIZE) + sizeof(c_base)) / sizeof(struct citem))

struct kcfg {
	struct buffer b;
	const struct mh_info *mhi;
	struct citem citem[MAX_NUM_CITEMS];
	uint8_t num_citems;
};

static int set_byte(struct kcfg *kcfg, unsigned idx, unsigned char val) {
	if(kcfg->mhi->type == MHT_MK && idx < (unsigned)(kcfg->b.size + 1)) {
		kcfg->b.data[idx] = val;
		return 0;
	}
	if(idx < kcfg->b.size) {
		kcfg->b.data[idx] = val;
		return 0;
	}
	return -1;
}

static int get_byte(struct kcfg *kcfg, unsigned idx) {
	if(kcfg->mhi->type == MHT_MK && idx < (unsigned)(kcfg->b.size + 1))
		return kcfg->b.data[idx];

	if(idx < kcfg->b.size)
		return kcfg->b.data[idx];
	return -1;
}

static const struct citem *find_citem(struct kcfg *kcfg,  const char *key) {
	uint16_t i;

	for(i = 0; i < kcfg->num_citems; i++) {
		if(!strcasecmp(key, kcfg->citem[i].key))
			return &kcfg->citem[i];
	}
	return NULL;
}

static uint8_t width2mask(int w) {
	return (0xff >> (8-w));
}

int kcfg_set_val(struct kcfg *kcfg, const char *key, int val) {
	const struct citem *cp;
	int c;
	int idx, bit, mask;

	cp = find_citem(kcfg, key);
	if(!cp) {
		warn("(kcfg) unknown keyer option: %s", key);
		return -1;
	}

	idx = cp->off + cp->base_bit / 8;
	bit = cp->base_bit % 8;
	mask = width2mask(cp->width);

	c = get_byte(kcfg, idx) ;
	if(c < 0) {
		err("(kcfg) %s() index %d out of range!", __func__, idx);
		return -1;
	}

	if(val > mask) {
		err("(kcfg) %s() invalid value %d for %s", __func__, val, key);
		return -1;
	}

	c &= ~(mask << (bit + 1 - cp->width));
	c |=  (val << (bit + 1 - cp->width));

	set_byte(kcfg, idx, c);
	return 0;
}

struct kcfg *kcfg_create(const struct mh_info *mhi) {
	uint8_t buf_size = 0;

	if(mhi->type <= MHT_UNKNOWN || mhi->type >= MHT_MAX) {
		err("(kcfg) Could not create. Unsupported keyer type!");
		return NULL;
	}

	struct kcfg *kcfg = w_calloc(1, sizeof(*kcfg));
	kcfg->mhi = mhi;
	memcpy(kcfg->citem, c_base, sizeof(c_base));

	switch(mhi->type) {

	case MHT_MK:
		memcpy(&kcfg->citem[ARRAY_SIZE(c_base)], c_mk1, sizeof(c_mk1));
		kcfg->num_citems = ARRAY_SIZE(c_base) + ARRAY_SIZE(c_mk1);
		buf_size = 12;

		/* Bad hack to treat MK1 limitations. Store r1FrBase_Digital
		 * "behind" the buffer.
		 * Index 0 r1FrBase is handled by c_mk1 extension.
		 */
		kcfg->citem[0].off = 12;
		kcfg->citem[1].off = 12;
		kcfg->citem[2].off = 12;
		kcfg->citem[3].off = 12;
		kcfg->citem[4].off = 12;
		break;
	case MHT_DK2:
		memcpy(&kcfg->citem[ARRAY_SIZE(c_base)], c_dk2, sizeof(c_dk2));
		kcfg->num_citems = ARRAY_SIZE(c_base) + ARRAY_SIZE(c_dk2);
		buf_size = 56;
		break;
	case MHT_MK2:
		memcpy(&kcfg->citem[ARRAY_SIZE(c_base)], c_mk2, sizeof(c_mk2));
		kcfg->num_citems = ARRAY_SIZE(c_base) + ARRAY_SIZE(c_mk2);
		buf_size = 56;
		break;
	case MHT_MK2R:
	case MHT_MK2Rp:
		memcpy(&kcfg->citem[ARRAY_SIZE(c_base)], c_mk2r, sizeof(c_mk2r));
		kcfg->num_citems = ARRAY_SIZE(c_base) + ARRAY_SIZE(c_mk2r);
		buf_size = 56;
		break;
	case MHT_U2R:
		memcpy(&kcfg->citem[ARRAY_SIZE(c_base)], c_u2r, sizeof(c_u2r));
		kcfg->num_citems = ARRAY_SIZE(c_base) + ARRAY_SIZE(c_u2r);
		buf_size = 56;
		break;
	case MHT_CK:
		kcfg->num_citems = ARRAY_SIZE(c_base);
		buf_size = 12;
		break;
	case MHT_DK:
		kcfg->num_citems = ARRAY_SIZE(c_base);
		buf_size = 12;
		break;
/*
	case MHT_MK:
		kcfg->num_citems = ARRAY_SIZE(c_base);
		buf_size = 12;
		break;
*/
	case MHT_SM:
	case MHT_SMD:
		kcfg->num_citems = ARRAY_SIZE(c_base);
		buf_size = 56;
		break;
	}

	buf_add_size(&kcfg->b, buf_size);

	// Init buffer
	uint16_t i;
	for(i = 0; i < kcfg->num_citems; i++)
		kcfg_set_val(kcfg, kcfg->citem[i].key, kcfg->citem[i].def);

	// DK2 (and probably DK as well) don't support mic PTT (PTT1 flag).
	// Hence we set PTT2 to 1 for CW mode as the default.
	if(mhi->type == MHT_DK2 || mhi->type == MHT_DK) {
		kcfg_set_val(kcfg, "r1FrBase_Cw.ptt1", 0);
		kcfg_set_val(kcfg, "r1FrBase_Cw.ptt2", 1);
	}

	// Overwrite some defaults for MK2
	if(mhi->type == MHT_MK2) {
		kcfg_set_val(kcfg, "r1FrBase_Cw.audioRx", AUDIO_SW_A);
		kcfg_set_val(kcfg, "r1FrBase_Cw.audioTx", AUDIO_SW_A);
		kcfg_set_val(kcfg, "r1FrBase_Cw.audioTxFootSw", AUDIO_SW_A);

		kcfg_set_val(kcfg, "r1FrBase_Voice.audioRx", AUDIO_SW_A);
		kcfg_set_val(kcfg, "r1FrBase_Voice.audioTx", AUDIO_SW_C);
		kcfg_set_val(kcfg, "r1FrBase_Voice.audioTxFootSw", AUDIO_SW_A);

		kcfg_set_val(kcfg, "r1FrBase_Digital.audioRx", AUDIO_SW_B);
		kcfg_set_val(kcfg, "r1FrBase_Digital.audioTx", AUDIO_SW_B);
		kcfg_set_val(kcfg, "r1FrBase_Digital.audioTxFootSw", AUDIO_SW_D);
	}

	return kcfg;
}

void kcfg_destroy(struct kcfg *kcfg) {
	if(kcfg)
		free(kcfg);
}

struct buffer *kcfg_get_buffer(struct kcfg *kcfg) {
	if(kcfg)
		return (&kcfg->b);
	return NULL;
}

void kcfg_iter_begin(struct kcfg *kcfg, struct kcfg_iterator *iter) {
	iter->kcfg = kcfg;
	iter->idx = -1;
}

int kcfg_iter_next(struct kcfg_iterator *iter) {
	if(!iter || !iter->kcfg)
		return 0;
	iter->idx++;
	if(iter->idx >= iter->kcfg->num_citems)
		return 0;
	return 1;
}

int kcfg_iter_get(struct kcfg_iterator *iter, const char **keyp, int *valp) {
	struct kcfg *kcfg = iter->kcfg;
	
	if(!iter || !kcfg)
		return -1;

	if(iter->idx >= kcfg->num_citems)
		return -1;

	struct citem *cp = &kcfg->citem[iter->idx];

	*keyp = cp->key;


	uint16_t idx = cp->off + cp->base_bit / 8;
	uint16_t bit = cp->base_bit % 8;
	uint16_t mask = width2mask(cp->width);
	int16_t	c = get_byte(kcfg, idx);

	if(c < 0) {
		err("(kcfg) %s() index %d out of range!", __func__, idx);
		return -1;
	}
	*valp = (c >> (bit + 1 - cp->width)) & mask;
	return 0;
}

void kcfg_update_keyer_mode(struct kcfg *kcfg, uint8_t cur, uint8_t r1, uint8_t r2) {
	if(!kcfg)
		return;
	if(kcfg->mhi->flags & MHF_HAS_R2) {
		kcfg_set_val(kcfg, "r1KeyerMode", r1);
		kcfg_set_val(kcfg, "r2KeyerMode", r2);
	} else {
		kcfg_set_val(kcfg, "r1KeyerMode", cur);
	}
}
