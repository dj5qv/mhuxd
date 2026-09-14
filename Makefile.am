SUBDIRS = src webui
dist_man_MANS = man/mhuxd.8
dist_doc_DATA = README.md COPYING AUTHORS
jsondir = $(JSONDIR)
dist_json_DATA = json/mh_rigtypes.json json/mh_displayoptions.json

distclean-local:
	-rm -rf debian/mhuxd.debhelper.log debian/mhuxd debian/mhuxd.substvars debian/files \
	debian/mhuxd.postinst.debhelper debian/mhuxd.postrm.debhelper debian/mhuxd.prerm.debhelper

# Build binary Debian packages with a snapshot version, e.g.
# 0.90~beta+20260914.144712.gff27a37 ("~beta" sorts before the final 0.90).
# The timestamp is the UTC commit time, so builds of the same commit get the
# same version on every platform. If tracked files have uncommitted changes,
# the build time is used instead and ".dirty" is appended, keeping such builds
# unique. debian/rules picks the version up from MHUXD_DEB_VERSION. The .deb
# files land in the parent directory of the source tree, as with plain
# dpkg-buildpackage.
deb:
	@cd $(top_srcdir) || exit 1; \
	upstream=`echo '$(PACKAGE_VERSION)' | sed -E 's/(alpha|beta|rc)/~\1/'`; \
	stamp=`date -u +%Y%m%d.%H%M%S`; suffix=; \
	if rev=`git rev-parse --short HEAD 2>/dev/null`; then \
		suffix=.g$$rev; \
		if git diff --quiet HEAD --; then \
			stamp=`TZ=UTC0 git log -1 --date=format-local:%Y%m%d.%H%M%S --format=%cd`; \
		else \
			suffix=$$suffix.dirty; \
		fi; \
	fi; \
	version=$$upstream+$$stamp$$suffix; \
	echo "Building Debian packages, version $$version"; \
	env -u MAKEFLAGS -u MFLAGS -u MAKELEVEL MHUXD_DEB_VERSION=$$version \
		dpkg-buildpackage -us -uc -b && \
	ls -1 ../*_$${version}_*.deb

.PHONY: deb

