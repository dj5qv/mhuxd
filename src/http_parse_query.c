
#include <string.h>
#include <ctype.h>
#include <ClearSilver.h>
#include "http_parse_query.h"
#include "logger.h"


static char *_cgi_url_unescape (char *value)
{
	int i = 0, o = 0;
	unsigned char *s = (unsigned char *)value;

	if (s == NULL) return value;
	while (s[i]) {
		if (s[i] == '+') {
			s[o++] = ' ';
			i++;
		}
		else if (s[i] == '%' && isxdigit(s[i+1]) && isxdigit(s[i+2])) {
			char num;
			num = (s[i+1] >= 'A') ? ((s[i+1] & 0xdf) - 'A') + 10 : (s[i+1] - '0');
			num *= 16;
			num += (s[i+2] >= 'A') ? ((s[i+2] & 0xdf) - 'A') + 10 : (s[i+2] - '0');
			s[o++] = num;
			i+=3;
		} else {
			s[o++] = s[i++];
		}
	}
	if (i && o) s[o] = '\0';
	return (char *)s;
}


int http_parse_query(HDF *hdf, char *query) {
	NEOERR *err;
	char *k, *v;
	char *saveptr;

	k = strtok_r(query, "&", &saveptr);
	while(k && *k) {
		v = strchr(k, '=');
		if(!v)
			v = "";
		else {
			*v = 0x00;
			v++;
		}

		_cgi_url_unescape(k);
		_cgi_url_unescape(v);

		err = hdf_set_value(hdf, k, v);
		if(err != STATUS_OK) {
			warn("%s() could not set hdf value %s/%s!", __func__, k, v);
			nerr_ignore(&err);
		}

		k = strtok_r(NULL, "&", &saveptr);
	}

	return 0;
}
