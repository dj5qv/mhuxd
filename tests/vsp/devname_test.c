/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2026  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

/* Unit test for vsp_devname_is_valid(). Links the real con_vsp.o, needs no root
   and touches no devices, so it can run on its own:

     make -C tests/vsp && ./tests/vsp/devname_test
*/

#include <stdio.h>
#include <string.h>
#include "con_vsp.h"

static const struct {
	const char *name;
	int want;
	const char *why;
} cases[] = {
	/* what the web UI generates, and reasonable hand written names */
	{ "cat1",     1, "web UI default" },
	{ "ptt1",     1, "web UI default" },
	{ "fsk1",     1, "web UI default" },
	{ "wk",       1, "web UI default" },
	{ "cat1_2",   1, "web UI collision suffix" },
	{ "CAT-1",    1, "upper case and dash" },
	{ "cat.1",    1, "dot" },
	{ "a",        1, "single character" },
	{ "0",        1, "leading digit" },
	{ "x9_-.y",   1, "every accepted class" },

	{ NULL,       0, "no name" },
	{ "",         0, "empty name" },
	{ "cat/1",    0, "path separator, would nest the device node" },
	{ ".",        0, "traversal" },
	{ "..",       0, "traversal" },
	{ "./x",      0, "traversal" },
	{ "cat 1",    0, "space, splits a udev SYMLINK+= value" },
	{ "_cat",     0, "leading punctuation" },
	{ "-cat",     0, "leading punctuation" },
	{ ".cat",     0, "leading punctuation" },
	{ "cat!1",    0, "collides with the sysfs name mangling" },
	{ "cat$1",    0, "shell metacharacter" },
	{ "cat;1",    0, "shell metacharacter" },
	{ "cat*1",    0, "glob metacharacter" },
	{ "\xc3\xa4", 0, "non-ASCII" },
	{ "cat\t1",   0, "control character" },
};

int main(void) {
	char name[VSP_DEVNAME_MAX + 8];
	unsigned fails = 0;
	size_t i;

	for(i = 0; i < sizeof(cases)/sizeof(*cases); i++) {
		int got = vsp_devname_is_valid(cases[i].name);

		if(got != cases[i].want) {
			printf("FAIL  %-10s got %s, want %s (%s)\n",
			       cases[i].name ? cases[i].name : "(NULL)",
			       got ? "accepted" : "rejected",
			       cases[i].want ? "accepted" : "rejected",
			       cases[i].why);
			fails++;
		}
	}

	memset(name, 'a', sizeof(name));
	name[VSP_DEVNAME_MAX] = 0;
	if(!vsp_devname_is_valid(name)) {
		printf("FAIL  %d characters rejected, the limit itself must fit\n", VSP_DEVNAME_MAX);
		fails++;
	}

	memset(name, 'a', sizeof(name));
	name[VSP_DEVNAME_MAX + 1] = 0;
	if(vsp_devname_is_valid(name)) {
		printf("FAIL  %d characters accepted, over the limit\n", VSP_DEVNAME_MAX + 1);
		fails++;
	}

	if(fails) {
		printf("devname_test: %u failure(s)\n", fails);
		return 1;
	}

	printf("devname_test: %zu cases pass\n", i + 2);
	return 0;
}
