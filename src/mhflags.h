
/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2013  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

#ifndef MHFLAGS_H
#define MHFLAGS_H

enum {
        MHC2DFL_RTS_R1 = (1<<0),
        MHC2DFL_RTS_R2 = (1<<1),
        MHC2DFL_PTT_R1 = (1<<2),
        MHC2DFL_PTT_R2 = (1<<3),
        MHC2DFL_KEY_R1 = (1<<6),
        MHC2DFL_KEY_R2 = (1<<7),

        MHD2CFL_CTS = (1<<0),
        MHD2CFL_LOCKOUT = (1<<1),
        MHD2CFL_ANY_PTT = (1<<2),
        MHD2CFL_R2      = (1<<3),
        MHD2CFL_SQUELCH = (1<<4),
        MHD2CFL_FSK_BUSY= (1<<5),
        MHD2CFL_NON_VOX_PTT = (1<<6),
        MHD2CFL_FOOTSWITCH = (1<<7),
};

#endif /* MHFLAGS_H */
