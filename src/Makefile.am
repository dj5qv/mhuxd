SUBDIRS = clearsilver
sbin_PROGRAMS = mhuxd 

mhuxd_CFLAGS = -g -Wall -Wno-format-truncation -Wno-strict-aliasing -D_GNU_SOURCE -DEV_NO_THREADS $(FUSE_CFLAGS) $(JANSSON_CFLAGS) \
	$(UDEV_CFLAGS) -DLOGDIR=\"$(LOGDIR)\" -DSTATEDIR=\"$(STATEDIR)\" -DRUNDIR=\"$(RUNDIR)\" \
	-DDATADIR=\"$(datadir)\" -DWEBUIDIR=\"$(WEBUIDIR)\" -DHDFDIR=\"$(HDFDIR)\" -DJSONDIR=\"$(JSONDIR)\" \
	-I$(srcdir)/clearsilver
mhuxd_DEPENDENCIES = clearsilver/libclearsilver.a
mhuxd_LDADD  = -Lclearsilver -lclearsilver $(JANSSON_LIBS)
mhuxd_SOURCES = buffer.c demux.c devmgr.c kcfg.c linux_udev.c logger.c main.c \
		mhcontrol.c mhinfo.c mhrouter.c mux.c net.c opts.c pglist.c tty.c util.c \
		conmgr.c con_vsp.c con_tcp.c linux_termios.c channel.c cfgmgr.c \
		daemon.c http_parser.c http_server.c webui.c restapi.c cfgnod.c version.c \
		http_parse_query.c radiotypes.c wkman.c proc_mcp.c proc_rotator.c mhmk2r.c \
                mhmk2.c mhsm.c citem.c dbuf.c result.c \
		buffer.h demux.h devmgr.h kcfg.h linux_udev.h logger.h main.h \
		mhcontrol.h mhinfo.h mhrouter.h mux.h net.h opts.h pglist.h tty.h util.h \
		conmgr.h con_vsp.h con_tcp.h linux_termios.h channel.h cfgmgr.h \
		daemon.h http_parser.h http_server.h http_codes.h webui.h restapi.h obuf.h cfgnod.h \
		version.h http_parse_query.h radiotypes.h wkman.h mhflags.h proc_mcp.h \
                proc_rotator.h mhmk2r.h mhmk2.h mhsm.h citem.h mhsm_sim.h dbuf.h result.h

install-data-local:
	$(MKDIR_P) $(DESTDIR)$(STATEDIR) $(DESTDIR)$(RUNDIR) $(DESTDIR)$(LOGDIR)

