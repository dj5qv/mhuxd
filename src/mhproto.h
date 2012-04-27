/*
 *  mhux - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#ifndef MHPROTO_H
#define MHPROTO_H

enum {
    MH_CHANNEL_FLAGS   = 0, /* 1-3 must match byte position in keyer frame. */
    MH_CHANNEL_CONTROL = 1,
    MH_CHANNEL_WINKEY  = 2,
    MH_CHANNEL_PS2     = 3,
    MH_CHANNEL_R1      = 4,
    MH_CHANNEL_R2      = 5,
    MH_CHANNEL_R1_FSK  = 6,
    MH_CHANNEL_R2_FSK  = 7,
    MH_CHANNEL_MAX     = 8
};

enum {
    CMD_NOP             = 0x00,
    CMD_SET_CHANNEL_R1  = 0x01,
    CMD_SET_CHANNEL_AUX_R2  = 0x02,
    CMD_SET_CHANNEL_FSK_R1  = 0x03,
    CMD_SET_CHANNEL_FSK_R2  = 0x04,
    CMD_GET_VERSION         = 0x05,
    CMD_START_BOOTLOADER    = 0x06,
    CMD_JUST_RESTARTED      = 0x07,
    CMD_STORE_SETTINGS      = 0x08,
    CMD_SET_SETTINGS        = 0x09,
    CMD_SET_KEYER_MODE      = 0x0a,
    CMD_STORE_WINKEY_INIT   = 0x0b,
    CMD_RECORD_FSK_CW_MSG   = 0x0c,
    CMD_PLAY_FSK_CW_MSG     = 0x0d,
    CMD_ABORT_FSK_CW_MSG    = 0x0e,
    CMD_WINKEY_NO_RESPONSE  = 0x0f,
    CMD_ON_CONNECT          = 0x1a,
    CMD_STORE_CHANNEL_R1    = 0x1b,
    CMD_STORE_CHANNEL_R2    = 0x1c,
    CMD_PRESET_NAME         = 0x1d, /* MK2 only */
    CMD_CAT_R1_FR_MODE_INFO = 0x1e,
    CMD_CAT_R1_FREQUENCY_INFO = 0x1f,
    CMD_STORE_FSK_MSG_1     = 0x20, /* MK2, MK2R only */
    CMD_STORE_FSK_MSG_2     = 0x21,
    CMD_STORE_FSK_MSG_3     = 0x22,
    CMD_STORE_FSK_MSG_4     = 0x23,
    CMD_STORE_FSK_MSG_5     = 0x24,
    CMD_STORE_FSK_MSG_6     = 0x25,
    CMD_STORE_FSK_MSG_7     = 0x26,
    CMD_STORE_FSK_MSG_8     = 0x27,
    CMD_STORE_FSK_MSG_9     = 0x28,
    CMD_DVK_REC             = 0x29, /* MK2 only */
    CMD_DVK_PLAY            = 0x2a, /* MK2 only */
    CMD_DISPLAY_HOST_STRING_EVENT = 0x2b, /* SM, MK2 only */
    CMD_DISPLAY_HOST_STRING = 0x2c, /* SM, MK2 only */
    CMD_CANCEL_HOST_STRING  = 0x2d, /* SM, MK2 only */
    CMD_STORE_DISPLAY_STRING = 0x2e, /* SM, MK2 only */
    CMD_GET_DISPLAY_STRING  = 0x2f, /* SM, MK2 only */
    CMD_HOST_FOCUS_CONTROL  = 0x31, /* MK2R, U2R only */
    CMD_STORE_SCENARIO      = 0x32, /* MK2R, U2R only */
    CMD_GET_SCENARIO        = 0x33, /* MK2R, U2R only */
    CMD_APPLY_SCENARIO      = 0x34, /* MK2R, U2R only */
    CMD_HOST_ACC_OUTPUTS_CONTROL = 0x35, /* MK2R, U2R only */
    CMD_CAT_R2_FR_MODE_INFO = 0x36, /* MK2R only */
    CMD_CAT_R2_FREQUENCY_INFO = 0x37, /* MK2R only */

    /* 0x41 - 0x4D, 0x76 StationMaster only, not supported yet. */

    CMD_U2R_STATE           = 0x75,  /* U2R only */
    CMD_USB_RX_OVERFLOW     = 0x77,
    CMD_MPK_STATE           = 0x78, /* MK, DK2 only */
    CMD_ACC_STATE           = 0x79, /* U2R, MK2R only */
    CMD_DVK_CONTROL         = 0x7a, /* MK2R only */
    CMD_MOK_STATE           = 0x7b, /* MK2R only */

    CMD_KEYER_MODE          = 0x7c,
    CMD_AUTO_NUMBER         = 0x7d,
    CMD_ARE_YOU_THERE       = 0x7e,
    CMD_NOT_SUPPORTED       = 0x7f
};

enum {
	MH_BIT_MSB     = (1<<7),
	MHC2DFL_PTT_R1 = (1<<2),
	MHC2DFL_PTT_R2 = (1<<3),

	MHD2CFL_ANY_PTT = (1<<2),
	MHD2CFL_R2	= (1<<3),
	MHD2CFL_SQUELCH = (1<<4),
	MHD2CFL_FSK_BUSY= (1<<5),
	MHD2CFL_NON_VOX_PTT = (1<<6),
	MHD2CFL_FOOTSWITCH = (1<<7),

};

/* Command parameter sizes supported by mhux. */

enum {
	CMDPLEN_VERSION = 22,
};




#endif // MHPROTO_H
