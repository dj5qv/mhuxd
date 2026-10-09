/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2026  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */

/*
 * Replays CAT traces through the native CAT decoders and checks assertions.
 * Trace format: see README.md in this directory.
 *
 *   cat_replay [-v] file.trace...
 *
 * -v prints every frame with its classification and the decoded state, in
 * trace format, so the output can be used as a trace again.
 */

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "cat_civ.h"
#include "cat_state.h"
#include "cat_trace.h"

#define MAX_PAYLOAD 4096

struct replay {
	const char *path;
	int lineno;
	int verbose;
	int failures;
	int asserts;
	struct cat_civ_cfg civ_cfg;
	struct cat_civ *civ;
	struct cat_state st;
	uint64_t t_ms;
	uint64_t day_ms;	// added to HH:MM:SS times after midnight
	char tag[8];
};

static void fmt_freq(char *buf, size_t sz, const struct cat_item *it) {
	if(it->valid)
		snprintf(buf, sz, "%u", it->val);
	else
		snprintf(buf, sz, "?");
}

static void frame_cb(const struct cat_civ_frame *f, void *user_data) {
	struct replay *rp = user_data;
	char time[32], line[CAT_TRACE_LINE_MAX];
	if(!rp->verbose)
		return;

	snprintf(time, sizeof(time), "%llu.%03llu", (unsigned long long)(f->t_ms / 1000),
		 (unsigned long long)(f->t_ms % 1000));
	cat_trace_civ_frame(line, sizeof(line), time, rp->tag, f, &rp->st);
	printf("%s\n", line);
}

