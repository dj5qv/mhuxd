SUBDIRS = clearsilver
sbin_PROGRAMS = mhuxd 

mhuxd_CFLAGS = -g -Wall -Wno-format-truncation -Wno-strict-aliasing -D_GNU_SOURCE -DEV_NO_THREADS $(FUSE_CFLAGS) $(JANSSON_CFLAGS) \
	$(UDEV_CFLAGS) -DLOGDIR=\"$(LOGDIR)\" -DSTATEDIR=\"$(STATEDIR)\" -DRUNDIR=\"$(RUNDIR)\" \
	-DDATADIR=\"$(datadir)\" -DWEBUIDIR=\"$(WEBUIDIR)\" -DJSONDIR=\"$(JSONDIR)\" \
	-I$(srcdir)/clearsilver
mhuxd_DEPENDENCIES = clearsilver/libclearsilver.a
mhuxd_LDADD  = -Lclearsilver -lclearsilver $(JANSSON_LIBS)
mhuxd_SOURCES = buffer.c demux.c devmgr.c kcfg.c linux_udev.c logger.c main.c \
		mhcontrol.c mhinfo.c mhrouter.c mux.c net.c opts.c pglist.c tty.c util.c \
		conmgr.c con_vsp.c con_tcp.c con_client.c linux_termios.c channel.c cfgmgr.c \
		daemon.c http_parser.c http_server.c restapi.c cfgnod.c version.c \
		http_parse_query.c radiotypes.c wkman.c proc_mcp.c proc_rotator.c mhmk2r.c \
		mhmk2.c mhsm.c citem.c dbuf.c cfgmgrj.c rigctld_client.c rigctld_proc.c ws.c \
		app_ctx.c eventbus.c cat_civ.c cat_ascii.c cat_ybin.c cat_state.c cat_native.c cat_trace.c \
		buffer.h demux.h devmgr.h kcfg.h linux_udev.h logger.h \
		mhcontrol.h mhinfo.h mhrouter.h mux.h net.h opts.h pglist.h tty.h util.h \
		conmgr.h con_vsp.h con_tcp.h con_client.h linux_termios.h channel.h cfgmgr.h \
		daemon.h http_parser.h http_server.h http_codes.h restapi.h obuf.h cfgnod.h \
		version.h http_parse_query.h radiotypes.h wkman.h mhflags.h proc_mcp.h \
	    proc_rotator.h mhmk2r.h mhmk2.h mhsm.h citem.h mhsm_sim.h dbuf.h cfgmgrj.h rigctld_client.h rigctld_proc.h \
	    ws.h http_server_int.h app_ctx.h eventbus.h events.h device.h ptt_byte.h \
	    cat.h cat_civ.h cat_ascii.h cat_ybin.h cat_state.h cat_native.h cat_trace.h

install-data-local:
	$(MKDIR_P) $(DESTDIR)$(STATEDIR) $(DESTDIR)$(LOGDIR) $(DESTDIR)$(RUNDIR)

