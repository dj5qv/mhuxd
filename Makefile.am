SUBDIRS = src webui
dist_man_MANS = man/mhuxd.8
dist_doc_DATA = README.md COPYING AUTHORS
jsondir = $(JSONDIR)
dist_json_DATA = json/mh_rigtypes.json json/mh_displayoptions.json

distclean-local:
	-rm -rf debian/mhuxd.debhelper.log debian/mhuxd debian/mhuxd.substvars debian/files \
	debian/mhuxd.postinst.debhelper debian/mhuxd.postrm.debhelper debian/mhuxd.prerm.debhelper

