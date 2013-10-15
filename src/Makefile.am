sbin_PROGRAMS = mhuxd 
mhuxd_CFLAGS = -D_GNU_SOURCE -pthread $(FUSE_CFLAGS) $(UDEV_CFLAGS) 
mhuxd_LDADD  = $(FUSE_LIBS) $(UDEV_LIBS)
mhuxd_SOURCES = buffer.c ctl.c demux.c kcfg.c linux_termios.c logger.c mhinfo.c opts.c radiotypes.c util.c cfgfile.c ctlmsg.c dispatcher.c launcher.c linux_udev.c main.c mux.c pglist.c tty.c vsp.c