static void fail(struct replay *rp, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void fail(struct replay *rp, const char *fmt, ...) {
	va_list ap;
	fprintf(stderr, "%s:%d: ", rp->path, rp->lineno);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	rp->failures++;
}

static int ensure_decoder(struct replay *rp) {
	if(rp->civ)
		return 0;
	rp->civ = cat_civ_create(&rp->civ_cfg, &rp->st);
	if(!rp->civ) {
		fail(rp, "out of memory");
		return -1;
	}
	cat_civ_set_frame_cb(rp->civ, frame_cb, rp);
	return 0;
}

// Strip a trailing comment ('#' outside of quotes) and trailing white space.
static void strip_comment(char *s) {
	int quoted = 0;
	for(char *p = s; *p; p++) {
		if(*p == '\\' && quoted && p[1]) {
			p++;
		} else if(*p == '"') {
			quoted = !quoted;
		} else if(*p == '#' && !quoted) {
			*p = '\0';
			break;
		}
	}
	size_t n = strlen(s);
	while(n && isspace((unsigned char)s[n - 1]))
		s[--n] = '\0';
}

static int hexval(char c) {
	if(c >= '0' && c <= '9') return c - '0';
	if(c >= 'a' && c <= 'f') return c - 'a' + 10;
	if(c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

// Hex bytes ("fe fe 94", "fefe94") and quoted text ("FA;\r") into buf.
static int parse_payload(struct replay *rp, const char *s, uint8_t *buf, size_t *len) {
	size_t n = 0;

	while(*s) {
		if(isspace((unsigned char)*s)) {
			s++;
		} else if(*s == '"') {
			s++;
			while(*s && *s != '"') {
				int c = (unsigned char)*s++;
				if(c == '\\' && *s) {
					c = *s++;
					switch(c) {
					case 'r': c = '\r'; break;
					case 'n': c = '\n'; break;
					case 't': c = '\t'; break;
					case 'x':
						if(hexval(s[0]) < 0 || hexval(s[1]) < 0) {
							fail(rp, "bad \\x escape");
							return -1;
						}
						c = hexval(s[0]) * 16 + hexval(s[1]);
						s += 2;
						break;
					default: break;	// \\ and \"
					}
				}
				if(n == MAX_PAYLOAD)
					goto too_long;
				buf[n++] = c;
			}
			if(*s != '"') {
				fail(rp, "unterminated string");
				return -1;
			}
			s++;
		} else {
			if(hexval(s[0]) < 0 || hexval(s[1]) < 0) {
				fail(rp, "bad hex byte near '%s'", s);
				return -1;
			}
			if(n == MAX_PAYLOAD)
				goto too_long;
			buf[n++] = hexval(s[0]) * 16 + hexval(s[1]);
			s += 2;
		}
	}
	*len = n;
	return 0;

too_long:
	fail(rp, "payload longer than %d bytes", MAX_PAYLOAD);
	return -1;
}

// "12.345" (seconds) or "HH:MM:SS.mmm". Returns 0 if tok is a time stamp.
static int parse_time(const char *tok, uint64_t *t_ms) {
	unsigned h = 0, m = 0;
	double sec;
	char *end;

	if(!isdigit((unsigned char)tok[0]))
		return -1;
	if(strchr(tok, ':')) {
		int consumed = 0;
		if(sscanf(tok, "%u:%u:%lf%n", &h, &m, &sec, &consumed) != 3 || tok[consumed])
			return -1;
	} else {
		errno = 0;
		sec = strtod(tok, &end);
		if(errno || *end)
			return -1;
	}
	*t_ms = (uint64_t)((h * 3600.0 + m * 60.0 + sec) * 1000.0 + 0.5);
	return 0;
}

static int parse_tag(const char *tok, enum cat_dir *dir) {
	if(!strcmp(tok, "R")) {
		*dir = CAT_DIR_RADIO;
	} else if(!strcmp(tok, "M")) {
		*dir = CAT_DIR_OWN;
	} else if(tok[0] == 'H' && tok[1] >= '1' && tok[1] <= '9' && !tok[2]) {
		*dir = CAT_DIR_HOST;
	} else {
		return -1;
	}
	return 0;
}

static void data_line(struct replay *rp, char *line) {
	static uint8_t buf[MAX_PAYLOAD];
	char *p = line, *tok;
	enum cat_dir dir;
	uint64_t t;
	size_t len;

	tok = strsep(&p, " \t");
	if(parse_time(tok, &t) == 0) {
		t += rp->day_ms;
		if(t + 12 * 3600 * 1000ULL < rp->t_ms) {
			// HH:MM:SS from the CAT monitor, past midnight
			rp->day_ms += 24 * 3600 * 1000ULL;
			t += 24 * 3600 * 1000ULL;
		}
		if(t < rp->t_ms) {
			fail(rp, "time goes backwards");
			return;
		}
		rp->t_ms = t;
		while(p && isspace((unsigned char)*p))
			p++;
		tok = p ? strsep(&p, " \t") : NULL;
	} else {
		rp->t_ms += 10;	// lines without time stamp are 10 ms apart
	}

	if(!tok || parse_tag(tok, &dir)) {
		fail(rp, "expected [time] H1..H9|M|R <bytes>, got '%s'", line);
		return;
	}
	snprintf(rp->tag, sizeof(rp->tag), "%s", tok);

	if(parse_payload(rp, p ? p : "", buf, &len) || ensure_decoder(rp))
		return;
	cat_civ_feed(rp->civ, dir, buf, len, rp->t_ms);
}

static void config_line(struct replay *rp, char *line) {
	char *p = line + 1, *tok;

	tok = strsep(&p, " \t");
	if(strcmp(tok, "civ")) {
		fail(rp, "unknown protocol '%s'", tok);
		return;
	}
	if(rp->civ) {
		// The CAT monitor writes the configuration whenever the decoder starts over.
		cat_civ_destroy(rp->civ);
		rp->civ = NULL;
		cat_state_init(&rp->st);
	}
	memset(&rp->civ_cfg, 0, sizeof(rp->civ_cfg));
	while((tok = strsep(&p, " \t"))) {
		unsigned v;
		if(!*tok)
			continue;
		if(sscanf(tok, "addr=%x", &v) == 1 && v <= 0xff)
			rp->civ_cfg.radio_addr = v;
		else if(sscanf(tok, "freq_len=%u", &v) == 1 && v >= 3 && v <= 6)
			rp->civ_cfg.freq_len = v;
		else
			fail(rp, "bad civ option '%s'", tok);
	}
	if(rp->verbose) {
		char buf[64];
		cat_trace_civ_config(buf, sizeof(buf), &rp->civ_cfg);
		printf("%s\n", buf);
	}
}

static void check(struct replay *rp, const char *key, const char *want, const char *got) {
	rp->asserts++;
	if(strcasecmp(want, got))
		fail(rp, "%s: expected %s, got %s", key, want, got);
}

static void check_item(struct replay *rp, const char *key, const char *want, const struct cat_item *it) {
	char got[16];
	fmt_freq(got, sizeof(got), it);
	check(rp, key, want, got);
}

static void check_u32(struct replay *rp, const char *key, const char *want, uint32_t val) {
	char got[16];
	snprintf(got, sizeof(got), "%u", val);
	check(rp, key, want, got);
}

static void assert_line(struct replay *rp, char *line) {
	char *p = line + 1, *tok;

	if(ensure_decoder(rp))
		return;

	const struct cat_state *st = &rp->st;
	const struct cat_civ_stats *stats = cat_civ_get_stats(rp->civ);

	while((tok = strsep(&p, " \t"))) {
		char *val, got[32];
		if(!*tok)
			continue;
		val = strchr(tok, '=');
		if(!val) {
			fail(rp, "expected key=value, got '%s'", tok);
			continue;
		}
		*val++ = '\0';

		if(!strcmp(tok, "rx")) {
			check_item(rp, tok, val, &st->vfo[CAT_VFO_SEL].freq);
		} else if(!strcmp(tok, "unsel")) {
			check_item(rp, tok, val, &st->vfo[CAT_VFO_UNSEL].freq);
		} else if(!strcmp(tok, "tx")) {
			uint32_t tx = cat_state_tx_freq(st);
			if(tx)
				snprintf(got, sizeof(got), "%u", tx);
			else
				snprintf(got, sizeof(got), "?");
			check(rp, tok, val, got);
		} else if(!strcmp(tok, "mode") || !strcmp(tok, "mode_unsel")) {
			enum cat_mode m = cat_state_mode(st, tok[4] ? CAT_VFO_UNSEL : CAT_VFO_SEL);
			check(rp, tok, val, m ? cat_mode_token(m) : "?");
		} else if(!strcmp(tok, "data")) {
			check_item(rp, tok, val, &st->vfo[CAT_VFO_SEL].data);
		} else if(!strcmp(tok, "keyer")) {
			check(rp, tok, val, cat_trace_keyer_str(cat_mode_to_keyer(cat_state_mode(st, CAT_VFO_SEL))));
		} else if(!strcmp(tok, "split")) {
			check_item(rp, tok, val, &st->split);
		} else if(!strcmp(tok, "vfo")) {
			check(rp, tok, val, st->vfo_ab.valid ? (st->vfo_ab.val ? "B" : "A") : "?");
		} else if(!strcmp(tok, "addr")) {
			snprintf(got, sizeof(got), "%02x", cat_civ_radio_addr(rp->civ));
			check(rp, tok, val, got);
		} else if(!strcmp(tok, "frames")) {
			check_u32(rp, tok, val, stats->frames);
		} else if(!strcmp(tok, "replies")) {
			check_u32(rp, tok, val, stats->replies);
		} else if(!strcmp(tok, "applied")) {
			check_u32(rp, tok, val, stats->applied);
		} else if(!strcmp(tok, "unmatched")) {
			check_u32(rp, tok, val, stats->unmatched);
		} else if(!strcmp(tok, "bad")) {
			check_u32(rp, tok, val, stats->bad);
		} else if(!strcmp(tok, "jams")) {
			check_u32(rp, tok, val, stats->jams);
		} else if(!strcmp(tok, "dropped")) {
			check_u32(rp, tok, val, stats->dropped);
		} else if(!strcmp(tok, "foreign")) {
			check_u32(rp, tok, val, stats->foreign);
		} else {
			fail(rp, "unknown key '%s'", tok);
		}
	}
}

static int replay_file(const char *path, int verbose) {
	struct replay rp = { .path = path, .verbose = verbose };
	char *line = NULL;
	size_t cap = 0;
	FILE *f;

	f = fopen(path, "r");
	if(!f) {
		fprintf(stderr, "%s: %s\n", path, strerror(errno));
		return -1;
	}
	cat_state_init(&rp.st);

	while(getline(&line, &cap, f) >= 0) {
		rp.lineno++;
		strip_comment(line);
		char *s = line;
		while(isspace((unsigned char)*s))
			s++;
		if(!*s)
			continue;
		if(*s == '@')
			config_line(&rp, s);
		else if(*s == '=')
			assert_line(&rp, s);
		else
			data_line(&rp, s);
	}
	free(line);
	fclose(f);

	if(rp.civ) {
		const struct cat_civ_stats *s = cat_civ_get_stats(rp.civ);
		printf("%s %s: %d assertions, %u frames, %u applied, %u unmatched, %u bad, %u jams, %u dropped\n",
		       rp.failures ? "FAIL" : "ok  ", path, rp.asserts, s->frames, s->applied,
		       s->unmatched, s->bad, s->jams, s->dropped);
		cat_civ_destroy(rp.civ);
	} else {
		printf("%s %s: no data\n", rp.failures ? "FAIL" : "ok  ", path);
	}
	return rp.failures ? -1 : 0;
}

int main(int argc, char **argv) {
	int verbose = 0, failed = 0, i = 1;

	if(i < argc && !strcmp(argv[i], "-v")) {
		verbose = 1;
		i++;
	}
	if(i >= argc) {
		fprintf(stderr, "usage: %s [-v] file.trace...\n", argv[0]);
		return 2;
	}
	for(; i < argc; i++)
		if(replay_file(argv[i], verbose))
			failed++;
	return failed ? 1 : 0;
}
