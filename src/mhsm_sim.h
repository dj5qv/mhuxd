#ifndef MHSM_SIM_C
#define MHSM_SIM_C

#include <stdint.h>
#include "buffer.h"
#include "logger.h"

static const char *label_ant1 = "ANT-1";
static const char *name_ant1 = "ANTENNA 1";
static const char *label_ant2 = "ANT-2";
static const char *name_ant2 = "ANTENNA 2";
static const char *label_ant3 = "ANT-3";
static const char *name_ant3 = "ANTENNA 3";
static const char *label_ant4 = "ANT-4";
static const char *name_ant4 = "ANTENNA 4";
static const char *label_ant5 = "ANT-5";
static const char *name_ant5 = "ANTENNA 5";


static const char *label_grp1 = "GRP-1";
static const char *name_grp1 = "GROUP 1";
static const char *label_grp2 = "GRP-2";
static const char *name_grp2 = "GROUP 2";
static const char *label_grp3 = "VR-1";
static const char *name_grp3 = "VIRO 1";
static const char *label_grp4 = "VR-2";
static const char *name_grp4 = "VIRO 2";


static const char *name_band1 = "80-CW";
static const char *name_band_stop = "$STOPPER$";

static struct buffer bp_buf;

static void smsim_init() {
	int i;
	int32_t lowf, hif;

	buf_reset(&bp_buf);

	buf_append_c(&bp_buf, 2);  // format
	buf_append_c(&bp_buf, 0);  // sequencerOutputs
	buf_append_c(&bp_buf, 0);
	buf_append_c(&bp_buf, 0);
	buf_append_c(&bp_buf, 0);  // bandDependend
	buf_append_c(&bp_buf, 0);
	buf_append_c(&bp_buf, 0);
	buf_append_c(&bp_buf, 1);  // civFunc
	buf_append_c(&bp_buf, 24); // civBaudRate
	buf_append_c(&bp_buf, 0);  // civAddress
	buf_append_c(&bp_buf, 0);  // extSerFunc
	buf_append_c(&bp_buf, 24); // extSerBaudRate
	buf_append_c(&bp_buf, 0);  // extSerPar
	buf_append_c(&bp_buf, 5);  // antSwDelay #1
	buf_append_c(&bp_buf, 0);  // antSwDelay #2
	buf_append_c(&bp_buf, 0);  // bbmDelay
	buf_append_c(&bp_buf, 0);  // inhibitLead #1
	buf_append_c(&bp_buf, 5);  // inhibitLead #2
	buf_append_c(&bp_buf, 0);  // flags

	for(i = 19; i <= 114; i++)
		buf_append_c(&bp_buf, 8);

	// ant #1
	struct buffer buf;
	buf_reset(&buf);
	buf_append_c(&buf, 0);  // size, not known yet
	buf_append_c(&buf, strlen(label_ant1));
	buf_append(&buf, (uint8_t *)label_ant1, strlen(label_ant1));
	buf_append_c(&buf, strlen(name_ant1));
	buf_append(&buf, (uint8_t *)name_ant1, strlen(name_ant1));
	buf_append_c(&buf, 0);  // output settings
	buf_append_c(&buf, 0);
	buf_append_c(&buf, 0);
	buf_append_c(&buf, 0);  // flags
	buf_append_c(&buf, 1);  // paAntNumber
	buf_append_c(&buf, 1);  // rotator
	buf_append_c(&buf, 90); // rotator offset
	buf_append_c(&buf, 0);  // rotator offset
	*buf.data = buf.size - 1;
	buf_append(&bp_buf, buf.data, buf.size);

	// ant #2
	buf_reset(&buf);
	buf_append_c(&buf, 0);  // size, not known yet
	buf_append_c(&buf, strlen(label_ant2));
	buf_append(&buf, (uint8_t *)label_ant2, strlen(label_ant2));
	buf_append_c(&buf, strlen(name_ant2));
	buf_append(&buf, (uint8_t *)name_ant2, strlen(name_ant2));
	buf_append_c(&buf, 0);  // output settings
	buf_append_c(&buf, 0);
	buf_append_c(&buf, 0);
	buf_append_c(&buf, 0);  // flags
	buf_append_c(&buf, 2);  // paAntNumber
	buf_append_c(&buf, 0);  // rotator
	*buf.data = buf.size - 1;
	buf_append(&bp_buf, buf.data, buf.size);

	// ant #3
	buf_reset(&buf);
	buf_append_c(&buf, 0);  // size, not known yet
	buf_append_c(&buf, strlen(label_ant3));
	buf_append(&buf, (uint8_t *)label_ant3, strlen(label_ant3));
	buf_append_c(&buf, strlen(name_ant3));
	buf_append(&buf, (uint8_t *)name_ant3, strlen(name_ant3));
	buf_append_c(&buf, 0);  // output settings
	buf_append_c(&buf, 0);
	buf_append_c(&buf, 0);
	buf_append_c(&buf, 0);  // flags
	buf_append_c(&buf, 2);  // paAntNumber
	buf_append_c(&buf, 0);  // rotator
	*buf.data = buf.size - 1;
	buf_append(&bp_buf, buf.data, buf.size);

	// ant #4
	buf_reset(&buf);
	buf_append_c(&buf, 0);  // size, not known yet
	buf_append_c(&buf, strlen(label_ant4));
	buf_append(&buf, (uint8_t *)label_ant4, strlen(label_ant4));
	buf_append_c(&buf, strlen(name_ant4));
	buf_append(&buf, (uint8_t *)name_ant4, strlen(name_ant4));
	buf_append_c(&buf, 0);  // output settings
	buf_append_c(&buf, 0);
	buf_append_c(&buf, 0);
	buf_append_c(&buf, 0);  // flags
	buf_append_c(&buf, 2);  // paAntNumber
	buf_append_c(&buf, 1);  // rotator
	buf_append_c(&buf, 30); // rotator offset
	buf_append_c(&buf, 0);  // rotator offset
	*buf.data = buf.size - 1;
	buf_append(&bp_buf, buf.data, buf.size);

	// ant #5
	buf_reset(&buf);
	buf_append_c(&buf, 0);  // size, not known yet
	buf_append_c(&buf, strlen(label_ant5));
	buf_append(&buf, (uint8_t *)label_ant5, strlen(label_ant5));
	buf_append_c(&buf, strlen(name_ant5));
	buf_append(&buf, (uint8_t *)name_ant5, strlen(name_ant5));
	buf_append_c(&buf, 0);  // output settings
	buf_append_c(&buf, 0);
	buf_append_c(&buf, 0);
	buf_append_c(&buf, 0);  // flags
	buf_append_c(&buf, 2);  // paAntNumber
	buf_append_c(&buf, 0);  // rotator
	*buf.data = buf.size - 1;
	buf_append(&bp_buf, buf.data, buf.size);

	// ant stop
	buf_append_c(&bp_buf, 0);

	// group #1
	buf_reset(&buf);
	buf_append_c(&buf, 0);
	buf_append_c(&buf, strlen(label_grp1));
	buf_append(&buf, (uint8_t *)label_grp1, strlen(label_grp1));
	buf_append_c(&buf, strlen(name_grp1));
	buf_append(&buf, (uint8_t *)name_grp1, strlen(name_grp1));
	buf_append_c(&buf, 2);  // num antennas
	// #1
	buf_append_c(&buf, 0);  // idx
	buf_append_c(&buf, 1);  // min az
	buf_append_c(&buf, 0);  // min az
	buf_append_c(&buf, 2);  // max az
	buf_append_c(&buf, 0);  // min az
	// #2
	buf_append_c(&buf, 1);  // idx
	buf_append_c(&buf, 2);  // min az
	buf_append_c(&buf, 0);  // min az
	buf_append_c(&buf, 3);  // max az
	buf_append_c(&buf, 0);  // min az

	buf_append_c(&buf, 1);  // flags -> ant group
	buf_append_c(&buf, 0);  // status
	buf_append_c(&buf, 0);  // status
	*buf.data = buf.size - 1;
	buf_append(&bp_buf, buf.data, buf.size);

	// group #2
	buf_reset(&buf);
	buf_append_c(&buf, 0);
	buf_append_c(&buf, strlen(label_grp2));
	buf_append(&buf, (uint8_t *)label_grp2, strlen(label_grp2));
	buf_append_c(&buf, strlen(name_grp2));
	buf_append(&buf, (uint8_t *)name_grp2, strlen(name_grp2));
	buf_append_c(&buf, 2);  // num antennas
	// #1
	buf_append_c(&buf, 2);  // idx
	buf_append_c(&buf, 90);  // min az
	buf_append_c(&buf, 0);  // min az
	buf_append_c(&buf, 100);  // max az
	buf_append_c(&buf, 0);  // min az
	// #2
	buf_append_c(&buf, 3);  // idx
	buf_append_c(&buf, 100);  // min az
	buf_append_c(&buf, 0);  // min az
	buf_append_c(&buf, 110);  // max az
	buf_append_c(&buf, 0);  // min az

	buf_append_c(&buf, 1);  // flags -> ant group
	buf_append_c(&buf, 0);  // status
	buf_append_c(&buf, 0);  // status
	*buf.data = buf.size - 1;
	buf_append(&bp_buf, buf.data, buf.size);

	// group #3
	buf_reset(&buf);
	buf_append_c(&buf, 0);
	buf_append_c(&buf, strlen(label_grp3));
	buf_append(&buf, (uint8_t *)label_grp3, strlen(label_grp3));
	buf_append_c(&buf, strlen(name_grp3));
	buf_append(&buf, (uint8_t *)name_grp3, strlen(name_grp3));
	buf_append_c(&buf, 2);  // num antennas
	// #1
	buf_append_c(&buf, 2);  // idx
	buf_append_c(&buf, 90);  // min az
	buf_append_c(&buf, 0);  // min az
	buf_append_c(&buf, 100);  // max az
	buf_append_c(&buf, 0);  // min az
	// #2
	buf_append_c(&buf, 3);  // idx
	buf_append_c(&buf, 100);  // min az
	buf_append_c(&buf, 0);  // min az
	buf_append_c(&buf, 110);  // max az
	buf_append_c(&buf, 0);  // min az

	buf_append_c(&buf, 0);  // flags -> virt. rotator
	buf_append_c(&buf, 0);  // status
	buf_append_c(&buf, 0);  // status
	*buf.data = buf.size - 1;
	buf_append(&bp_buf, buf.data, buf.size);


	// group #4
	buf_reset(&buf);
	buf_append_c(&buf, 0);
	buf_append_c(&buf, strlen(label_grp4));
	buf_append(&buf, (uint8_t *)label_grp4, strlen(label_grp4));
	buf_append_c(&buf, strlen(name_grp4));
	buf_append(&buf, (uint8_t *)name_grp4, strlen(name_grp4));
	buf_append_c(&buf, 2);  // num antennas
	// #1
	buf_append_c(&buf, 2);  // idx
	buf_append_c(&buf, 90);  // min az
	buf_append_c(&buf, 0);  // min az
	buf_append_c(&buf, 100);  // max az
	buf_append_c(&buf, 0);  // min az
	// #2
	buf_append_c(&buf, 3);  // idx
	buf_append_c(&buf, 100);  // min az
	buf_append_c(&buf, 0);  // min az
	buf_append_c(&buf, 110);  // max az
	buf_append_c(&buf, 0);  // min az

	buf_append_c(&buf, 0);  // flags -> virt. rotator
	buf_append_c(&buf, 0);  // status
	buf_append_c(&buf, 0);  // status
	*buf.data = buf.size - 1;
	buf_append(&bp_buf, buf.data, buf.size);


	// grp stop
	buf_append_c(&bp_buf, 0);

	// band #1
	lowf = 3500000;
	hif = 3600000;
	buf_reset(&buf);
	buf_append_c(&buf, 0);  // size, not known yet
	buf_append_c(&buf, lowf & 0xff);
	buf_append_c(&buf, (lowf >> 8) & 0xff);
	buf_append_c(&buf, (lowf >> 16) & 0xff);
	buf_append_c(&buf, (lowf >> 24) & 0xff);
	buf_append_c(&buf, hif & 0xff);
	buf_append_c(&buf, (hif >> 8) & 0xff);
	buf_append_c(&buf, (hif >> 16) & 0xff);
	buf_append_c(&buf, (hif >> 24) & 0xff);
	buf_append_c(&buf, strlen(name_band1));
	buf_append(&buf, (uint8_t *)name_band1, strlen(name_band1));

	buf_append_c(&buf, 0); // outputs
	buf_append_c(&buf, 0); // bpf/seq mask
	buf_append_c(&buf, 0); // bpf/seq mask
	buf_append_c(&buf, 0); // bpf/seq mask
	buf_append_c(&buf, 4); // num ant selections

	buf_append_c(&buf, 0); // #1
	buf_append_c(&buf, 4); // ant idx
	buf_append_c(&buf, 0); // flags

	buf_append_c(&buf, 3); // #2  #grp id 3
	buf_append_c(&buf, 0); //
	buf_append_c(&buf, 0); // flags


	buf_append_c(&buf, 2); // #3 grp id 2
	buf_append_c(&buf, 0); // ba0 grp id 2
	buf_append_c(&buf, 0); // flags

	buf_append_c(&buf, 0); // #4
	buf_append_c(&buf, 3); // ba0 ant id 3
	buf_append_c(&buf, 0); // flags


	buf_append_c(&buf, 0); // status
	buf_append_c(&buf, 0); // status
	buf_append_c(&buf, 0); // status

	*buf.data = buf.size - 1;
	buf_append(&bp_buf, buf.data, buf.size);

	// band stopper record
	lowf = 0;
	hif = 0xffffffff;
	buf_reset(&buf);
	buf_append_c(&buf, 0);  // size, not known yet
	buf_append_c(&buf, lowf & 0xff);
	buf_append_c(&buf, (lowf >> 8) & 0xff);
	buf_append_c(&buf, (lowf >> 16) & 0xff);
	buf_append_c(&buf, (lowf >> 24) & 0xff);
	buf_append_c(&buf, hif & 0xff);
	buf_append_c(&buf, (hif >> 8) & 0xff);
	buf_append_c(&buf, (hif >> 16) & 0xff);
	buf_append_c(&buf, (hif >> 24) & 0xff);
	buf_append_c(&buf, strlen(name_band_stop));
	buf_append(&buf, (uint8_t *)name_band_stop, strlen(name_band_stop));

	buf_append_c(&buf, 0); // outputs
	buf_append_c(&buf, 0); // bpf/seq mask
	buf_append_c(&buf, 0); // bpf/seq mask
	buf_append_c(&buf, 0); // bpf/seq mask
	buf_append_c(&buf, 0); // num ant selections

	buf_append_c(&buf, 0); // status
	buf_append_c(&buf, 0); // status
	buf_append_c(&buf, 0); // status

	*buf.data = buf.size - 1;
	buf_append(&bp_buf, buf.data, buf.size);

	// band stop
	buf_append_c(&bp_buf, 0);


	info("(smsim) bp_buf size: %d", bp_buf.size);
}


#endif // MHSM_SIM_C
