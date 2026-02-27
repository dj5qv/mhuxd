noinst_LIBRARIES = libclearsilver.a
noinst_HEADERS = ClearSilver.h cs_config.h

AM_CPPFLAGS = -I. -I$(srcdir) -I$(srcdir)/util -I$(srcdir)/cs -I$(srcdir)/streamhtmlparser
libclearsilver_a_CFLAGS = -g -Wall -Wextra -Wno-strict-aliasing -Wno-unused-parameter
libclearsilver_a_SOURCES = \
		cs/csparse.c \
		util/neo_auto.c \
		util/neo_err.c \
		util/neo_files.c \
		util/neo_hash.c \
		util/neo_hdf.c  \
		util/neo_misc.c \
		util/neo_rand.c \
		util/neo_str.c  \
		util/ulist.c \
		util/wildmat.c \
		streamhtmlparser/htmlparser.c \
		streamhtmlparser/statemachine.c \
		streamhtmlparser/jsparser.c \
		ClearSilver.h \
		util/neo_hdf.h \
		streamhtmlparser/htmlparser.h \
		streamhtmlparser/jsparser.h \
		streamhtmlparser/statemachine.h \
		streamhtmlparser/htmlparser_fsm.h \
		streamhtmlparser/jsparser_fsm.h
